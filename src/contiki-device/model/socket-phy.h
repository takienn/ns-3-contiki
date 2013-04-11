/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2005,2006 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#ifndef SOCKET_PHY_H
#define SOCKET_PHY_H

#include "ns3/callback.h"
#include "ns3/event-id.h"
#include "ns3/packet.h"
#include "ns3/object.h"
#include "ns3/traced-callback.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/random-variable.h"
#include "ns3/channel.h"
#include "ns3/simulator.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/enum.h"
#include "ns3/pointer.h"
#include "ns3/net-device.h"
#include "ns3/trace-source-accessor.h"

#include <math.h>
#include <stdint.h>

namespace ns3 {

class ContikiChannel;

class SocketPhy : public Object
{
public:
  static TypeId GetTypeId (void);

  virtual void StartReceivePacket (Ptr<Packet> packet, double rxPowerDbm) = 0;
  virtual void SetDevice (Ptr<Object> device) = 0;
  virtual void SetMobility (Ptr<Object> mobility) = 0;
  virtual Ptr<Object> GetDevice (void) const = 0;
  virtual Ptr<Object> GetMobility (void) = 0;
  virtual void SendPacket (Ptr<const Packet> packet) = 0;

  virtual void SetChannel (Ptr<ContikiChannel> channel) = 0;
  virtual Ptr<ContikiChannel> GetChannel (void) const = 0;

protected:
  virtual void DoDispose (void) = 0;
  virtual void EndReceive (Ptr<Packet> packet) = 0;

private:
  Ptr<Object> m_device;
  Ptr<Object> m_mobility;

  EventId m_endRxEvent;
  UniformVariable m_random;
};

} // namespace ns3


#endif /* SOCKET_PHY_H */
