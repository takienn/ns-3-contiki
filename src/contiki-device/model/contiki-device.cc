#include <semaphore.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string>

#include "contiki-device.h"

extern "C" void ContikiMain(char *nodeId, int mode, char *addr, char *app);

NS_LOG_COMPONENT_DEFINE("ContikiNetDevice");

namespace ns3 {

IpcReader::Data ContikiIpcReader::DoRead(void) {
	NS_LOG_FUNCTION_NOARGS ();

	uint32_t bufferSize = 65536;
	uint8_t *buf = (uint8_t *) malloc(bufferSize);
	memset(buf, 0, bufferSize);

	NS_ABORT_MSG_IF(buf == 0, "malloc() failed");

	size_t input_size = 0;

	fflush(stdout);
	int rtval;
	sem_getvalue(m_sem_traffic_done, &rtval);
	if (rtval == 1) {

		NS_LOG_LOGIC("contiki wrote something " << m_nodeId);

		if (sem_wait(m_sem_in) == -1)
			NS_FATAL_ERROR("sem_wait() failed: " << strerror(errno));

		NS_LOG_LOGIC("ns3 reading " << m_nodeId);
		// First read input size
		memcpy(&input_size, m_traffic_in, sizeof(size_t));

		// Now read input
		memcpy(buf, m_traffic_in + sizeof(size_t), input_size);
		memset(m_traffic_in, 0, bufferSize + sizeof(size_t));

		NS_LOG_LOGIC("ns3 read " << m_nodeId);

		NS_LOG_LOGIC("ns3 releasing contiki after read " << m_nodeId);
		sem_wait(m_sem_traffic_done);
		sem_post(m_sem_traffic_go);
		NS_LOG_LOGIC("ns3 released contiki after read " << m_nodeId);

		if (sem_post(m_sem_in) == -1)
			NS_FATAL_ERROR("sem_post() failed: " << strerror(errno));
	}

	if (input_size == 0) {
		//NS_LOG_LOGIC ("ContikiNetDeviceFdReader::DoRead(): done" << m_nodeId);
		free(buf);
		buf = 0;
		//len = 0;
	}

	if (input_size > 0)
		NS_LOG_LOGIC("read data of length " << input_size);
	return IpcReader::Data(buf, input_size);
}

NS_OBJECT_ENSURE_REGISTERED(ContikiNetDevice);

TypeId ContikiNetDevice::GetTypeId(void) {
	static TypeId tid =
			TypeId("ns3::ContikiNetDevice").SetParent<NetDevice>().AddConstructor<
					ContikiNetDevice>().AddAttribute("Start",
					"The simulation time at which to spin up the socket device read thread.",
					TimeValue(Seconds(0.)),
					MakeTimeAccessor(&ContikiNetDevice::m_tStart),
					MakeTimeChecker()).AddAttribute("Stop",
					"The simulation time at which to tear down the socket device read thread.",
					TimeValue(Seconds(0.)),
					MakeTimeAccessor(&ContikiNetDevice::m_tStop),
					MakeTimeChecker());
	return tid;
}
//
sem_t* ContikiNetDevice::m_sem_time = NULL;
////sem_t* ContikiNetDevice::m_sem_timer = NULL;
sem_t* ContikiNetDevice::m_sem_go = NULL;
sem_t* ContikiNetDevice::m_sem_done = NULL;
int ContikiNetDevice::m_shm_time = 0;
uint8_t *ContikiNetDevice::m_traffic_time = 0;
uint32_t ContikiNetDevice::m_nNodes = 0;

ContikiNetDevice::ContikiNetDevice() :
		m_node(0), m_ifIndex(0), m_traffic_in(NULL), m_traffic_out(NULL), m_traffic_size(
				65536), m_time_size(8), m_startEvent(), m_stopEvent(), m_ipcReader(
				0) {
	NS_LOG_FUNCTION_NOARGS ();
	m_packetBuffer = new uint8_t[65536];

	Start(m_tStart);
}

ContikiNetDevice::~ContikiNetDevice() {
	NS_LOG_FUNCTION_NOARGS ();

	StopContikiDevice();

	delete[] m_packetBuffer;
	m_packetBuffer = 0;
	m_bridgedDevice = 0;
}

uint32_t ContikiNetDevice::GetNNodes(void) {
	NS_LOG_FUNCTION_NOARGS ();
	return m_nNodes;
}

void ContikiNetDevice::SetNNodes(uint32_t n) {
	NS_LOG_FUNCTION (n);
	m_nNodes = n;
}
void ContikiNetDevice::DoDispose() {
	NS_LOG_FUNCTION_NOARGS ();
	StopContikiDevice();
	NetDevice::DoDispose();
}

void ContikiNetDevice::Start(Time tStart) {
	NS_LOG_FUNCTION (tStart);

	//
	//
	// Cancel any pending start event and schedule a new one at some relative time in the future.
	//
	Simulator::Cancel(m_startEvent);
	m_startEvent = Simulator::Schedule(tStart,
			&ContikiNetDevice::StartContikiDevice, this);

	void (*f)(void) = 0;
	Simulator::ScheduleWithContext(m_nodeId, NanoSeconds(1.0), f);
}

void ContikiNetDevice::Stop(Time tStop) {
	NS_LOG_FUNCTION (tStop);
	//
	// Cancel any pending stop event and schedule a new one at some relative time in the future.
	//
	Simulator::Cancel(m_stopEvent);
	m_stopEvent = Simulator::Schedule(tStop,
			&ContikiNetDevice::StopContikiDevice, this);
}

void ContikiNetDevice::StartContikiDevice(void) {
	NS_LOG_FUNCTION_NOARGS ();

	NS_ABORT_MSG_IF(m_traffic_in != NULL,
			"ContikiNetDevice::StartContikiDevice(): IPC Shared Memory already created");

	m_nodeId = GetNode()->GetId();

	// Launching contiki process and wait

	NS_LOG_LOGIC ("Creating IPC Shared Memory");

	CreateIpc();

	//
	// Spin up the Contiki Device and start receiving packets.
	//
	//
	// Now spin up a read thread to read packets from the tap device.
	//
	NS_ABORT_MSG_IF(m_ipcReader != 0,
			"ContikiNetDevice::StartContikiDevice(): Receive thread is already running");
	NS_LOG_LOGIC ("Spinning up read thread");

	m_ipcReader = Create<ContikiIpcReader>();
	m_ipcReader->Start(MakeCallback(&ContikiNetDevice::ReadCallback, this),
			m_nodeId, child);

	if ((child = fork()) == -1)
		NS_ABORT_MSG(
				"ContikiNetDevice::CreateIpc(): Unix fork error, errno = " << strerror (errno));
	else if (child) { /*  This is the parent. */
		NS_LOG_DEBUG ("Parent process");NS_LOG_LOGIC("Child PID: " << child);




		//Ordering contiki to continue, now that all preparations are ready.
//
//		int status;
//		waitpid(child, &status, WUNTRACED);
//		if(WIFSTOPPED(status))
//			kill(child, SIGCONT);

	} else {

		//For the sake of respect, contiki won't continue
		//untill ordered to do so, ns-3 might still need
		//to prepare some stuff.
		//printf("Stopping child %d\n", getpid());
//		if (kill(getpid(), SIGSTOP) == -1)
//			perror("kill(sigstop) failed ");764 nodeId 1

		//Running Contiki

		std::stringstream ss;
		ss << m_nodeId;
		char c_nodeId[128];
		strcpy(c_nodeId, ss.str().c_str());
		char app[128] = "\0";
		strcpy(app, m_application.c_str());

		//char path[128] = "/home/kentux/mercurial/contiki-original/examples/dummy/dummy.ns3";

		//execlp(path,path,c_nodeId, "0","NULL", app, NULL);
		NS_LOG_LOGIC("ContikiMain pid " << getpid() << " nodeId " << c_nodeId << " m_nodeId " << m_nodeId);
		fflush(stdout);

		ContikiMain(c_nodeId, 0, NULL, app);

	}

}

void ContikiNetDevice::ContikiClockHandle(uint64_t oldValue,
		uint64_t newValue) {

	int rtval; // to probe sem_done value
	uint32_t N = GetNNodes(); // Number of nodes

	/////// Writing new time //////////////

	NS_LOG_LOGIC("Handling new time step " << newValue);
	if (sem_wait(m_sem_time) == -1)
		NS_FATAL_ERROR("sem_wait() failed: " << strerror(errno));

	memcpy(m_traffic_time, (void *) &newValue, 8);

	if (sem_post(m_sem_time) == -1)
		NS_FATAL_ERROR("sem_wait() failed: " << strerror(errno));
	///////////////////////////////////////

	////// Waiting for contiki to live the moment /////////
	NS_LOG_LOGIC("ns-3 waiting for contikis at " << Simulator::Now() << std::endl);
	fflush(stdout);
	if (sem_getvalue(m_sem_done, &rtval) == -1)
		perror("sem_getvalue(m_sem_done) error");
	while ((uint32_t) (rtval) < N) {
		//NS_LOG_LOGIC("contikis not ready yet: " << rtval);
		sem_getvalue(m_sem_done, &rtval);
	}
	NS_LOG_LOGIC("ns-3 got contikis");
	fflush(stdout);
	///////////////////////////////////////////////////////

	///// Resetting the semaphores ///////
	for (uint32_t i = 0; i < N; i++) {
		sem_wait(m_sem_done);
	}

	// Releasing Contiki Nodes
	NS_LOG_LOGIC("Releasing All Contiki Processes\n");
	for(uint32_t i = 0; i< N; i++){
		sem_post(m_sem_go);
		NS_LOG_LOGIC("Releasing Contiki Processe " << i << "\n");
	}
	NS_LOG_LOGIC("Released All Contiki Processes\n");
	//////////////////////////////////////

	//XXX This trick is to force ns-3 to go in slow motion so that
	//Contiki can follow

	//void (*f)(void) = 0;
	//Simulator::ScheduleWithContext(0, MilliSeconds(1.0), f);

}

uint32_t ContikiNetDevice::GetNodeId() {
	return m_nodeId;
}

void ContikiNetDevice::StopContikiDevice(void) {
	NS_LOG_FUNCTION_NOARGS ();

	if (m_ipcReader != 0) {
		m_ipcReader->Stop();
		m_ipcReader = 0;
	}

	//unmapping mapped memory addresses and closing semaphore variables

	if (m_traffic_in != NULL) {
		if (munmap(m_traffic_in, m_traffic_size + sizeof(size_t)) == -1)
			NS_FATAL_ERROR("munmap() failed: " << strerror(errno));

		m_traffic_in = NULL;
	}

	if (m_traffic_out != NULL) {
		if (munmap(m_traffic_out, m_traffic_size + sizeof(size_t)) == -1)
			NS_FATAL_ERROR("munmap() failed: " << strerror(errno));

		m_traffic_out = NULL;
	}

	// Well, time memory is unique for all nodes it might just been
	// unmapped by another node, no need to do it again.
	if (m_traffic_time != NULL) {
		if (munmap(m_traffic_time, m_time_size) == -1)
			NS_FATAL_ERROR("munmap() failed: " << strerror(errno));

		m_traffic_time = NULL;
	}

	NS_LOG_LOGIC("Killing Child");
	kill(child, SIGKILL);

	usleep(100000);

	ClearIpc();
}

void ContikiNetDevice::CreateIpc(void) {
	NS_LOG_FUNCTION_NOARGS ();

	Ptr<NetDevice> nd = GetBridgedNetDevice();
	Ptr<Node> n = nd->GetNode();

	/* Generate MAC address, assign to Node */
	Mac64Address mac64Address = Mac64Address::Allocate();

	Address ndAddress = Address(mac64Address);
	nd->SetAddress(ndAddress);

	/* Shared Memory*/

	// Preparing shm/sem names
	m_shm_in_name << "/ns_contiki_traffic_in_" << m_nodeId;
	m_shm_out_name << "/ns_contiki_traffic_out_" << m_nodeId;
	m_shm_timer_name << "/ns_contiki_traffic_timer_" << m_nodeId;
	m_shm_time_name << "/ns_contiki_traffic_time_";// << m_nodeId;

	m_sem_in_name << "/ns_contiki_sem_in_" << m_nodeId;
	m_sem_out_name << "/ns_contiki_sem_out_" << m_nodeId;
	m_sem_timer_name << "/ns_contiki_sem_timer_" << m_nodeId;
	m_sem_time_name << "/ns_contiki_sem_time_";// << m_nodeId;
	m_sem_go_name << "/ns_contiki_sem_go_";	// << m_nodeId;;
	m_sem_done_name << "/ns_contiki_sem_done_";	// << m_nodeId;;
	m_sem_timer_go_name << "/ns_contiki_sem_timer_go_" << m_nodeId;
	m_sem_timer_done_name << "/ns_contiki_sem_timer_done_" << m_nodeId;

	//Assuring there are no shm/sem leftovers from previous executions
	//ClearIpc();

	//Now Assuring the creation of ALL shm/sem objects but traffic_in and traffic_timer
	// are left for the read thread (where they are used) to map.
	if ((m_shm_in = shm_open(m_shm_in_name.str().c_str(),
			O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))
			== -1)
		NS_FATAL_ERROR("shm_open(m_shm_in) " << strerror(errno));

	if ((m_shm_out = shm_open(m_shm_out_name.str().c_str(),
			O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))
			== -1)
		NS_FATAL_ERROR("shm_open(m_shm_out)" << strerror(errno));

	if ((m_shm_timer = shm_open(m_shm_timer_name.str().c_str(),
			O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1)
		NS_FATAL_ERROR("shm_open()" << strerror(errno));

	if ((m_shm_time = shm_open(m_shm_time_name.str().c_str(), O_RDWR | O_CREAT,
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

	if (m_traffic_time == NULL)
		m_traffic_time = (uint8_t *) mmap(NULL, m_time_size, PROT_READ | PROT_WRITE,
				MAP_SHARED, m_shm_time, 0);

	if ((m_sem_in = sem_open(m_sem_in_name.str().c_str(), O_CREAT | O_EXCL,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1)) == SEM_FAILED )
		NS_FATAL_ERROR(" ns -3 sem_open(sem_in) failed: " << strerror(errno));

	if ((m_sem_out = sem_open(m_sem_out_name.str().c_str(), O_CREAT | O_EXCL,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1)) == SEM_FAILED )
		NS_FATAL_ERROR("ns -3 sem_open(sem_out) failed: " << strerror(errno));

	if ((m_sem_time = sem_open(m_sem_time_name.str().c_str(), O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1)) == SEM_FAILED )
		NS_FATAL_ERROR("ns -3 sem_open(sem_time) failed: " << strerror(errno));
	if ((m_sem_timer = sem_open(m_sem_timer_name.str().c_str(), O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1)) == SEM_FAILED )
		NS_FATAL_ERROR("ns -3 sem_open(sem_timer) failed: " << strerror(errno));
	if ((m_sem_go = sem_open(m_sem_go_name.str().c_str(), O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)) == SEM_FAILED )
		NS_FATAL_ERROR("ns -3 sem_open(sem_go) failed: " << strerror(errno));

	if ((m_sem_done = sem_open(m_sem_done_name.str().c_str(), O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)) == SEM_FAILED )
		NS_FATAL_ERROR("ns -3 sem_open(sem_done) failed: " << strerror(errno));
	if ((m_sem_timer_go = sem_open(m_sem_timer_go_name.str().c_str(), O_CREAT,
			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)) == SEM_FAILED )
		NS_FATAL_ERROR("ns -3 sem_open(m_sem_timer_go) failed: " << strerror(errno));

	if ((m_sem_timer_done = sem_open(m_sem_timer_done_name.str().c_str(),
			O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)) == SEM_FAILED )
		NS_FATAL_ERROR("ns -3 sem_open(m_sem_timer_done) failed: " << strerror(errno));

}

void ContikiNetDevice::ClearIpc() {

	munmap(m_traffic_in, sizeof(size_t) + m_traffic_size);
	munmap(m_traffic_out, sizeof(size_t) + m_traffic_size);
	munmap(m_traffic_time, m_time_size);

	shm_unlink(m_shm_in_name.str().c_str());
	shm_unlink(m_shm_out_name.str().c_str());
	shm_unlink(m_shm_time_name.str().c_str());
	shm_unlink(m_shm_timer_name.str().c_str());

	sem_close(m_sem_in);
	sem_close(m_sem_out);
	sem_close(m_sem_time);
	sem_close(m_sem_timer);
	sem_close(m_sem_go);
	sem_close(m_sem_done);

	sem_unlink(m_sem_in_name.str().c_str());
	sem_unlink(m_sem_out_name.str().c_str());
	sem_unlink(m_sem_time_name.str().c_str());
	sem_unlink(m_sem_timer_name.str().c_str());
	sem_unlink(m_sem_go_name.str().c_str());
	sem_unlink(m_sem_done_name.str().c_str());

	NS_LOG_LOGIC("Cleared semaphores and Shared memories\n");
}

void ContikiNetDevice::ReadCallback(uint8_t *buf, ssize_t len) {
	NS_LOG_FUNCTION_NOARGS ();

	NS_ASSERT_MSG(buf != 0, "invalid buf argument");
	NS_ASSERT_MSG(len > 0, "invalid len argument");

	NS_LOG_INFO ("ContikiNetDevice::ReadCallback(): Received packet on node " << m_nodeId);NS_LOG_INFO ("ContikiNetDevice::ReadCallback(): Scheduling handler");
	Simulator::ScheduleWithContext(m_nodeId, Seconds(0.0),
			MakeEvent(&ContikiNetDevice::ForwardToBridgedDevice, this, buf,
					len));
}

void ContikiNetDevice::ForwardToBridgedDevice(uint8_t *buf, ssize_t len) {
	NS_LOG_FUNCTION (buf << len);

	//
	// First, create a packet out of the byte buffer we received and free that
	// buffer.
	//
	Ptr<Packet> packet = Create<Packet>(reinterpret_cast<const uint8_t *>(buf),
			len);
	free(buf);
	buf = 0;

	Address src, dst;
	uint16_t type;

	NS_LOG_LOGIC ("Received packet from socket");

	// Pull source, destination and type information from packet
	Ptr<Packet> p = Filter(packet, &src, &dst, &type);

	if (p == 0) {
		NS_LOG_LOGIC ("ContikiNetDevice::ForwardToBridgedDevice:  Discarding packet as unfit for ns-3 consumption");
		return;
	}

	NS_LOG_LOGIC ("Pkt source is " << src);NS_LOG_LOGIC ("Pkt destination is " << dst);NS_LOG_LOGIC ("Pkt LengthType is " << type);NS_LOG_LOGIC ("Forwarding packet from external socket to simulated network");

	if (m_mode == MACPHYOVERLAY) {
		if (m_ns3AddressRewritten == false) {
			//
			// Set the ns-3 device's mac address to the overlying container's
			// mac address
			//
			Mac48Address learnedMac = Mac48Address::ConvertFrom(src);
			NS_LOG_LOGIC ("Learned MacAddr is " << learnedMac << ": setting ns-3 device to use this address");
			m_bridgedDevice->SetAddress(Mac48Address::ConvertFrom(learnedMac));
			m_ns3AddressRewritten = true;
		}

		NS_LOG_LOGIC ("Forwarding packet to ns-3 device via Send()");
		m_bridgedDevice->Send(packet, dst, type);
		//m_bridgedDevice->SendFrom (packet, src, dst, type);
		return;
	} else {
		Address nullAddr = Address();
		m_bridgedDevice->Send(packet, nullAddr, uint16_t(0));
	}
}

Ptr<Packet> ContikiNetDevice::Filter(Ptr<Packet> p, Address *src, Address *dst,
		uint16_t *type) {
	NS_LOG_FUNCTION (p);
	/* Fill out src, dst and maybe type for the Send() function
	 *   This needs to be completed for MACOVERLAY mode to function - currently crashes because improper src/dst assigned */
	return p;
}

Ptr<NetDevice> ContikiNetDevice::GetBridgedNetDevice(void) {
	NS_LOG_FUNCTION_NOARGS ();
	return m_bridgedDevice;
}

void ContikiNetDevice::SetMac(Ptr<ContikiMac> mac) {
	m_macLayer = mac;
}

Ptr<ContikiMac> ContikiNetDevice::GetMac(void) {
	return m_macLayer;
}

void ContikiNetDevice::SetPhy(Ptr<ContikiPhy> phy) {
	m_phy = phy;
}

Ptr<ContikiPhy> ContikiNetDevice::GetPhy(void) {
	return m_phy;
}

void ContikiNetDevice::SetBridgedNetDevice(Ptr<NetDevice> bridgedDevice) {
	NS_LOG_FUNCTION (bridgedDevice);

	NS_ASSERT_MSG(m_node != 0,
			"ContikiNetDevice::SetBridgedDevice:  Bridge not installed in a node");
	//NS_ASSERT_MSG (bridgedDevice != this, "ContikiNetDevice::SetBridgedDevice:  Cannot bridge to self");
	NS_ASSERT_MSG(m_bridgedDevice == 0,
			"ContikiNetDevice::SetBridgedDevice:  Already bridged");

	/* Disconnect the bridged device from the native ns-3 stack
	 *  and branch to network stack on the other side of the socket. */
	bridgedDevice->SetReceiveCallback(
			MakeCallback(&ContikiNetDevice::DiscardFromBridgedDevice, this));
	bridgedDevice->SetPromiscReceiveCallback(
			MakeCallback(&ContikiNetDevice::ReceiveFromBridgedDevice, this));
	m_bridgedDevice = bridgedDevice;
}

bool ContikiNetDevice::DiscardFromBridgedDevice(Ptr<NetDevice> device,
		Ptr<const Packet> packet, uint16_t protocol, const Address &src) {
	NS_LOG_FUNCTION (device << packet << protocol << src);NS_LOG_LOGIC ("Discarding packet stolen from bridged device " << device);
	return true;
}

bool ContikiNetDevice::ReceiveFromBridgedDevice(Ptr<NetDevice> device,
		Ptr<const Packet> packet, uint16_t protocol, Address const &src,
		Address const &dst, PacketType packetType) {
	NS_LOG_DEBUG ("Packet UID is " << packet->GetUid ());
	/* Forward packet to socket */
	Ptr<Packet> p = packet->Copy();
	NS_LOG_LOGIC ("Writing packet to shared memory");
	p->CopyData(m_packetBuffer, p->GetSize());

	NS_LOG_UNCOND("NS-3 is writing for node " << child << " on sem " << m_sem_out_name.str().c_str() << "\n");

	int tmp = 0;

	while(tmp == 0)
	{
		sem_getvalue(m_sem_out, &tmp);
		NS_LOG_UNCOND("value of m_sem_out in node " << m_nodeId << " is " << tmp << "\n");
		//sleep(1);
	}


	if (sem_wait(m_sem_out) == -1)
		NS_FATAL_ERROR("sem_wait() failed: " << strerror(errno));

	//XXX Maybe check if p->GetSize() doesn't exceed m_traffic_size

	NS_LOG_UNCOND("NS-3 got sem_out for node " << child << "\n");

	size_t output_size = (size_t) p->GetSize();
	//writing traffic size first
	memcpy(m_traffic_out, &output_size, sizeof(size_t));

	//Now writing actual traffic
	void *retval = memcpy(m_traffic_out + sizeof(size_t), m_packetBuffer,
			p->GetSize());

	if (sem_post(m_sem_out) == -1)
		NS_FATAL_ERROR("sem_wait() failed: " << strerror(errno));

	NS_LOG_UNCOND("NS-3 wrote for node " << child << "\n");

	NS_ABORT_MSG_IF(retval == (void * ) -1,
			"ContikiNetDevice::ReceiveFromBridgedDevice(): memcpy() (AKA Write) error.");
	NS_LOG_LOGIC ("End of receive packet handling on node " << m_node->GetId ());
	return true;
}

void ContikiNetDevice::SetIfIndex(const uint32_t index) {
	NS_LOG_FUNCTION_NOARGS ();
	m_ifIndex = index;
}

uint32_t ContikiNetDevice::GetIfIndex(void) const {
	NS_LOG_FUNCTION_NOARGS ();
	return m_ifIndex;
}

Ptr<Channel> ContikiNetDevice::GetChannel(void) const {
	NS_LOG_FUNCTION_NOARGS ();
	return 0;
}

void ContikiNetDevice::SetAddress(Address address) {
	NS_LOG_FUNCTION (address);
	m_address = Mac64Address::ConvertFrom(address);
}

Address ContikiNetDevice::GetAddress(void) const {
	NS_LOG_FUNCTION_NOARGS ();
	return m_address;
}

void ContikiNetDevice::SetMode(std::string mode) {
	NS_LOG_FUNCTION (mode);
	if (mode.compare("MACPHYOVERLAY") == 0) {
		m_mode = (ContikiNetDevice::Mode) 2;
	} else if (mode.compare("PHYOVERLAY") == 0) {
		m_mode = (ContikiNetDevice::Mode) 1;
	} else {
		m_mode = (ContikiNetDevice::Mode) 0;
	}
}

void ContikiNetDevice::SetApplication(std::string application) {

	m_application = application;
}

ContikiNetDevice::Mode ContikiNetDevice::GetMode(void) {
	NS_LOG_FUNCTION_NOARGS ();
	return m_mode;
}

bool ContikiNetDevice::SetMtu(const uint16_t mtu) {
	NS_LOG_FUNCTION_NOARGS ();
	m_mtu = mtu;
	return true;
}

uint16_t ContikiNetDevice::GetMtu(void) const {
	NS_LOG_FUNCTION_NOARGS ();
	return m_mtu;
}

bool ContikiNetDevice::IsLinkUp(void) const {
	NS_LOG_FUNCTION_NOARGS ();
	return true;
}

void ContikiNetDevice::AddLinkChangeCallback(Callback<void> callback) {
	NS_LOG_FUNCTION_NOARGS ();
}

bool ContikiNetDevice::IsBroadcast(void) const {
	NS_LOG_FUNCTION_NOARGS ();
	return true;
}

Address ContikiNetDevice::GetBroadcast(void) const {
	NS_LOG_FUNCTION_NOARGS ();
	return Mac64Address("ff:ff:ff:ff:ff:ff:ff:ff");
}

bool ContikiNetDevice::IsMulticast(void) const {
	NS_LOG_FUNCTION_NOARGS ();
	return true;
}

Address ContikiNetDevice::GetMulticast(Ipv4Address multicastGroup) const {
	NS_LOG_FUNCTION (this << multicastGroup);
	Mac48Address multicast = Mac48Address::GetMulticast(multicastGroup);
	return multicast;
}

bool ContikiNetDevice::IsPointToPoint(void) const {
	NS_LOG_FUNCTION_NOARGS ();
	return false;
}

bool ContikiNetDevice::IsBridge(void) const {
	NS_LOG_FUNCTION_NOARGS ();
	//
	// Returning false from IsBridge in a device called ContikiNetDevice may seem odd
	// at first glance, but this test is for a device that bridges ns-3 devices
	// together.  The Tap bridge doesn't do that -- it bridges an ns-3 device to
	// a Linux device.  This is a completely different story.
	//
	return false;
}

bool ContikiNetDevice::Send(Ptr<Packet> packet, const Address& dest,
		uint16_t protocolNumber) {
	NS_LOG_FUNCTION (packet);
	/* Send to MAC Layer */
	m_macLayer->Enqueue(packet);
	return true;
}

bool ContikiNetDevice::SendFrom(Ptr<Packet> packet, const Address& src,
		const Address& dst, uint16_t protocol) {
	NS_LOG_FUNCTION (packet << src << dst << protocol);
	NS_FATAL_ERROR(
			"ContikiNetDevice::Send: You may not call SendFrom on a ContikiNetDevice directly");
	return true;
}

Ptr<Node> ContikiNetDevice::GetNode(void) const {
	NS_LOG_FUNCTION_NOARGS ();
	return m_node;
}

void ContikiNetDevice::SetNode(Ptr<Node> node) {
	NS_LOG_FUNCTION_NOARGS ();
	m_node = node;
}

bool ContikiNetDevice::NeedsArp(void) const {
	NS_LOG_FUNCTION_NOARGS ();
	return true;
}

void ContikiNetDevice::SetReceiveCallback(NetDevice::ReceiveCallback cb) {
	NS_LOG_FUNCTION_NOARGS ();
	m_rxCallback = cb;
}

void ContikiNetDevice::SetPromiscReceiveCallback(
		NetDevice::PromiscReceiveCallback cb) {
	NS_LOG_FUNCTION_NOARGS ();
	m_promiscRxCallback = cb;
}

bool ContikiNetDevice::SupportsSendFrom() const {
	NS_LOG_FUNCTION_NOARGS ();
	return true;
}

Address ContikiNetDevice::GetMulticast(Ipv6Address addr) const {
	NS_LOG_FUNCTION (this << addr);
	return Mac48Address::GetMulticast(addr);
}

} // namespace ns3
