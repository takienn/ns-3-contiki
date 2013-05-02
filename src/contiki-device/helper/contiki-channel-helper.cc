/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/** Copyright (c) 2009 University of Washington
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
#include "contiki-channel-helper.h"

NS_LOG_COMPONENT_DEFINE ("ContikiChannelHelper");

namespace ns3 {

ContikiChannelHelper::ContikiChannelHelper ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_deviceFactory.SetTypeId ("ns3::ContikiChannel");
}

void 
ContikiChannelHelper::SetAttribute (std::string n1, const AttributeValue &v1)
{
  NS_LOG_FUNCTION (n1 << &v1);
  m_deviceFactory.Set (n1, v1);
}

Ptr<ContikiChannel>
ContikiChannelHelper::Create ()
{
  Ptr<ContikiChannel> channel = m_deviceFactory.Create<ContikiChannel> ();
  return channel;
}

Ptr<ContikiChannel>
ContikiChannelHelper::Create (Ptr<ContikiNetDevice> ContikiNetDevice)
{
  Ptr<ContikiChannel> channel = m_deviceFactory.Create<ContikiChannel> ();
  ContikiNetDevice->GetPhy()->SetChannel(channel);
  return channel;
}

void
ContikiChannelHelper::Install (Ptr<ContikiChannel> channel, Ptr<ContikiNetDevice> ContikiNetDevice)
{
  ContikiNetDevice->GetPhy()->SetChannel(channel);
}


void
ContikiChannelHelper::Install (Ptr<Node> node, Ptr<NetDevice> nd)
{
  Ptr<ContikiChannel> channel = m_deviceFactory.Create<ContikiChannel> ();
}


} // namespace ns3
