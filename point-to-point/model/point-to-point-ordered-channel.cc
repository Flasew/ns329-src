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
 *
 * Author: Weiyang Wang <wew168@ucsd.edu>
 */

#include <iostream>

#include "point-to-point-ordered-channel.h"
#include "point-to-point-net-device.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PointToPointOrderedChannel");

NS_OBJECT_ENSURE_REGISTERED (PointToPointOrderedChannel);

TypeId
PointToPointOrderedChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PointToPointOrderedChannel")
    .SetParent<PointToPointChannel> ()
    .SetGroupName ("PointToPoint")
    .AddConstructor<PointToPointOrderedChannel> ()
  ;
  return tid;
}

PointToPointOrderedChannel::PointToPointOrderedChannel ()
  : PointToPointChannel ()
{
}

PointToPointOrderedChannel::~PointToPointOrderedChannel ()
{
}

bool
PointToPointOrderedChannel::TransmitStart (
  Ptr<const Packet> p,
  Ptr<PointToPointNetDevice> src,
  Time txTime)
{
  NS_LOG_FUNCTION (this << p << src);
  NS_LOG_LOGIC ("UID is " << p->GetUid () << ")");

  m_packetQueue.emplace(p->Copy());

  Simulator::Schedule (m_delay, &PointToPointOrderedChannel::PacketGo, src);

  return true;
}

void
PointToPointOrderedChannel::PacketGo(
  Ptr<const Packet> p, 
  Ptr<PointToPointNetDevice> src) 
{
  NS_LOG_FUNCTION (this << p << src);

  NS_ASSERT (m_link[0].m_state != INITIALIZING);
  NS_ASSERT (m_link[1].m_state != INITIALIZING);
  NS_ASSERT (m_packetQueue.size() != 0);

  uint32_t wire = src == m_link[0].m_src ? 0 : 1;
  Time txTime = src->GetDataRate.CalculateBytesTxTime (p->GetSize ());
  Ptr<Packet> p = m_packetQueue.front();
  m_packetQueue.pop_front();

  Simulator::ScheduleWithContext (m_link[wire].m_dst->GetNode ()->GetId (),
                                  txTime, &PointToPointNetDevice::Receive,
                                  m_link[wire].m_dst, p->Copy ());

  // Call the tx anim callback on the net device
  m_txrxPointToPoint (p, src, m_link[wire].m_dst, txTime, txTime + m_delay);
}

} // namespace ns3
