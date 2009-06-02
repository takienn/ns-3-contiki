/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 *
 * Authors: Joe Kopena <tjkopena@cs.drexel.edu>
 *
 * This program conducts a simple experiment: It places two nodes at a
 * parameterized distance apart.  One node generates packets and the
 * other node receives.  The stat framework collects data on packet
 * loss.  Outside of this program, a control script uses that data to
 * produce graphs presenting performance at the varying distances.
 * This isn't a typical simulation but is a common "experiment"
 * performed in real life and serves as an accessible exemplar for the
 * stat framework.  It also gives some intuition on the behavior and
 * basic reasonability of the NS-3 WiFi models.
 *
 * Applications used by this program are in test02-apps.h and
 * test02-apps.cc, which should be in the same place as this file.
 * 
 */

// #define NS3_LOG_ENABLE // Now defined by Makefile

#include <sstream>

#include "ns3/core-module.h"
#include "ns3/common-module.h"
#include "ns3/node-module.h"
#include "ns3/helper-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"

#include "ns3/stats-module.h"

#include "wifi-example-apps.h"

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE ("WiFiThroughputExperiment");




void TxCallback(Ptr<CounterCalculator<uint32_t> > datac,
                std::string path, Ptr<const Packet> packet,
                Mac48Address realto) {
  NS_LOG_INFO("Sent frame to " << realto << "; counted in " <<
              datac->GetKey());
  datac->Update();
  // end TxCallback
}




