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

#include <sys/stat.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sstream>
#include <string>

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

NS_LOG_COMPONENT_DEFINE("ContikiNetDeviceHelper");

namespace ns3 {

ContikiNetDeviceHelper::ContikiNetDeviceHelper() {
	NS_LOG_FUNCTION_NOARGS ();
	m_deviceFactory.SetTypeId("ns3::ContikiNetDevice");
}

void ContikiNetDeviceHelper::SetAttribute(std::string n1,
		const AttributeValue &v1) {
	NS_LOG_FUNCTION (n1 << &v1);
	m_deviceFactory.Set(n1, v1);
}

Ptr<ContikiNetDevice> ContikiNetDeviceHelper::Install(Ptr<Node> node) {
	/* Connect node to bridge */
	Ptr<ContikiNetDevice> bridge = m_deviceFactory.Create<ContikiNetDevice>();
	node->AddDevice(bridge);
	/* Loop bridge to itself */
	bridge->SetBridgedNetDevice(bridge);

	return bridge;
}

Ptr<NetDevice> ContikiNetDeviceHelper::Install(Ptr<Node> node,
		Ptr<NetDevice> nd) {
	Ptr<ContikiNetDevice> bridge = m_deviceFactory.Create<ContikiNetDevice>();
	node->AddDevice(bridge);
	bridge->SetBridgedNetDevice(nd);

	return bridge;
}

void ContikiNetDeviceHelper::Install(NodeContainer nodes, std::string mode,
		std::string apps) {

	//
	// Registering our Clock trace sink to handle clock synchronization
	//
	Simulator::GetImplementation()->TraceConnectWithoutContext("CurrentTs",
			MakeCallback(&ContikiNetDeviceHelper::ContikiClockHandle, this));

	uint32_t nodeCount = nodes.GetN();

	m_nNodes = nodeCount;

	/* Seperating contiki applications paths
	 * the apps list is a ',' separated list of applications
	 */
	std::vector<std::string> apps_list;
	std::stringstream ss(apps);
	std::string app;
	while (std::getline(ss, app, ',')) {
		apps_list.push_back(app);
	}

	/* Create Pointers */
	Ptr<ContikiNetDevice> bridge[nodeCount + 1];
	Ptr<ContikiMac> mac[nodeCount + 1];
	Ptr<ContikiPhy> phy[nodeCount + 1];

	/* Create Helpers */
	ContikiMacHelper contikiMacHelper;
	ContikiPhyHelper contikiPhyHelper;
	ContikiChannelHelper contikiChannelHelper;

	/* Create Channel */
	Ptr<ContikiChannel> channel = contikiChannelHelper.Create();
	channel->SetPropagationDelayModel(
			CreateObject<ConstantSpeedPropagationDelayModel>());
	Ptr<LogDistancePropagationLossModel> log = CreateObject<
			LogDistancePropagationLossModel>();
	channel->SetPropagationLossModel(log);

	/* Create Position of all Nodes */
	Ptr<MobilityModel> pos = CreateObject<ConstantPositionMobilityModel>();
	pos->SetPosition(Vector(0.0, 0.0, 0.0));

	/* Build Network Stack for all Nodes */
	for (uint8_t i = 0; i < nodeCount; i++) {

		// Preparing IPC objects
		CreateIpc(nodes.Get(i)->GetId());

		bridge[i] = m_deviceFactory.Create<ContikiNetDevice>();
		bridge[i]->SetMode(mode);

		if (i < apps_list.size())
			bridge[i]->SetApplication(apps_list[i]);
		else
			bridge[i]->SetApplication(apps_list[0]); // XXX Needs better solution here

		/* Add Socket Bridge to Node (Spawns Contiki Process */
		nodes.Get(i)->AddDevice(bridge[i]);
		bridge[i]->SetBridgedNetDevice(bridge[i]);
		/* Add MAC layer to ContikiNetDevice */
		mac[i] = contikiMacHelper.Install(bridge[i]);
		/* Add PHY to ContikiNetDevice and ContikiMac */
		phy[i] = contikiPhyHelper.Install(bridge[i], mac[i],
				ContikiPhy::DSSS_O_QPSK_GHz);
		contikiPhyHelper.EnablePcapAll("contiki-device", true);

		/* Add Physical Components (Channel and Position) */
		contikiChannelHelper.Install(channel, bridge[i]);
		phy[i]->SetMobility(pos);
	}
}

void ContikiNetDeviceHelper::CreateIpc(uint32_t nodeId) {
	NS_LOG_FUNCTION_NOARGS ();

	/* Shared Memory*/

	// Preparing shm/sem names
	m_ipc_in_name << "/ns_contiki_sem_in_" << nodeId;
	m_ipc_out_name << "/ns_contiki_sem_out_" << nodeId;
	m_ipc_timer_name << "/ns_contiki_sem_timer_" << nodeId;
	m_ipc_time_name << "/ns_contiki_sem_time_" << nodeId;
	m_ipc_go_name << "/ns_contiki_sem_go_";	// << nodeId;;
	m_ipc_done_name << "/ns_contiki_sem_done_";	// << nodeId;;
	m_ipc_timer_go_name << "/ns_contiki_sem_timer_go_" << nodeId;
	m_ipc_timer_done_name << "/ns_contiki_sem_timer_done_" << nodeId;

	//Assuring there are no shm/sem leftovers from previous executions
	//ClearIpc();

	//Now Assuring the creation of ALL shm/sem objects but traffic_in and traffic_timer
	// are left for the read thread (where they are used) to map.
	if ((m_shm_in = shm_open(m_ipc_in_name.str().c_str(),
			O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))
			== -1)
		NS_FATAL_ERROR("shm_open(m_shm_in) " << strerror(errno));

	if ((m_shm_out = shm_open(m_ipc_out_name.str().c_str(),
			O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))
			== -1)
		NS_FATAL_ERROR("shm_open(m_shm_out)" << strerror(errno));

	if ((m_shm_timer = shm_open(m_ipc_timer_name.str().c_str(),
			O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1)
		NS_FATAL_ERROR("shm_open()" << strerror(errno));

	if ((m_shm_time = shm_open(m_ipc_time_name.str().c_str(), O_RDWR | O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1)
		NS_FATAL_ERROR("shm_open(m_shm_time) " << strerror(errno));

	if (ftruncate(m_shm_in, m_traffic_size + sizeof(size_t)) == -1)
		NS_FATAL_ERROR("ftruncate(m_shm_in) " << strerror(errno));

	if (ftruncate(m_shm_out, m_traffic_size + sizeof(size_t)) == -1)
		NS_FATAL_ERROR("shm_open(m_shm_out) " << strerror(errno));

	if (ftruncate(m_shm_time, m_time_size) == -1)
		NS_FATAL_ERROR("ftruncate(m_shm_time)" << strerror(errno));

	if (ftruncate(m_shm_timer, m_time_size + 1) == -1)
		NS_FATAL_ERROR("ftruncate(m_shm_timer)" << strerror(errno));

	/* XXX A note here: not just data is trafered but also data size
	 * it is saved in sizeof(size_t) memory.
	 */

	m_traffic_out = (uint8_t *) mmap(NULL,
			sizeof(size_t) + m_traffic_size + sizeof(size_t),
			PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_out, 0);

	//if (m_traffic_time == NULL)
	m_traffic_time = (uint8_t *) mmap(NULL, m_time_size, PROT_READ | PROT_WRITE,
			MAP_SHARED, m_shm_time, 0);

	if ((m_sem_in = sem_open(m_ipc_in_name.str().c_str(), O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1)) == SEM_FAILED )
		NS_FATAL_ERROR(" ns -3 sem_open(sem_in) failed: " << strerror(errno));

	if ((m_sem_out = sem_open(m_ipc_out_name.str().c_str(), O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1)) == SEM_FAILED )
		NS_FATAL_ERROR("ns -3 sem_open(sem_out) failed: " << strerror(errno));

	if ((m_sem_time = sem_open(m_ipc_time_name.str().c_str(), O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1)) == SEM_FAILED )
		NS_FATAL_ERROR("ns -3 sem_open(sem_time) failed: " << strerror(errno));
	if ((m_sem_timer = sem_open(m_ipc_timer_name.str().c_str(), O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1)) == SEM_FAILED )
		NS_FATAL_ERROR("ns -3 sem_open(sem_timer) failed: " << strerror(errno));
	if ((m_sem_go = sem_open(m_ipc_go_name.str().c_str(), O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)) == SEM_FAILED )
		NS_FATAL_ERROR("ns -3 sem_open(sem_go) failed: " << strerror(errno));

	if ((m_sem_done = sem_open(m_ipc_done_name.str().c_str(), O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)) == SEM_FAILED )
		NS_FATAL_ERROR("ns -3 sem_open(sem_done) failed: " << strerror(errno));
	if ((m_sem_timer_go = sem_open(m_ipc_timer_go_name.str().c_str(), O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)) == SEM_FAILED )
		NS_FATAL_ERROR(
				"ns -3 sem_open(m_sem_timer_go) failed: " << strerror(errno));

	if ((m_sem_timer_done = sem_open(m_ipc_timer_done_name.str().c_str(),
			O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)) == SEM_FAILED )
		NS_FATAL_ERROR(
				"ns -3 sem_open(m_sem_timer_done) failed: " << strerror(errno));

}

void ContikiNetDeviceHelper::ClearIpc() {

	munmap(m_traffic_in, sizeof(size_t) + m_traffic_size);
	munmap(m_traffic_out, sizeof(size_t) + m_traffic_size);
	munmap(m_traffic_time, m_time_size);

	shm_unlink(m_ipc_in_name.str().c_str());
	shm_unlink(m_ipc_out_name.str().c_str());
	shm_unlink(m_ipc_time_name.str().c_str());
	shm_unlink(m_ipc_timer_name.str().c_str());

	sem_close(m_sem_in);
	sem_close(m_sem_out);
	sem_close(m_sem_time);
	sem_close(m_sem_timer);
	sem_close(m_sem_go);
	sem_close(m_sem_done);

	sem_unlink(m_ipc_in_name.str().c_str());
	sem_unlink(m_ipc_out_name.str().c_str());
	sem_unlink(m_ipc_time_name.str().c_str());
	sem_unlink(m_ipc_timer_name.str().c_str());
	sem_unlink(m_ipc_go_name.str().c_str());
	sem_unlink(m_ipc_done_name.str().c_str());
}

void ContikiNetDeviceHelper::ContikiClockHandle(uint64_t oldValue,
		uint64_t newValue) {

	int rtval; // to probe sem_done value
	uint32_t N = m_nNodes;

	/////// Writing new time //////////////
	if (sem_wait(m_sem_time) == -1)
		NS_FATAL_ERROR("sem_wait() failed: " << strerror(errno));

	memcpy(m_traffic_time, (void *) &newValue, 8);

	if (sem_post(m_sem_time) == -1)
		NS_FATAL_ERROR("sem_wait() failed: " << strerror(errno));
	///////////////////////////////////////

	////// Waiting for contiki to live the moment /////////
	NS_LOG_LOGIC("ns-3 waiting for contiki at " << Simulator::Now());
	fflush(stdout);
	if (sem_getvalue(m_sem_done, &rtval) == -1)
		perror("sem_getvalue(m_sem_done) error");
	while ((uint32_t) (rtval) < N) {
		sem_getvalue(m_sem_done, &rtval);
	}NS_LOG_LOGIC("ns-3 got contiki");
	fflush(stdout);
	///////////////////////////////////////////////////////

	///// Resetting the semaphores ///////
	for (uint32_t i = 0; i < N; i++) {
		sem_wait(m_sem_done);
		sem_post(m_sem_go);
	}
	//////////////////////////////////////
}
} // namespace ns3
