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

#ifndef CONTIKI_DEVICE_HELPER_H
#define CONTIKI_DEVICE_HELPER_H

#include "ns3/net-device-container.h"
#include "ns3/object-factory.h"
#include "ns3/contiki-device.h"
#include <string.h>

namespace ns3 {

class Node;
class AttributeValue;

/**
 * \brief build ContikiNetDevice to allow ns-3 simulations to interact with Linux
 * tap devices and processes on the Linux host.
 */
class ContikiNetDeviceHelper
{
public:
  /**
   * Construct a ContikiNetDeviceHelper
   */
  ContikiNetDeviceHelper ();

  /*
   * Set an attribute in the underlying ContikiNetDevice net device when these
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
   * \param node The Ptr<Node> to install the ContikiNetDevice in
   * \param nd The Ptr<NetDevice> to attach to the bridge.
   * \returns A pointer to the new ContikiNetDevice NetDevice.
   */
  Ptr<NetDevice> Install (Ptr<Node> node, Ptr<NetDevice> nd);

  /**
   * This method installs a ContikiNetDevice on the specified Node and a loopback
   * bridge with the created ContikiNetDevice.  The Node is specified using
   * a Ptr<Node>.
   *
   * \param node The Ptr<Node> to install the ContikiNetDevice in
   * \returns A pointer to the new ContikiNetDevice NetDevice.
   */

  Ptr<NetDevice> Install (Ptr<Node> node, Ptr<NetDevice> nd, std::string mode,  std::string app);

  Ptr<ContikiNetDevice> Install (Ptr<Node> node);

  /**
   * This method installs the entire Network Stack to a collection of Nodes.
   * All nodes are located on the same vector co-ordinate and use the same executable for fork/exec.
   *
   * \param node The NodeContainer to install the various ContikiNetDevice objects.
   * \param path The path used to locate the executable for the fork/exec call.
   * \param mode The stack operation mode (MACOVERLAY or PHYOVERLAY; medium emulation or layer 2 and medium emulation).
   */
  void Install (NodeContainer nodes, std::string mode, std::string apps);


private:
  ObjectFactory m_deviceFactory;
};

} // namespace ns3


#endif /* CONTIKI_DEVICE_HELPER_H */
