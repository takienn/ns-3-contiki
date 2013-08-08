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

Ptr<NetDevice>
ContikiNetDeviceHelper::Install (Ptr<Node> node, Ptr<NetDevice> nd,std::string mode, std::string app)
{
  Ptr<ContikiNetDevice> bridge = m_deviceFactory.Create<ContikiNetDevice> ();
  node->AddDevice (bridge);
  bridge->SetBridgedNetDevice (nd);
  bridge->SetApplication(app);

  Simulator::GetImplementation()->TraceConnectWithoutContext("CurrentTs",
  		  MakeCallback(ContikiNetDevice::ContikiClockHandle));

  bridge->SetNNodes(1);
  bridge->SetMode(mode);

  return bridge;
}

void
ContikiNetDeviceHelper::Install (NodeContainer nodes, std::string mode, std::string apps, bool pcaps)
{
	  /* Create Position of all Nodes */
	  Ptr<MobilityModel> pos = CreateObject<ConstantPositionMobilityModel> ();
	  pos->SetPosition (Vector (0.0, 0.0, 0.0));
	  Ptr<PropagationDelayModel> propagationDelay =
			  CreateObject<ConstantSpeedPropagationDelayModel>();

	  Ptr<LogDistancePropagationLossModel> log = CreateObject<LogDistancePropagationLossModel> ();

	  Install (nodes, pos, propagationDelay, log, mode, apps, pcaps);
}

void
ContikiNetDeviceHelper::Install (NodeContainer nodes, Ptr<MobilityModel> pos,
		Ptr<PropagationDelayModel> propagationDelay,
		Ptr<LogDistancePropagationLossModel> propagationLoss,
		std::string mode,
		  std::string apps, bool pcaps)
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
  ContikiMacHelper contikiMacHelper;
  ContikiPhyHelper contikiPhyHelper;
  ContikiChannelHelper contikiChannelHelper;

  /* Create Channel */
  Ptr<ContikiChannel> channel = contikiChannelHelper.Create();
  channel->SetPropagationDelayModel (propagationDelay);
  channel->SetPropagationLossModel (propagationLoss);


  //
  // Registering our Clock trace sink to handle clock synchronization
  //
  Simulator::GetImplementation()->TraceConnectWithoutContext("CurrentTs",
		  MakeCallback(&ContikiNetDevice::ContikiClockHandle));

  /* Build Network Stack for all Nodes */
  for (uint32_t i = 0; i < nodeCount; i++)
  {
    bridge[i] =  m_deviceFactory.Create<ContikiNetDevice> ();
    bridge[i]->SetMode(mode);
    bridge[i]->Start(NanoSeconds(bridge[i]->GetNodeId()));

    if(i < apps_list.size())
    	bridge[i]->SetApplication(apps_list[i]);
    else
    	bridge[i]->SetApplication(apps_list[0]); // XXX Needs better solution here

    /* Add Socket Bridge to Node (Spawns Contiki Process */
    nodes.Get(i)->AddDevice(bridge[i]);
    bridge[i]->SetBridgedNetDevice(bridge[i]); 
    /* Add MAC layer to ContikiNetDevice */
    mac[i] = contikiMacHelper.Install(bridge[i]);
    /* Add PHY to ContikiNetDevice and ContikiMac */
    phy[i] = contikiPhyHelper.Install(bridge[i], mac[i], ContikiPhy::DSSS_O_QPSK_GHz);

    if(pcaps)
    	contikiPhyHelper.EnablePcapAll("contiki-device", true);

    /* Add Physical Components (Channel and Position) */
    contikiChannelHelper.Install(channel, bridge[i]);
    phy[i]->SetMobility(pos);
  }
}


} // namespace ns3
