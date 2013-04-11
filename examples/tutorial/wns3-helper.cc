#include <iostream>
#include <fstream>
#include "ns3/simulator-module.h"
#include "ns3/node-module.h"
#include "ns3/core-module.h"
#include "ns3/helper-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/contrib-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);


  NodeContainer csmaNodes;
  csmaNodes.Create (2);
  NodeContainer wifiNodes;
  wifiNodes.Add (csmaNodes.Get (1));
  wifiNodes.Create (3);

  NetDeviceContainer csmaDevices;
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("5Mbps"));
  csma.SetChannelAttribute ("Delay", StringValue ("2ms"));
  csmaDevices = csma.Install (csmaNodes);

  NetDeviceContainer wifiDevices;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  WifiHelper wifi = WifiHelper::Default ();
  wifiDevices = wifi.Install (wifiPhy, wifiNodes);

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator",
				 "X", StringValue ("100.0"),
				 "Y", StringValue ("100.0"),
				 "Rho", StringValue ("Uniform:0:30"));
  mobility.SetMobilityModel ("ns3::StaticMobilityModel");
  mobility.Install (wifiNodes);

  Ipv4InterfaceContainer csmaInterfaces;
  Ipv4InterfaceContainer wifiInterfaces;
  InternetStackHelper internet;
  internet.Install (NodeContainer::GetGlobal ());
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  csmaInterfaces = ipv4.Assign (csmaDevices);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  wifiInterfaces = ipv4.Assign (wifiDevices);

  GlobalRouteManager::PopulateRoutingTables ();

  ApplicationContainer apps;
  OnOffHelper onoff ("ns3::UdpSocketFactory", 
                     InetSocketAddress ("10.1.2.2", 1025));
  onoff.SetAttribute ("OnTime", StringValue ("Constant:1.0"));
  onoff.SetAttribute ("OffTime", StringValue ("Constant:0.0"));
  apps = onoff.Install (csmaNodes.Get (0));
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (4.0));

  PacketSinkHelper sink ("ns3::UdpSocketFactory",
			 InetSocketAddress ("10.1.2.2", 1025));
  apps = sink.Install (wifiNodes.Get (1));
  apps.Start (Seconds (0.0));
  apps.Stop (Seconds (4.0));

  std::ofstream ascii;
  ascii.open ("wns3-helper.tr");
  CsmaHelper::EnableAsciiAll (ascii);
  CsmaHelper::EnablePcapAll ("wns3-helper");
  YansWifiPhyHelper::EnablePcapAll ("wsn3-helper");

  GtkConfigStore config;
  config.Configure ();

  Simulator::Stop (Seconds (4.5));

  Simulator::Run ();
  Simulator::Destroy ();


  return 0;
}
