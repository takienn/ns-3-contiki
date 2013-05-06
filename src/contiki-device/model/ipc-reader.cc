#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <stdio.h>

#include <sstream>

#include "ns3/log.h"
#include "ns3/fatal-error.h"
#include "ns3/simple-ref-count.h"
#include "ns3/system-thread.h"
#include "ns3/simulator.h"

#include "ipc-reader.h"

NS_LOG_COMPONENT_DEFINE("IpcReader");

namespace ns3 {

IpcReader::IpcReader() :
		m_traffic_in(NULL), m_nodeId(0), m_pid(0), m_readCallback(0), m_readThread(
				0), m_stop(false), m_destroyEvent() {
}

IpcReader::~IpcReader() {
	Stop();
}

/*
sem_t *IpcReader::m_sem_timer = NULL;
int IpcReader::m_shm_timer = 0;
void *IpcReader::m_traffic_timer = NULL;
*/

void IpcReader::Start(
		Callback<void, char *, ssize_t> readCallback, uint32_t nodeId,
		pid_t pid) {

	NS_ASSERT_MSG(m_readThread == 0, "ipc read thread already exists");
	//NS_ASSERT_MSG (m_writeThread == 0, "ipc write thread already exists");

	m_readCallback = readCallback;
	m_nodeId = nodeId;
	m_pid = pid;

	//
	// We're going to spin up a thread soon, so we need to make sure we have
	// a way to tear down that thread when the simulation stops.  Do this by
	// scheduling a "destroy time" method to make sure the thread exits before
	// proceeding.
	//
	if (!m_destroyEvent.IsRunning()) {
		// hold a reference to ensure that this object is not
		// deallocated before the destroy-time event fires
		this->Ref();
		m_destroyEvent = Simulator::ScheduleDestroy(&IpcReader::DestroyEvent,
				this);
	}

	//
	// Now spin up a thread to read from the fd
	//
	NS_LOG_LOGIC ("Spinning up read thread");

	m_readThread = Create<SystemThread>(MakeCallback(&IpcReader::Run, this));
	m_readThread->Start();
}

void IpcReader::DestroyEvent(void) {
	Stop();
	this->Unref();
}

void IpcReader::Stop(void) {
	m_stop = true;

	// join the read thread
	if (m_readThread != 0) {
		m_readThread->Join();
		m_readThread = 0;
	}

	// reset everything else
	m_traffic_in = NULL;
	m_readCallback.Nullify();
	m_stop = false;
}

// This runs in a separate thread
void IpcReader::Run(void) {

	m_shm_in_name << "/ns_contiki_traffic_in_" << m_nodeId;
	m_shm_timer_name << "/ns_contiki_traffic_timer_";// << m_nodeId;

	m_sem_in_name << "/ns_contiki_sem_in_" << m_nodeId;
	m_sem_timer_name << "/ns_contiki_sem_timer_";// << m_nodeId;

	size_t m_traffic_size = 65536,
			m_time_size = 8;

	if ((m_shm_in = shm_open(m_shm_in_name.str().c_str(), O_RDWR, 0)) == -1)
			perror("thread shm_open(shm_in) error");

	if ((m_shm_timer = shm_open(m_shm_timer_name.str().c_str(), O_RDWR, 0)) == -1)
				perror("thread shm_open(shm_timer)");

	if ((m_sem_in = sem_open(m_sem_in_name.str().c_str(), 0)) == SEM_FAILED)
		perror("thread sem_open(m_sem_in) error");

	if ((m_sem_timer = sem_open(m_sem_timer_name.str().c_str(), 0)) == SEM_FAILED)
		perror("thread sem_open(m_sem_timer) error");


	m_traffic_in = (char *)mmap(NULL,
						m_traffic_size,
						PROT_READ | PROT_WRITE,
						MAP_SHARED,
						m_shm_in,
						0);

	m_traffic_timer = (char *)mmap(NULL,
						m_time_size,
						PROT_READ | PROT_WRITE,
						MAP_SHARED,
						m_shm_timer,
						0);

	for (;;) {

		if (m_stop) {
			// this thread is done
			break;
		}

		/////////////////////////////////////////////////////////

		// Checking if contiki requested to schedule a timer
		uint64_t timerval = 0;

		if (sem_wait(m_sem_timer) == -1)
			NS_FATAL_ERROR("sem_wait(): error" << strerror (errno));

		memcpy(&timerval, m_traffic_timer, 8);

		memset(m_traffic_timer, 0, 8);

		if (sem_post(m_sem_timer) == -1)
			NS_FATAL_ERROR("sem_post(): error" << strerror (errno));

		if(timerval > 0)
			SetTimer(timerval, 0);

		//////////////////////////////////////////////////////////

		// Processing traffic sent by contiki

		struct IpcReader::Data data = DoRead();
		// reading stops when m_len is zero
		if (data.m_len == 0) {
			break;
		}
		// the callback is only called when m_len is positive (data
		// is ignored if m_len is negative)
		else if (data.m_len > 0) {
			m_readCallback(data.m_buf, data.m_len);
		}

	}
}

void IpcReader::CheckTimer(void) {
}

void IpcReader::SetTimer(uint64_t time, int type) {

	void (*f)(void) = 0;

	Simulator::ScheduleWithContext(m_nodeId,NanoSeconds(time), f);
	//Simulator::ScheduleWithContext(m_nodeId,NanoSeconds(time), &IpcReader::SendAlarm, this);
}

void IpcReader::SendAlarm(void) {
	if (kill(m_pid, SIGALRM) == -1)
		NS_FATAL_ERROR("kill(SIGALRM) failed: " << strerror(errno));
}

} // namespace ns3
