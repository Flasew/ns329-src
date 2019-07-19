/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2008 University of Washington
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
 */

#include <iostream>

#include "point-to-point-delayed-net-device.h"

#include "ns3/log.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include "ns3/mac48-address.h"
#include "ns3/llc-snap-header.h"
#include "ns3/error-model.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"
#include "ns3/pointer.h"
#include "ns3/net-device-queue-interface.h"
#include "point-to-point-net-device.h"
#include "point-to-point-channel.h"
#include "ppp-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PointToPointDelayedNetDevice");

NS_OBJECT_ENSURE_REGISTERED (PointToPointDelayedNetDevice);

TypeId 
PointToPointDelayedNetDevice::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PointToPointDelayedNetDevice")
    .SetParent<PointToPointNetDevice> ()
    .SetGroupName ("PointToPoint")
    .AddConstructor<PointToPointDelayedNetDevice> ()
    .AddAttribute ("Delay", "Delay added on this network device",
                   TimeValue (Seconds (0.0)),
                   MakeTimeAccessor (&PointToPointDelayedNetDevice::m_delay),
                   MakeTimeChecker ())
  ;
  return tid;
}


PointToPointDelayedNetDevice::PointToPointDelayedNetDevice ()
  : PointToPointNetDevice ()
{
  m_delay = Time(Seconds(0));
}

PointToPointDelayedNetDevice::~PointToPointDelayedNetDevice ()
{
}

void 
PointToPointDelayedNetDevice::SetDelay (Time delay) 
{
  NS_LOG_FUNCTION (this << delay);
  m_delay = delay;
}

Time 
PointToPointDelayedNetDevice::GetDelay ()
{
  NS_LOG_FUNCTION (this);
  return m_delay;
}

bool 
PointToPointDelayedNetDevice::Send (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber) 
{
  NS_LOG_FUNCTION (this << packet << dest << protocolNumber);
  NS_LOG_LOGIC ("p=" << packet << ", dest=" << &dest);
  NS_LOG_LOGIC ("UID is " << packet->GetUid ());

  //
  // If IsLinkUp() is false it means there is no channel to send any packet 
  // over so we just hit the drop trace on the packet and return an error.
  //
  if (IsLinkUp () == false)
    {
      m_macTxDropTrace (packet);
      return false;
    }

  //
  // Stick a point to point protocol header on the packet in preparation for
  // shoving it out the door.
  //
  AddHeader (packet, protocolNumber);

  m_macTxTrace (packet);

  //
  // We should enqueue and dequeue the packet to hit the tracing hooks.
  //
  if (m_queue->Enqueue (packet))
    {
      //
      // If the channel is ready for transition we send the packet right now
      // 
      if (m_txMachineState == READY)
        {
          Simulator::Schedule (m_delay, &PointToPointDelayedNetDevice::DoTransmit, this);
        }
      return true;
    }

  // Enqueue may fail (overflow)

  m_macTxDropTrace (packet);
  return false;
}

void 
PointToPointDelayedNetDevice::TransmitComplete () 
{
  NS_LOG_FUNCTION (this);

  NS_ASSERT_MSG (m_txMachineState == BUSY, "Must be BUSY if transmitting");
  m_txMachineState = READY;

  NS_ASSERT_MSG (m_currentPkt != 0, "PointToPointDelayedNetDevice::TransmitComplete(): m_currentPkt zero");

  m_phyTxEndTrace (m_currentPkt);
  m_currentPkt = 0;
  
  Simulator::Schedule (m_delay, &PointToPointDelayedNetDevice::DoTransmit, this);
}

void 
PointToPointDelayedNetDevice::DoTransmit () 
{
  if (m_txMachineState == BUSY) return;
  Ptr<Packet> packet = m_queue->Dequeue ();
  if (packet == 0)
    {
      NS_LOG_LOGIC ("No pending packets in device queue");
      return;
    }
  m_snifferTrace (packet);
  m_promiscSnifferTrace (packet);
  TransmitStart (packet);
}

}
