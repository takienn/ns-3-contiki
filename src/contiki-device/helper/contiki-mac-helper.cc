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

#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/enum.h"
#include "ns3/names.h"
#include "contiki-mac-helper.h"

NS_LOG_COMPONENT_DEFINE ("ContikiMacHelper");

namespace ns3 {

ContikiMacHelper::ContikiMacHelper ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_deviceFactory.SetTypeId ("ns3::ContikiMac");
}

void 
ContikiMacHelper::SetAttribute (std::string n1, const AttributeValue &v1)
{
  NS_LOG_FUNCTION (n1 << &v1);
  m_deviceFactory.Set (n1, v1);
}

Ptr<ContikiMac>
ContikiMacHelper::Install (Ptr<ContikiNetDevice> ContikiNetDevice)
{
  /* Create NullMac and connect to ContikiNetDevice */
  Ptr<ContikiMac> mac = m_deviceFactory.Create<ContikiMac> ();
  ContikiNetDevice->SetMac (mac);
  /* Connect MAC to ContikiNetDevice for Rx from channel */
  mac->SetBridge(ContikiNetDevice);

  return mac;
}

} // namespace ns3
