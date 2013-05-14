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
#include "ns3/node-container.h"
#include "ns3/enum.h"
#include "ns3/names.h"
#include "ns3/mobility-module.h"
#include "contiki-mac-helper.h"
#include "contiki-phy-helper.h"
#include "contiki-channel-helper.h"
#include "contiki-device-helper.h"

NS_LOG_COMPONENT_DEFINE ("ContikiNetDeviceHelper");

namespace ns3 {

ContikiNetDeviceHelper::ContikiNetDeviceHelper ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_deviceFactory.SetTypeId ("ns3::ContikiNetDevice");
}

void 
ContikiNetDeviceHelper::SetAttribute (std::string n1, const AttributeValue &v1)
{
  NS_LOG_FUNCTION (n1 << &v1);
  m_deviceFactory.Set (n1, v1);
}

Ptr<ContikiNetDevice>
ContikiNetDeviceHelper::Install (Ptr<Node> node)
{
  /* Connect node to bridge */
  Ptr<ContikiNetDevice> bridge = m_deviceFactory.Create<ContikiNetDevice> ();
  node->AddDevice (bridge);
  /* Loop bridge to itself */
  bridge->SetBridgedNetDevice (bridge);

  return bridge;
}

Ptr<NetDevice>
ContikiNetDeviceHelper::Install (Ptr<Node> node, Ptr<NetDevice> nd)
{
  Ptr<ContikiNetDevice> bridge = m_deviceFactory.Create<ContikiNetDevice> ();
  node->AddDevice (bridge);
  bridge->SetBridgedNetDevice (nd);

  return bridge;
}

void
ContikiNetDeviceHelper::Install (NodeContainer nodes, std::string mode, std::string apps)
{
  uint32_t nodeCount = nodes.GetN();

  //Seperating contiki applications paths
  std::vector<std::string> apps_list;
  std::stringstream ss(apps);
  std::string app;
  while(std::getline(ss, app, ','))
  {
	  apps_list.push_back(app);
  }

  //Informing Contiki device about the number of nodes.
  ContikiNetDevice::SetNNodes(nodeCount);

  /* Create Pointers */
  Ptr<ContikiNetDevice> bridge [nodeCount + 1];
  Ptr<ContikiMac> mac [nodeCount + 1];
  Ptr<ContikiPhy> phy [nodeCount + 1];

  /* Create Helpers */
  ContikiMacHelper ContikiMacHelper;
  ContikiPhyHelper ContikiPhyHelper;
  ContikiChannelHelper ContikiChannelHelper;

  /* Create Channel */
  Ptr<ContikiChannel> channel = ContikiChannelHelper.Create();
  channel->SetPropagationDelayModel (CreateObject<ConstantSpeedPropagationDelayModel> ());
  Ptr<LogDistancePropagationLossModel> log = CreateObject<LogDistancePropagationLossModel> ();
  channel->SetPropagationLossModel (log);

  /* Create Position of all Nodes */
  Ptr<MobilityModel> pos = CreateObject<ConstantPositionMobilityModel> ();
  pos->SetPosition (Vector (0.0, 0.0, 0.0));

  /* Build Network Stack for all Nodes */
  for (uint8_t i = 0; i < nodeCount; i++)
  {
    bridge[i] =  m_deviceFactory.Create<ContikiNetDevice> ();
    bridge[i]->SetMode(mode);

    if(i < apps_list.size())
    	bridge[i]->SetApplication(apps_list[i]);
    else
    	bridge[i]->SetApplication(apps_list[0]); // XXX Needs better solution here

    /* Add Socket Bridge to Node (Spawns Contiki Process */
    nodes.Get(i)->AddDevice(bridge[i]);
    bridge[i]->SetBridgedNetDevice(bridge[i]); 
    /* Add MAC layer to ContikiNetDevice */
    mac[i] = ContikiMacHelper.Install(bridge[i]);
    /* Add PHY to ContikiNetDevice and ContikiMac */
    phy[i] = ContikiPhyHelper.Install(bridge[i], mac[i], ContikiPhy::DSSS_O_QPSK_GHz);

    /* Add Physical Components (Channel and Position) */
    ContikiChannelHelper.Install(channel, bridge[i]);
    phy[i]->SetMobility(pos);
  }
}


} // namespace ns3