//----------------------------------------------------------------------
//-- main
//----------------------------------------------
int main(int argc, char *argv[]) {

  double distance = 50.0;
  string format("omnet");

  string experiment("wifi-throughput-test");
  string strategy("wifi-default");
  string input;
  string runID;
  int nStas = 2;
  
  //LogComponentEnableAll(LOG_LEVEL_ALL);
  
  {
    stringstream sstr;
    sstr << "run-" << time(NULL);
    runID = sstr.str();
  }

  // Set up command line parameters used to control the experiment.
  CommandLine cmd;
  cmd.AddValue("distance", "Distance apart to place nodes (in meters).",
               distance);
  cmd.AddValue("format", "Format to use for data output.",
               format);
  cmd.AddValue("experiment", "Identifier for experiment.",
               experiment);
  cmd.AddValue("strategy", "Identifier for strategy.",
               strategy);
  cmd.AddValue("run", "Identifier for run.",
               runID);
  cmd.Parse (argc, argv);

  if (format != "omnet" && format != "db") {
    NS_LOG_ERROR("Unknown output format '" << format << "'");
    return -1;
  }

  #ifndef STATS_HAS_SQLITE3
  if (format == "db") {
      NS_LOG_ERROR("sqlite support not compiled in.");
      return -1;
  }
  #endif

  {
  stringstream sstr("");
  sstr << distance;
  input = sstr.str();
  }




  //------------------------------------------------------------
  //-- Create nodes and network stacks
  //--------------------------------------------
  NS_LOG_INFO("Creating nodes.");
  NodeContainer stas;
  NodeContainer ap;
  NetDeviceContainer apDev;
  NetDeviceContainer stasDev;
  Ipv4InterfaceContainer staInterface;
  Ipv4InterfaceContainer apInterface;

  Ssid ssid = Ssid ( "wifi-default" );
  
  ap.Create(1);
  stas.Create(nStas);
  
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  WifiHelper wifi = WifiHelper::Default ();

  InternetStackHelper internet;
  internet.Install(NodeContainer(stas,ap));
  Ipv4AddressHelper ipAddrs;
  ipAddrs.SetBase("192.168.0.0", "255.255.255.0");

  
  NS_LOG_INFO("Installing WiFi and Internet stack on AP.");
  // wifiMac.SetType ("ns3::AdhocWifiMac");
  wifiMac.SetType ("ns3::NqapWifiMac",
                   "Ssid", SsidValue (ssid),
                   "BeaconGeneration", BooleanValue (true),
                   "BeaconInterval", TimeValue (Seconds (2.5)));
  apDev = wifi.Install(wifiPhy, wifiMac, ap);
  apInterface = ipAddrs.Assign(apDev);
  
  
  NS_LOG_INFO("Installing WiFi and Internet stack on nodes.");
  WifiHelper wifiNodes = WifiHelper::Default ();
  NqosWifiMacHelper wifiMacNodes = NqosWifiMacHelper::Default ();
  //wifiMac.SetType ("ns3::AdhocWifiMac");
  wifiMac.SetType ("ns3::NqstaWifiMac",
                   "Ssid", SsidValue (ssid),
                   "ActiveProbing", BooleanValue (false));
  stasDev = wifi.Install(wifiPhy, wifiMac, stas);
  staInterface = ipAddrs.Assign(stasDev);


  //------------------------------------------------------------
  //-- Setup physical layout
  //--------------------------------------------
  NS_LOG_INFO("Installing static mobility; distance " << distance << " .");
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc =
    CreateObject<ListPositionAllocator>();
  positionAlloc->Add(Vector(0.0, 0.0, 0.0));
  positionAlloc->Add(Vector(0.0, distance, 0.0));
  mobility.SetPositionAllocator(positionAlloc);
  mobility.Install(NodeContainer(stas,ap));




  //------------------------------------------------------------
  //-- Create a custom traffic source and sink
  //--------------------------------------------
  NS_LOG_INFO ("Create traffic source & sink.");
  Ptr<Node> appSink = NodeList::GetNode(0);  
  Ptr<Receiver> receiver = CreateObject<Receiver>();
  appSink->AddApplication(receiver);
  receiver->Start(Seconds(0));
  
  for( int i=0; i<nStas; i++ ) {
    Ptr<Node> appSource = NodeList::GetNode(i+1);  
    Ptr<Sender> sender = CreateObject<Sender>();
    sender->SetAttribute ("Destination", Ipv4AddressValue(apInterface.GetAddress(0)));
    sender->SetAttribute ("DataRate", DataRateValue (DataRate ("1kbps")) );
    sender->SetAttribute ("PacketSize", UintegerValue(1000));
    sender->SetAttribute ("NumPackets", UintegerValue(1000));
    
    appSource->AddApplication(sender);
    sender->Start(Seconds(1));
  }
  //  Config::Set("/NodeList/*/ApplicationList/*/$Sender/Destination",
  //              Ipv4AddressValue("192.168.0.2"));




  //------------------------------------------------------------
  //-- Setup stats and data collection
  //--------------------------------------------

  // Create a DataCollector object to hold information about this run.
  DataCollector data;
  data.DescribeRun(experiment,
                   strategy,
                   input,
                   runID);

  // Add any information we wish to record about this run.
  data.AddMetadata("author", "tjkopena");


  // Create a counter to track how many frames are generated.  Updates
  // are triggered by the trace signal generated by the WiFi MAC model
  // object.  Here we connect the counter to the signal via the simple
  // TxCallback() glue function defined above.
  Ptr<PacketCounterCalculator> totalTx = CreateObject<PacketCounterCalculator>();
  totalTx->SetKey("wifiMac-tx-packets");
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTx",
                  MakeCallback(&PacketCounterCalculator::PacketUpdate, totalTx));
  data.AddDataCalculator(totalTx);

  Ptr<PacketCounterCalculator> totalTxDrop = CreateObject<PacketCounterCalculator>();
  totalTxDrop->SetKey("wifiMac-txDrop-packets");
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTxDrop",
                  MakeCallback(&PacketCounterCalculator::PacketUpdate, totalTxDrop));
  data.AddDataCalculator(totalTxDrop);

  
  // This is similar, but creates a counter to track how many frames
  // are received.  Instead of our own glue function, this uses a
  // method of an adapter class to connect a counter directly to the
  // trace signal generated by the WiFi MAC.
  Ptr<PacketCounterCalculator> totalRx = CreateObject<PacketCounterCalculator>();
  totalRx->SetKey("wifiMac-rx-packets");
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                  MakeCallback(&PacketCounterCalculator::PacketUpdate, totalRx));
  data.AddDataCalculator(totalRx);
  
  Ptr<PacketCounterCalculator> totalRxDrop = CreateObject<PacketCounterCalculator>();
  totalRxDrop->SetKey("wifiMac-rxDrop-packets");
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRxDrop",
                  MakeCallback(&PacketCounterCalculator::PacketUpdate, totalRxDrop));
  data.AddDataCalculator(totalRxDrop);
  
  


  // This counter tracks how many packets---as opposed to frames---are
  // generated.  This is connected directly to a trace signal provided
  // by our Sender class.
  Ptr<PacketCounterCalculator> appTx =
    CreateObject<PacketCounterCalculator>();
  appTx->SetKey("sender-tx-packets");
  Config::Connect("/NodeList/*/ApplicationList/*/$Sender/Tx",
                  MakeCallback(&PacketCounterCalculator::PacketUpdate,
                                    appTx));
  data.AddDataCalculator(appTx);

  // Here a counter for received packets is directly manipulated by
  // one of the custom objects in our simulation, the Receiver
  // Application.  The Receiver object is given a pointer to the
  // counter and calls its Update() method whenever a packet arrives.
  Ptr<CounterCalculator<> > appRx =
    CreateObject<CounterCalculator<> >();
  appRx->SetKey("receiver-rx-packets");
  receiver->SetCounter(appRx);
  data.AddDataCalculator(appRx);




  /**
   * Just to show this is here...
   Ptr<MinMaxAvgTotalCalculator<uint32_t> > test = 
   CreateObject<MinMaxAvgTotalCalculator<uint32_t> >();
   test->SetKey("test-dc");
   data.AddDataCalculator(test);

   test->Update(4);
   test->Update(8);
   test->Update(24);
   test->Update(12);
  **/

  // This DataCalculator connects directly to the transmit trace
  // provided by our Sender Application.  It records some basic
  // statistics about the sizes of the packets received (min, max,
  // avg, total # bytes), although in this scenaro they're fixed.
  Ptr<PacketSizeMinMaxAvgTotalCalculator> appTxPkts =
    CreateObject<PacketSizeMinMaxAvgTotalCalculator>();
  appTxPkts->SetKey("tx-pkt-size");
  Config::Connect("/NodeList/*/ApplicationList/*/$Sender/Tx",
                  MakeCallback
                    (&PacketSizeMinMaxAvgTotalCalculator::PacketUpdate,
                    appTxPkts));
  data.AddDataCalculator(appTxPkts);


  // Here we directly manipulate another DataCollector tracking min,
  // max, total, and average propagation delays.  Check out the Sender
  // and Receiver classes to see how packets are tagged with
  // timestamps to do this.
  Ptr<TimeMinMaxAvgTotalCalculator> delayStat =
    CreateObject<TimeMinMaxAvgTotalCalculator>();
  delayStat->SetKey("delay");
  receiver->SetDelayTracker(delayStat);
  data.AddDataCalculator(delayStat);



  YansWifiPhyHelper::EnablePcap ("wifi-throughput-experiment", stasDev);
  YansWifiPhyHelper::EnablePcap ("wifi-throughput-experiment", apDev);

  //------------------------------------------------------------
  //-- Run the simulation
  //--------------------------------------------
  NS_LOG_INFO("Run Simulation.");
  Simulator::Stop (Seconds (50));
  Simulator::Run();    
  Simulator::Destroy();




  //------------------------------------------------------------
  //-- Generate statistics output.
  //--------------------------------------------

  // Pick an output writer based in the requested format.
  Ptr<DataOutputInterface> output = 0;
  if (format == "omnet") {
    NS_LOG_INFO("Creating omnet formatted data output.");
    output = CreateObject<OmnetDataOutput>();
  } else if (format == "db") {
    #ifdef STATS_HAS_SQLITE3
      NS_LOG_INFO("Creating sqlite formatted data output.");
      output = CreateObject<SqliteDataOutput>();
    #endif
  } else {
    NS_LOG_ERROR("Unknown output format " << format);
  }

  // Finally, have that writer interrogate the DataCollector and save
  // the results.
  if (output != 0)
    output->Output(data);

  // end main
}
