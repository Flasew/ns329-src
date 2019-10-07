/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 Weiyang Wang
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Weiyang Wang <wew168@ucsd.edu>
 */

#include "simple-tdtcp-socket-base.h"
#include <stdint.h>
#include <vector>
#include <queue>
#include <list>
#include <set>
#include <unordered_map>
#include "ns3/object.h"
#include "ns3/uinteger.h"
#include "ns3/traced-value.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/sequence-number.h"
#include "ns3/rtt-estimator.h"
#include "ns3/event-id.h"
#include "ns3/packet.h"
#include "ns3/tcp-socket.h"
#include "ns3/ipv4-end-point.h"
#include "ns3/ipv4-address.h"
#include "ns3/abort.h"
#include "ns3/node.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/ipv4.h"
#include "ns3/ipv6.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv6-route.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv6-routing-protocol.h"
#include "ns3/simulation-singleton.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/pointer.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/data-rate.h"
#include "ns3/object.h"
#include "tcp-socket-base.h"
#include "tcp-l4-protocol.h"
#include "ipv4-end-point.h"
#include "ipv6-end-point.h"
#include "ipv6-l3-protocol.h"
#include "tcp-tx-buffer.h"
#include "tcp-rx-buffer.h"
#include "rtt-estimator.h"
#include "tcp-header.h"
#include "tcp-option-winscale.h"
#include "tcp-option-ts.h"
#include "tcp-option-sack-permitted.h"
#include "tcp-option-sack.h"
#include "tcp-congestion-ops.h"
#include "tcp-recovery-ops.h"
#include "tcp-socket-state.h"
#include "tcp-socket-base.h"

using namespace std;

namespace ns3 {

  NS_LOG_COMPONENT_DEFINE ("SimpleTdTcpSocketBase");

  NS_OBJECT_ENSURE_REGISTERED (SimpleTdTcpSocketBase);

  TypeId SimpleTdTcpSocketBase::GetTypeId (void) {
    static TypeId tid = TypeId ("ns3::SimpleTdTcpSocketBase")
      .SetParent<TcpSocketBase> ()
      .SetGroupName ("Internet")
      .AddConstructor<SimpleTdTcpSocketBase> ()
      .AddAttribute ("NumSubflows",
          "Number of subflows.",
          UintegerValue(0),
          MakeUintegerAccessor (&SimpleTdTcpSocketBase::SetNumSubflows,
            &SimpleTdTcpSocketBase::GetNumSubflows),
          MakeUintegerChecker<uint16_t> ());
    /*
       .AddAttribute ("CurrSubflow",
       "Current subflow.",
       UintegerValue(0),
       MakeUintegerAccessor (&SimpleTdTcpSocketBase::SetCurrSubflow,
       &SimpleTdTcpSocketBase::GetCurrSubflow),
       MakeUintegerChecker<uint16_t> ());
       */
    return tid;
  }
  Ptr<TcpSocketBase>
    SimpleTdTcpSocketBase::Fork (void)
    {
      return CopyObject<SimpleTdTcpSocketBase> (this);
    }

  /**
   * \brief Get the instance TypeId
   * \return the instance TypeId
   */
  TypeId SimpleTdTcpSocketBase::GetInstanceTypeId () const {
    return SimpleTdTcpSocketBase::GetTypeId ();
  }

  /**
   * Create an unbound TCP socket
   */
  SimpleTdTcpSocketBase::SimpleTdTcpSocketBase (void) : TcpSocketBase() {
    m_subflows = vector<Ptr<TcpSocketState>> ();
    m_packetmap = unordered_map<uint32_t, uint16_t> ();
  }

  /**
   * Clone a TCP socket, for use upon receiving a connection request in LISTEN state
   *
   * \param sock the original Tcp Socket
   */
  SimpleTdTcpSocketBase::SimpleTdTcpSocketBase (const SimpleTdTcpSocketBase& sock) : 
    TcpSocketBase (sock),
    m_nflows (sock.m_nflows),
    m_currflow (sock.m_currflow),
    m_subflows (sock.m_subflows),
    m_packetmap (sock.m_packetmap) { }

