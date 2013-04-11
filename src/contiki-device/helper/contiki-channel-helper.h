/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 University of Washington
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

#ifndef CONTIKI_CHANNEL_HELPER_H
#define CONTIKI_CHANNEL_HELPER_H

#include "ns3/net-device-container.h"
#include "ns3/object-factory.h"
#include "ns3/contiki-device.h"
#include <string>

namespace ns3 {

class Node;
class AttributeValue;

/**
 * \brief build SocketBridge to allow ns-3 simulations to interact with Linux 
 * tap devices and processes on the Linux host.
 */
class ContikiChannelHelper
{
public:
  /**
   * Construct a ContikiChannelHelper to make life easier for people wanting to
   * have their simulations interact with Linux tap devices and processes
   * on the Linux host.
   */
  ContikiChannelHelper ();

  /*
   * Set an attribute in the underlying SocketBridge net device when these
   * devices are automatically created.
   *
   * \param n1 the name of the attribute to set
   * \param v1 the value of the attribute to set
   */
  void SetAttribute (std::string n1, const AttributeValue &v1);

  /**
   * This method installs a ContikiNetDevice on the specified Node and forms the
   * bridge with the NetDevice specified.  The Node is specified using
   * a Ptr<Node> and the NetDevice is specified using a Ptr<NetDevice>
   *
   * \param node The Ptr<Node> to install the SocketBridge in
   * \param nd The Ptr<NetDevice> to attach to the bridge.
   * \returns A pointer to the new SocketBridge NetDevice.
   */
  void Install (Ptr<Node> node, Ptr<NetDevice> nd);

  /**
   * This method creates a new channel. 
   *
   * \returns A pointer to the new ContikiChannel
   */
  Ptr<ContikiChannel> Create ();


  /**
   * This method creates a new channel and connects the specified SocketBridge 
   * to the channel.
   *
   * \param bridge The Ptr<ContikiNetDevice> to install to the new channel
   * \returns A pointer to the new ContikiChannel
   */
  Ptr<ContikiChannel> Create (Ptr<ContikiNetDevice> bridge);

  /**
   * This method attaches an existing ContikiNetDevice to an existing channel.
   *
   * \param channel The Ptr<ContikiChannel> that the bridge will be connected to
   * \param bridge The Ptr<ContikiNetDevice> to install to the new channel
   */
  void Install (Ptr<ContikiChannel> channel, Ptr<ContikiNetDevice> bridge);


private:
  ObjectFactory m_deviceFactory;
};

} // namespace ns3


#endif
