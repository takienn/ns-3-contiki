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

#ifndef CONTIKI_MAC_HELPER_H
#define CONTIKI_MAC_HELPER_H

#include "ns3/net-device-container.h"
#include "ns3/object-factory.h"
#include "ns3/contiki-device.h"
#include <string>

namespace ns3 {

class Node;
class AttributeValue;

/**
 * \brief build ContikiMac to allow ns-3 simulations to interact with Linux
 * tap devices and processes on the Linux host.
 */
class ContikiMacHelper
{
public:
  /**
   * Construct a ContikiMacHelper to make life easier for people wanting to
   * have their simulations interact with Linux tap devices and processes
   * on the Linux host.
   */
  ContikiMacHelper ();

  /*
   * Set an attribute in the underlying ContikiMac net device when these
   * devices are automatically created.
   *
   * \param n1 the name of the attribute to set
   * \param v1 the value of the attribute to set
   */
  void SetAttribute (std::string n1, const AttributeValue &v1);

  /**
   * This method installs a ContikiMac on the specified ContikiNetDevice
   *
   * \param ContikiNetDevice The Ptr<ContikiNetDevice> to install the ContikiMac in
   * \returns A pointer to the new ContikiMac layer.
   */
  Ptr<ContikiMac> Install (Ptr<ContikiNetDevice> ContikiNetDevice);

private:
  ObjectFactory m_deviceFactory;
};

} // namespace ns3


#endif /* CONTIKI_MAC_HELPER_H */