  SimpleTdTcpSocketBase::~SimpleTdTcpSocketBase (void) { }

  void SimpleTdTcpSocketBase::ReceivedAck (Ptr<Packet> packet, const TcpHeader& tcpHeader) {
    NS_LOG_FUNCTION (this << tcpHeader);

    NS_ASSERT (0 != (tcpHeader.GetFlags () & TcpHeader::ACK));
    NS_ASSERT (m_tcb->m_segmentSize > 0);

    // RFC 6675, Section 5, 1st paragraph:
    // Upon the receipt of any ACK containing SACK information, the
    // scoreboard MUST be updated via the Update () routine (done in ReadOptions)
    bool scoreboardUpdated = false;
    ReadOptions (tcpHeader, scoreboardUpdated);

    SequenceNumber32 ackNumber = tcpHeader.GetAckNumber ();
    SequenceNumber32 oldHeadSequence = m_txBuffer->HeadSequence ();
    m_txBuffer->DiscardUpTo (ackNumber);

    uint16_t sendFlow = 0;
    if (m_subflows.size() > 1) {
      auto it = m_packetmap.find(ackNumber.GetValue());
      if (it == m_packetmap.end()) {
        sendFlow = m_currflow;
      }
      else {
        sendFlow = it->second;
        m_packetmap.erase(it);
      }

      TcpSockStateSyncSubflow(m_tcb, m_subflows[m_currflow]);
      TcpSockStateSyncSubflow(m_subflows[sendFlow], m_tcb);
    }

    if (ackNumber > oldHeadSequence && (m_tcb->m_ecnState != TcpSocketState::ECN_DISABLED) && (tcpHeader.GetFlags () & TcpHeader::ECE))
    {
      if (m_ecnEchoSeq < ackNumber)
      {
        NS_LOG_INFO ("Received ECN Echo is valid");
        m_ecnEchoSeq = ackNumber;
        NS_LOG_DEBUG (TcpSocketState::EcnStateName[m_tcb->m_ecnState] << " -> ECN_ECE_RCVD");
        m_tcb->m_ecnState = TcpSocketState::ECN_ECE_RCVD;
      }
    }

    // Update bytes in flight before processing the ACK for proper calculation of congestion window
    NS_LOG_INFO ("Update bytes in flight before processing the ACK.");
    BytesInFlight ();

    // RFC 6675 Section 5: 2nd, 3rd paragraph and point (A), (B) implementation
    // are inside the function ProcessAck
    ProcessAck (ackNumber, scoreboardUpdated, oldHeadSequence);

    if (m_subflows.size() > 1) {
      TcpSockStateSyncSubflow(m_tcb, m_subflows[sendFlow]);
      TcpSockStateSyncSubflow(m_subflows[m_currflow], m_tcb);
    }

    // If there is any data piggybacked, store it into m_rxBuffer
    if (packet->GetSize () > 0)
    {
      ReceivedData (packet, tcpHeader);
    }

    // RFC 6675, Section 5, point (C), try to send more data. NB: (C) is implemented
    // inside SendPendingData
    SendPendingData (m_connected);
  }

