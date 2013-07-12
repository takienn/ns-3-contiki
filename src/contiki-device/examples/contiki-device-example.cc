/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include <stdio.h>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
//#include "ns3/wifi-module.h"
#include "ns3/contiki-device.h"
#include "ns3/contiki-device-helper.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SocketBridgeExample");

int 
main (int argc, char *argv[])
{
  GlobalValue::Bind ("SimulatorImplementationType", StringValue ("ns3::RealtimeSimulatorImpl"));
  GlobalValue::Bind ("ChecksumEnabled", BooleanValue (true));

  /* Create 2 nodes */
  NodeContainer nodes;
  nodes.Create(100);

  /* Bridge nodes to Contiki processes */ 
  ContikiNetDeviceHelper contikiDeviceHelper;
  contikiDeviceHelper.Install(nodes, "PHYOVERLAY");

  Simulator::Stop (Seconds (1000));
  Simulator::Run ();
  Simulator::Destroy ();
  
  return 0;
}
