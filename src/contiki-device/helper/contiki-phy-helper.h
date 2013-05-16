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

#ifndef CONTIKI_PHY_HELPER_H
#define CONTIKI_PHY_HELPER_H

#include "ns3/net-device-container.h"
#include "ns3/object-factory.h"
#include "ns3/contiki-device.h"
#include "ns3/trace-helper.h"
#include <string>

namespace ns3 {

class Node;
class AttributeValue;

/**
 * \brief build ContikiPhy to allow ns-3 simulations to interact with Linux
 * tap devices and processes on the Linux host.
 */
class ContikiPhyHelper : public PcapHelperForDevice
{
public:
  /**
   * Construct a ContikiPhyHelper to make life easier for people wanting to
   * have their simulations interact with Linux tap devices and processes
   * on the Linux host.
   */
  ContikiPhyHelper ();

  /*
   * Set an attribute in the underlying ContikiPhy net device when these
   * devices are automatically created.
   *
   * \param n1 the name of the attribute to set
   * \param v1 the value of the attribute to set
   */
  void SetAttribute (std::string n1, const AttributeValue &v1);

  /**
   * This method installs a ContikiPhy on the specified Node/MAC layer
   *
   * \param node The Ptr<SocketBridge> to install the ContikiPhy in
   * \param mac The Ptr<SocketNullMac> to install the ContikiPhy in
   * \param mode The mode the PHY will operate under 
   * \returns A pointer to the new ContikiPhy NetDevice.
   */
  Ptr<ContikiPhy> Install (Ptr<ContikiNetDevice> bridge, Ptr<ContikiMac> mac, ContikiPhy::PhyMode mode);

private:
  virtual void EnablePcapInternal (std::string prefix, Ptr<NetDevice> nd, bool promiscuous, bool explicitFilename);
  ObjectFactory m_deviceFactory;
  uint32_t m_pcapDlt;
};

} // namespace ns3


#endif /* CONTIKI_PHY_HELPER_H */