  void SimpleTdTcpSocketBase::SendEmptyPacket (uint8_t flags) {
    NS_LOG_FUNCTION (this << static_cast<uint32_t> (flags));

    if (m_endPoint == nullptr && m_endPoint6 == nullptr)
    {
      NS_LOG_WARN ("Failed to send empty packet due to null endpoint");
      return;
    }

    Ptr<Packet> p = Create<Packet> ();
    TcpHeader header;
    SequenceNumber32 s = m_tcb->m_nextTxSequence;

    if (flags & TcpHeader::FIN)
    {
      flags |= TcpHeader::ACK;
    }
    else if (m_state == FIN_WAIT_1 || m_state == LAST_ACK || m_state == CLOSING)
    {
      ++s;
    }

    AddSocketTags (p);

    header.SetFlags (flags);
    header.SetSequenceNumber (s);
    uint32_t ackn = s.GetValue();
    if ((flags & TcpHeader::SYN) || (flags & TcpHeader::FIN))
      ackn++;

    if (m_subflows.size() > 1) {
      m_packetmap[ackn] = m_currflow;
    }

    header.SetAckNumber (m_rxBuffer->NextRxSequence ());
    if (m_endPoint != nullptr)
    {
      header.SetSourcePort (m_endPoint->GetLocalPort ());
      header.SetDestinationPort (m_endPoint->GetPeerPort ());
    }
    else
    {
      header.SetSourcePort (m_endPoint6->GetLocalPort ());
      header.SetDestinationPort (m_endPoint6->GetPeerPort ());
    }
    AddOptions (header);

    // RFC 6298, clause 2.4
    m_rto = Max (m_rtt->GetEstimate () + Max (m_clockGranularity, m_rtt->GetVariation () * 4), m_minRto);

    uint16_t windowSize = AdvertisedWindowSize ();
    bool hasSyn = flags & TcpHeader::SYN;
    bool hasFin = flags & TcpHeader::FIN;
    bool isAck = flags == TcpHeader::ACK;
    if (hasSyn)
    {
      if (m_winScalingEnabled)
      { // The window scaling option is set only on SYN packets
        AddOptionWScale (header);
      }

      if (m_sackEnabled)
      {
        AddOptionSackPermitted (header);
      }

      if (m_synCount == 0)
      { // No more connection retries, give up
        NS_LOG_LOGIC ("Connection failed.");
        m_rtt->Reset (); //According to recommendation -> RFC 6298
        CloseAndNotify ();
        return;
      }
      else
      { // Exponential backoff of connection time out
        int backoffCount = 0x1 << (m_synRetries - m_synCount);
        m_rto = m_cnTimeout * backoffCount;
        m_synCount--;
      }

      if (m_synRetries - 1 == m_synCount)
      {
        UpdateRttHistory (s, 0, false);
      }
      else
      { // This is SYN retransmission
        UpdateRttHistory (s, 0, true);
      }

      windowSize = AdvertisedWindowSize (false);
    }
    header.SetWindowSize (windowSize);

    if (flags & TcpHeader::ACK)
    { // If sending an ACK, cancel the delay ACK as well
      m_delAckEvent.Cancel ();
      m_delAckCount = 0;
      if (m_highTxAck < header.GetAckNumber ())
      {
        m_highTxAck = header.GetAckNumber ();
      }
      if (m_sackEnabled && m_rxBuffer->GetSackListSize () > 0)
      {
        AddOptionSack (header);
      }
      NS_LOG_INFO ("Sending a pure ACK, acking seq " << m_rxBuffer->NextRxSequence ());
    }

    m_txTrace (p, header, this);

    if (m_endPoint != nullptr)
    {
      m_tcp->SendPacket (p, header, m_endPoint->GetLocalAddress (),
          m_endPoint->GetPeerAddress (), m_boundnetdevice);
    }
    else
    {
      m_tcp->SendPacket (p, header, m_endPoint6->GetLocalAddress (),
          m_endPoint6->GetPeerAddress (), m_boundnetdevice);
    }


    if (m_retxEvent.IsExpired () && (hasSyn || hasFin) && !isAck )
    { // Retransmit SYN / SYN+ACK / FIN / FIN+ACK to guard against lost
      NS_LOG_LOGIC ("Schedule retransmission timeout at time "
          << Simulator::Now ().GetSeconds () << " to expire at time "
          << (Simulator::Now () + m_rto.Get ()).GetSeconds ());
      m_retxEvent = Simulator::Schedule (m_rto, &SimpleTdTcpSocketBase::SendEmptyPacket, this, flags);
    }
  }

