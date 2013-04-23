#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <semaphore.h>

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
	m_evpipe[0] = -1;
	m_evpipe[1] = -1;
}

IpcReader::~IpcReader() {
	Stop();
}

void IpcReader::Start(void *addr,
		Callback<void, uint8_t *, ssize_t> readCallback, uint32_t nodeId,
		pid_t pid) {
	int tmp;

	NS_ASSERT_MSG(m_readThread == 0, "ipc read thread already exists");
	//NS_ASSERT_MSG (m_writeThread == 0, "ipc write thread already exists");

	// create a pipe for inter-thread event notification
	tmp = pipe(m_evpipe);
	if (tmp == -1) {
		NS_FATAL_ERROR("pipe() failed: " << strerror (errno));
	}

	// make the read end non-blocking
	tmp = fcntl(m_evpipe[0], F_GETFL);
	if (tmp == -1) {
		NS_FATAL_ERROR("fcntl() failed: " << strerror (errno));
	}
	if (fcntl(m_evpipe[0], F_SETFL, tmp | O_NONBLOCK) == -1) {
		NS_FATAL_ERROR("fcntl() failed: " << strerror (errno));
	}

	m_traffic_in = addr;
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

	// signal the read thread
	if (m_evpipe[1] != -1) {
		char zero = 0;
		ssize_t len = write(m_evpipe[1], &zero, sizeof(zero));
		if (len != sizeof(zero))
			NS_LOG_WARN ("incomplete write(): " << strerror (errno));
	}

	// join the read thread
	if (m_readThread != 0) {
		m_readThread->Join();
		m_readThread = 0;
	}

	// close the write end of the event pipe
	if (m_evpipe[1] != -1) {
		close(m_evpipe[1]);
		m_evpipe[1] = -1;
	}

	// close the read end of the event pipe
	if (m_evpipe[0] != -1) {
		close(m_evpipe[0]);
		m_evpipe[0] = -1;
	}

	// reset everything else
	m_traffic_in = NULL;
	m_readCallback.Nullify();
	m_stop = false;
}

// This runs in a separate thread
void IpcReader::Run(void) {

	int nfds = m_evpipe[0] + 1;
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(m_evpipe[0], &rfds);

	m_shm_in_name << "/ns_contiki_traffic_in_" << m_nodeId;
	m_shm_timer_name << "/ns_contiki_traffic_timer" << m_nodeId;

	m_sem_in_name << "/ns_contiki_sem_in_" << m_nodeId;
	m_sem_timer_name << "/ns_contiki_sem_timer_" << m_nodeId;

	size_t m_traffic_size = 65536;

	if ((m_shm_in = shm_open(m_shm_in_name.str().c_str(), O_RDONLY, 0)) == -1)
			perror("shm_open(shm_in)");

	if ((m_shm_timer = shm_open(m_shm_timer_name.str().c_str(), O_RDONLY, 0)) == -1)
				perror("shm_open(shm_in)");

	for (;;) {

		int r;
		fd_set readfds = rfds;

		r = select(nfds, &readfds, NULL, NULL, NULL);
		if (r == -1 && errno != EINTR) {
			NS_FATAL_ERROR("select() failed: " << strerror (errno));
		}

		if (FD_ISSET (m_evpipe[0], &readfds)) {
			// drain the event pipe
			for (;;) {
				char buf[1024];
				ssize_t len = read(m_evpipe[0], buf, sizeof(buf));
				if (len == 0) {
					NS_FATAL_ERROR("event pipe closed");
				}
				if (len < 0) {
					if (errno == EAGAIN || errno == EINTR
							|| errno == EWOULDBLOCK) {
						break;
					} else {
						NS_FATAL_ERROR("read() failed: " << strerror (errno));
					}
				}
			}
		}

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

		if (sem_wait(m_sem_in) == -1)
			NS_FATAL_ERROR("sem_wait(): error" << strerror (errno));
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

		if (sem_post(m_sem_in) == -1)
			NS_FATAL_ERROR("sem_post() failed: " << strerror (errno));

	}
}

void IpcReader::CheckTimer(void) {
}

void IpcReader::SetTimer(uint64_t time, int type) {
	switch (type)
	{
		case 0: Simulator::Schedule(NanoSeconds(time), NULL);
		case 1: Simulator::Schedule(NanoSeconds(time), &IpcReader::SendAlarm, this);
		default: NS_FATAL_ERROR("type = " << type << ". Invalid type"); break;
	}
}

void IpcReader::SendAlarm(void) {
	if (kill(m_pid, SIGALRM) == -1)
		NS_FATAL_ERROR("kill(SIGALRM) failed: " << strerror(errno));
}

} // namespace ns3
