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
#include "ns3/pcap-file-wrapper.h"
#include "ns3/trace-helper.h"
#include "contiki-phy-helper.h"

NS_LOG_COMPONENT_DEFINE ("ContikiPhyHelper");

namespace ns3 {

ContikiPhyHelper::ContikiPhyHelper ()
	:m_pcapDlt (PcapHelper::DLT_RAW)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_deviceFactory.SetTypeId ("ns3::ContikiPhy");
}

void
ContikiPhyHelper::SetAttribute (std::string n1, const AttributeValue &v1)
{
  NS_LOG_FUNCTION (n1 << &v1);
  m_deviceFactory.Set (n1, v1);
}

Ptr<ContikiPhy>
ContikiPhyHelper::Install (Ptr<ContikiNetDevice> bridge, Ptr<ContikiMac> mac, ContikiPhy::PhyMode mode)
{
  Ptr<ContikiPhy> phy = m_deviceFactory.Create<ContikiPhy> ();
  /* Add a PHY Layer to the ContikiNetDevice - will probably be removed */
  bridge->SetPhy(phy);
  /* Add a PHY Layer to the NullMac */
  mac->SetPhy(phy);
  /* Set Mode and subsequently data rate and energy detection threshold */
  phy->SetMode(mode);

  return phy;
}

static void
PcapSniffTxEvent (
  Ptr<PcapFileWrapper> file,
  Ptr<const Packet>   packet)
{
  uint32_t dlt = file->GetDataLinkType ();

  if (dlt == PcapHelper::DLT_RAW)
    {
      file->Write (Simulator::Now (), packet);
      return;
    }
  else
      NS_ABORT_MSG ("PcapSniffTxEvent(): Unexpected data link type " << dlt);
}

static void
PcapSniffRxEvent (
  Ptr<PcapFileWrapper> file,
  Ptr<const Packet> packet)
{
  uint32_t dlt = file->GetDataLinkType ();

  if (dlt == PcapHelper::DLT_RAW)
    {
      file->Write (Simulator::Now (), packet);
      return;
    }
  else
      NS_ABORT_MSG ("PcapSniffRxEvent(): Unexpected data link type " << dlt);
}

void
ContikiPhyHelper::EnablePcapInternal (std::string prefix, Ptr<NetDevice> nd, bool promiscuous, bool explicitFilename)
{
	//
	// All of the Pcap enable functions vector through here including the ones
	// that are wandering through all of devices on perhaps all of the nodes in
	// the system.  We can only deal with devices of type ContikiNetDevice.
	//
	Ptr<ContikiNetDevice> device = nd->GetObject<ContikiNetDevice>();
	if (device == 0) {
		NS_LOG_INFO ("YansWifiHelper::EnablePcapInternal(): Device " << &device << " not of type ns3::WifiNetDevice");
		return;
	}

	Ptr<ContikiPhy> phy = device->GetPhy();
	NS_ABORT_MSG_IF(phy == 0,
			"ContikiPhyHelper::EnablePcapInternal(): Phy layer in ContikiDevice must be set");

	PcapHelper pcapHelper;

	std::string filename;
	if (explicitFilename) {
		filename = prefix;
	} else {
		filename = pcapHelper.GetFilenameFromDevice(prefix, device);
	}

	Ptr<PcapFileWrapper> file = pcapHelper.CreateFile(filename, std::ios::out,
			m_pcapDlt);

	phy->TraceConnectWithoutContext("MonitorSnifferTx", MakeBoundCallback(&PcapSniffTxEvent, file));
	phy->TraceConnectWithoutContext("MonitorSnifferRx", MakeBoundCallback(&PcapSniffRxEvent, file));
}

} // namespace ns3