  uint32_t SimpleTdTcpSocketBase::SendDataPacket (SequenceNumber32 seq, uint32_t maxSize, bool withAck) {
    NS_LOG_FUNCTION (this << seq << maxSize << withAck);

    bool isRetransmission = false;
    if (seq != m_tcb->m_highTxMark)
    {
      isRetransmission = true;
    }

    Ptr<Packet> p = m_txBuffer->CopyFromSequence (maxSize, seq);
    uint32_t sz = p->GetSize (); // Size of packet
    uint8_t flags = withAck ? TcpHeader::ACK : 0;
    uint32_t remainingData = m_txBuffer->SizeFromSequence (seq + SequenceNumber32 (sz));

    if (m_tcb->m_pacing)
    {
      NS_LOG_INFO ("Pacing is enabled");
      if (m_pacingTimer.IsExpired ())
      {
        NS_LOG_DEBUG ("Current Pacing Rate " << m_tcb->m_currentPacingRate);
        NS_LOG_DEBUG ("Timer is in expired state, activate it " << m_tcb->m_currentPacingRate.CalculateBytesTxTime (sz));
        m_pacingTimer.Schedule (m_tcb->m_currentPacingRate.CalculateBytesTxTime (sz));
      }
      else
      {
        NS_LOG_INFO ("Timer is already in running state");
      }
    }

    if (withAck)
    {
      m_delAckEvent.Cancel ();
      m_delAckCount = 0;
    }

    // Sender should reduce the Congestion Window as a response to receiver's ECN Echo notification only once per window
    if (m_tcb->m_ecnState == TcpSocketState::ECN_ECE_RCVD && m_ecnEchoSeq.Get() > m_ecnCWRSeq.Get () && !isRetransmission)
    {
      NS_LOG_INFO ("Backoff mechanism by reducing CWND  by half because we've received ECN Echo");
      m_tcb->m_cWnd = std::max (m_tcb->m_cWnd.Get () / 2, m_tcb->m_segmentSize);
      m_tcb->m_ssThresh = m_tcb->m_cWnd;
      m_tcb->m_cWndInfl = m_tcb->m_cWnd;
      flags |= TcpHeader::CWR;
      m_ecnCWRSeq = seq;
      NS_LOG_DEBUG (TcpSocketState::EcnStateName[m_tcb->m_ecnState] << " -> ECN_CWR_SENT");
      m_tcb->m_ecnState = TcpSocketState::ECN_CWR_SENT;
      NS_LOG_INFO ("CWR flags set");
      NS_LOG_DEBUG (TcpSocketState::TcpCongStateName[m_tcb->m_congState] << " -> CA_CWR");
      if (m_tcb->m_congState == TcpSocketState::CA_OPEN)
      {
        m_congestionControl->CongestionStateSet (m_tcb, TcpSocketState::CA_CWR);
        m_tcb->m_congState = TcpSocketState::CA_CWR;
      }
    }

    AddSocketTags (p);

    if (m_closeOnEmpty && (remainingData == 0))
    {
      flags |= TcpHeader::FIN;
      if (m_state == ESTABLISHED)
      { // On active close: I am the first one to send FIN
        NS_LOG_DEBUG ("ESTABLISHED -> FIN_WAIT_1");
        m_state = FIN_WAIT_1;
      }
      else if (m_state == CLOSE_WAIT)
      { // On passive close: Peer sent me FIN already
        NS_LOG_DEBUG ("CLOSE_WAIT -> LAST_ACK");
        m_state = LAST_ACK;
      }
    }
    TcpHeader header;
    header.SetFlags (flags);
    header.SetSequenceNumber (seq);

    if (m_subflows.size() > 1) {
      m_packetmap[seq.GetValue() + sz] = m_currflow;
    }
    header.SetAckNumber (m_rxBuffer->NextRxSequence ());
    if (m_endPoint)
    {
      header.SetSourcePort (m_endPoint->GetLocalPort ());
      header.SetDestinationPort (m_endPoint->GetPeerPort ());
    }
    else
    {
      header.SetSourcePort (m_endPoint6->GetLocalPort ());
      header.SetDestinationPort (m_endPoint6->GetPeerPort ());
    }
    header.SetWindowSize (AdvertisedWindowSize ());
    AddOptions (header);

    if (m_retxEvent.IsExpired ())
    {
      // Schedules retransmit timeout. m_rto should be already doubled.

      NS_LOG_LOGIC (this << " SendDataPacket Schedule ReTxTimeout at time " <<
          Simulator::Now ().GetSeconds () << " to expire at time " <<
          (Simulator::Now () + m_rto.Get ()).GetSeconds () );
      m_retxEvent = Simulator::Schedule (m_rto, &SimpleTdTcpSocketBase::ReTxTimeout, this);
    }

    m_txTrace (p, header, this);

    if (m_endPoint)
    {
      m_tcp->SendPacket (p, header, m_endPoint->GetLocalAddress (),
          m_endPoint->GetPeerAddress (), m_boundnetdevice);
      NS_LOG_DEBUG ("Send segment of size " << sz << " with remaining data " <<
          remainingData << " via TcpL4Protocol to " <<  m_endPoint->GetPeerAddress () <<
          ". Header " << header);
    }
    else
    {
      m_tcp->SendPacket (p, header, m_endPoint6->GetLocalAddress (),
          m_endPoint6->GetPeerAddress (), m_boundnetdevice);
      NS_LOG_DEBUG ("Send segment of size " << sz << " with remaining data " <<
          remainingData << " via TcpL4Protocol to " <<  m_endPoint6->GetPeerAddress () <<
          ". Header " << header);
    }

    UpdateRttHistory (seq, sz, isRetransmission);

    // Update bytes sent during recovery phase
    if(m_tcb->m_congState == TcpSocketState::CA_RECOVERY)
    {
      m_recoveryOps->UpdateBytesSent (sz);
    }

    // Notify the application of the data being sent unless this is a retransmit
    if (seq + sz > m_tcb->m_highTxMark)
    {
      Simulator::ScheduleNow (&SimpleTdTcpSocketBase::NotifyDataSent, this,
          (seq + sz - m_tcb->m_highTxMark.Get ()));
    }
    // Update highTxMark
    m_tcb->m_highTxMark = std::max (seq + sz, m_tcb->m_highTxMark.Get ());
    return sz;
  }


