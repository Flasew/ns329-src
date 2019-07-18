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

#ifndef POINT_TO_POINT_DELAYED_NET_DEVICE_H
#define POINT_TO_POINT_DELAYED_NET_DEVICE_H


#include "point-to-point-net-device.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/net-device.h"
#include "ns3/callback.h"
#include "ns3/packet.h"
#include "ns3/traced-callback.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/ptr.h"
#include "ns3/mac48-address.h"

namespace ns3 {

class PointToPointDelayedNetDevice : public PointToPointNetDevice
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /**
   * Construct a PointToPointNetDevice
   *
   * This is the constructor for the PointToPointNetDevice.  It takes as a
   * parameter a pointer to the Node to which this device is connected, 
   * as well as an optional DataRate object.
   */
  PointToPointDelayedNetDevice ();

  /**
   * Destroy a PointToPointDelayedNetDevice
   *
   * This is the destructor for the PointToPointDelayedNetDevice.
   */
  virtual ~PointToPointDelayedNetDevice ();

  void SetDelay (Time delay);
  Time GetDelay (void);

  virtual bool Send (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber);

protected:

  void TransmitComplete (void);

  void DoTransmit (void);

  Time m_delay;

};

} // namespace ns3

#endif /* POINT_TO_POINT_NET_DEVICE_H */
