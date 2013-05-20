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

#ifndef CONTIKI_DEVICE_HELPER_H
#define CONTIKI_DEVICE_HELPER_H

#include "ns3/net-device-container.h"
#include "ns3/object-factory.h"
#include <string.h>

namespace ns3 {

class Node;
class AttributeValue;

/**
 * \brief build ContikiNetDevice to allow ns-3 simulations to interact with Linux
 * tap devices and processes on the Linux host.
 */
class ContikiNetDeviceHelper {
public:
	/**
	 * Construct a ContikiNetDeviceHelper
	 */
	ContikiNetDeviceHelper();

	/*
	 * Set an attribute in the underlying ContikiNetDevice net device when these
	 * devices are automatically created.
	 *
	 * \param n1 the name of the attribute to set
	 * \param v1 the value of the attribute to set
	 */
	void SetAttribute(std::string n1, const AttributeValue &v1);

	/**
	 * This method installs a ContikiNetDevice on the specified Node and forms the
	 * bridge with the NetDevice specified.  The Node is specified using
	 * a Ptr<Node> and the NetDevice is specified using a Ptr<NetDevice>
	 *
	 * \param node The Ptr<Node> to install the ContikiNetDevice in
	 * \param nd The Ptr<NetDevice> to attach to the bridge.
	 * \returns A pointer to the new ContikiNetDevice NetDevice.
	 */
	Ptr<NetDevice> Install(Ptr<Node> node, Ptr<NetDevice> nd);

	/**
	 * This method installs a ContikiNetDevice on the specified Node and a loopback
	 * bridge with the created ContikiNetDevice.  The Node is specified using
	 * a Ptr<Node>.
	 *
	 * \param node The Ptr<Node> to install the ContikiNetDevice in
	 * \returns A pointer to the new ContikiNetDevice NetDevice.
	 */
	Ptr<ContikiNetDevice> Install(Ptr<Node> node);

	/**
	 * This method installs the entire Network Stack to a collection of Nodes.
	 * All nodes are located on the same vector co-ordinate and use the same executable for fork/exec.
	 *
	 * \param node The NodeContainer to install the various ContikiNetDevice objects.
	 * \param path The path used to locate the executable for the fork/exec call.
	 * \param mode The stack operation mode (MACOVERLAY or PHYOVERLAY; medium emulation or layer 2 and medium emulation).
	 */
	void Install(NodeContainer nodes, std::string mode, std::string apps);

	/**
	 * \internal
	 *
	 * This a sink where changes in simulation time are received from CurrentTs trace source
	 * and written to the corresponding shared memory segment to synchronize contiki's clock.
	 * \param oldValue Old time value
	 * \param newValue New time value
	 */
	void ContikiClockHandle(uint64_t oldValue, uint64_t newValue);

private:

	/**
	 * \internal
	 *
	 * Create shared memory segments and spawn a new process passing NodeId as parameter to it for IPC.
	 * If this method returns, Contiki Process should be able to open and map the same memory segments
	 * so that an IPC can be done.
	 */
	void CreateIpc(uint32_t nodeId);

	/**
	 * \internal
	 * Clear the file descriptors for the shared memory segments
	 * and unmap the corresponding addresses and unlinking the semaphores
	 */
	void ClearIpc(void);

	ObjectFactory m_deviceFactory;

	uint32_t m_nNodes;

	/**
	 * \internal
	 * semaphore for time update operations
	 */
	sem_t *m_sem_time;

	/**
	 * \internal
	 * semaphore for timer scheduling operations
	 */
	sem_t *m_sem_timer;

	sem_t *m_sem_timer_go;
	sem_t *m_sem_timer_done;

	/**
	 * \internal
	 * Shared Memory Object for time
	 */
	int m_shm_time;

	/**
	 * \internal
	 * The pointer to a shared memory address where to synchronize
	 * current simulation time value.
	 */
	uint8_t *m_traffic_time;

	sem_t *m_sem_go;
	sem_t *m_sem_done;

	std::stringstream m_ipc_go_name;
	std::stringstream m_ipc_done_name;
	/**
	 * \internal
	 * Name of input semaphore
	 */
	std::stringstream m_ipc_in_name;

	/**
	 * \internal
	 * Name of output semaphore
	 */
	std::stringstream m_ipc_out_name;

	/**
	 * \internal
	 * Name of time semaphore
	 */
	std::stringstream m_ipc_time_name;

	/**
	 * \internal
	 * Name of timer semaphore
	 */
	std::stringstream m_ipc_timer_name;
	std::stringstream m_ipc_timer_go_name;
	std::stringstream m_ipc_timer_done_name;

	/**
	 * \internal
	 * The pointer to a shared memory address where to receive traffic from.
	 */
	uint8_t *m_traffic_in;

	/**
	 * \internal
	 * The pointer to a shared memory address where to send traffic to.
	 */
	uint8_t *m_traffic_out;

	/**
	 * \internal
	 * Shared Memory Object for input traffic
	 */
	int m_shm_in;

	/**
	 * \internal
	 * Shared Memory Object for output traffic
	 */
	int m_shm_out;

	/**
	 * \internal
	 * Shared Memory Object for timers
	 */
	int m_shm_timer;

	/**
	 * \internal
	 * Semaphore for writing operations
	 */
	sem_t *m_sem_out;

	/**
	 * \internal
	 * Semaphore for reading operations
	 */
	sem_t *m_sem_in;

	/*
	 * \internal
	 * Size of traffic buffer
	 */
	size_t m_traffic_size;

	/*
	 * \internal
	 * Size of time buffer
	 */
	size_t m_time_size;

};

} // namespace ns3

#endif /* CONTIKI_DEVICE_HELPER_H */