  void SimpleTdTcpSocketBase::SetNumSubflows (uint16_t nflows) {
    m_nflows = nflows;
    m_subflows = vector<Ptr<TcpSocketState>> ();
    for (int i = 0; i < nflows; i++) {
      m_subflows.push_back(CreateObject<TcpSocketState>());
    }
  }

  uint16_t SimpleTdTcpSocketBase::GetNumSubflows () const {
    return m_nflows;
  }

  void SimpleTdTcpSocketBase::SetCurrSubflow (uint16_t flowId) {
    TcpSockStateSyncSubflow(m_tcb, m_subflows[m_currflow]);
    TcpSockStateSyncSubflow(m_subflows[flowId], m_tcb);
    m_currflow = flowId;
  }

  uint16_t SimpleTdTcpSocketBase::GetCurrSubflow () const {
    return m_currflow;
  }

  void SimpleTdTcpSocketBase::TcpSockStateSyncSubflow(const Ptr<TcpSocketState> from, Ptr<TcpSocketState> to) {
    to->m_cWnd = from->m_cWnd;
    to->m_cWndInfl = from->m_cWndInfl;
    to->m_ssThresh = from->m_ssThresh;
    to->m_minRtt = from->m_minRtt;
    to->m_lastRtt = from->m_lastRtt;
    to->m_pacing = from->m_pacing;
    to->m_maxPacingRate = from->m_maxPacingRate;
    to->m_currentPacingRate = from->m_currentPacingRate;
  }

  void SimpleTdTcpSocketBase::TcpSockStateSyncConn(const Ptr<TcpSocketState> from, Ptr<TcpSocketState> to) {
    to->m_initialCWnd = from->m_initialCWnd;
    to->m_initialSsThresh = from->m_initialSsThresh;
    to->m_lastAckedSeq = from->m_lastAckedSeq;
    to->m_congState = from->m_congState;
    to->m_ecnState = from->m_ecnState;
    to->m_highTxMark = from->m_highTxMark;
    to->m_nextTxSequence = from->m_nextTxSequence;
    to->m_rcvTimestampValue = from->m_rcvTimestampValue;
    to->m_rcvTimestampEchoReply = from->m_rcvTimestampEchoReply;
    to->m_bytesInFlight = from->m_bytesInFlight;
  }

}
