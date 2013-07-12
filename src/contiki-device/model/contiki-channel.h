/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2006,2007 INRIA
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
 * Author: Mathieu Lacage, <mathieu.lacage@sophia.inria.fr>
 */
#ifndef CONTIKI_CHANNEL_H
#define CONTIKI_CHANNEL_H

#include "ns3/packet.h"
#include "ns3/channel.h"
#include "ns3/simulator.h"
#include "ns3/mobility-model.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/object-factory.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"

#include <vector>
#include <stdint.h>

#include "contiki-phy.h"

namespace ns3 {

class NetDevice;
class PropagationLossModel;
class PropagationDelayModel;
class ContikiPhy;

/**
 * \brief A Yans wifi channel
 * \ingroup wifi
 *
 * This wifi channel implements the propagation model described in
 * "Yet Another Network Simulator", (http://cutebugs.net/files/wns2-yans.pdf).
 *
 * This class is expected to be used in tandem with the ns3::YansWifiPhy
 * class and contains a ns3::PropagationLossModel and a ns3::PropagationDelayModel.
 * By default, no propagation models are set so, it is the caller's responsability
 * to set them before using the channel.
 */
class ContikiChannel : public Channel
{
public:
  static TypeId GetTypeId (void);

  ContikiChannel ();
  virtual ~ContikiChannel ();

  // inherited from Channel.
  virtual uint32_t GetNDevices (void) const;
  virtual Ptr<NetDevice> GetDevice (uint32_t i) const;

  void Add (Ptr<ContikiPhy> phy);

  /**
   * \param loss the new propagation loss model.
   */
  void SetPropagationLossModel (Ptr<PropagationLossModel> loss);
  /**
   * \param delay the new propagation delay model.
   */
  void SetPropagationDelayModel (Ptr<PropagationDelayModel> delay);

  /**
   * \param sender the device from which the packet is originating.
   * \param packet the packet to send
   * \param txPowerDbm the transmit power
   *
   * This method should not be invoked by normal users. It is
   * currently invoked only from Phy::Send. 
   */
  void Send (Ptr<ContikiPhy> sender, Ptr<const Packet> packet, double txPowerDbm);

private:
  ContikiChannel& operator = (const ContikiChannel&);
  ContikiChannel (const ContikiChannel &);

  typedef std::vector<Ptr<ContikiPhy> > PhyList;
  void Receive (uint32_t i, Ptr<Packet> packet, double rxPowerDbm) const;

  PhyList m_phyList;
  Ptr<PropagationLossModel> m_loss;
  Ptr<PropagationDelayModel> m_delay;
};

} // namespace ns3

#endif /* CONTIKI_CHANNEL_H */
