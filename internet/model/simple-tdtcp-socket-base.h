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

#ifndef SIMPLE_TDTCP_SOCKET_BASE_H
#define SIMPLE_TDTCP_SOCKET_BASE_H

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

class Ipv4EndPoint;
class Ipv6EndPoint;
class Node;
class Packet;
class TcpL4Protocol;
class TcpHeader;
class TcpCongestionOps;
class TcpRecoveryOps;
class RttEstimator;
class TcpRxBuffer;
class TcpTxBuffer;
class TcpOption;
class Ipv4Interface;
class Ipv6Interface;
class TcpSocketBase;
class TcpSocketState;

class SimpleTdTcpSocketBase : public TcpSocketBase {

public:
  /**
   * Get the type ID.
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Get the instance TypeId
   * \return the instance TypeId
   */
  virtual TypeId GetInstanceTypeId () const;

  /**
   * Create an unbound TCP socket
   */
  SimpleTdTcpSocketBase (void);

  /**
   * Clone a TCP socket, for use upon receiving a connection request in LISTEN state
   *
   * \param sock the original Tcp Socket
   */
  SimpleTdTcpSocketBase (const SimpleTdTcpSocketBase& sock);
  virtual ~SimpleTdTcpSocketBase (void);

  virtual void ReceivedAck (Ptr<Packet> packet, const TcpHeader& tcpHeader);
  virtual void SendEmptyPacket (uint8_t flags);
  virtual uint32_t SendDataPacket (SequenceNumber32 seq, uint32_t maxSize, bool withAck);

  void SetNumSubflows (uint16_t nflows);
  uint16_t GetNumSubflows () const;
  void SetCurrSubflow (uint16_t flowId);
  uint16_t GetCurrSubflow () const;

  virtual Ptr<TcpSocketBase> Fork (void);

private:
  uint16_t m_nflows {0};
  uint16_t m_currflow {0};

  vector<Ptr<TcpSocketState>> m_subflows;
  unordered_map<uint32_t, uint16_t> m_packetmap;

  void TcpSockStateSyncSubflow(const Ptr<TcpSocketState> from, Ptr<TcpSocketState> to);
  void TcpSockStateSyncConn(const Ptr<TcpSocketState> from, Ptr<TcpSocketState> to);
};

}

#endif
