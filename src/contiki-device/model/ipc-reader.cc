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
#include "contiki-device.h"

NS_LOG_COMPONENT_DEFINE("IpcReader");

namespace ns3 {
#ifndef DATA_TYPE
const uint8_t DATA_TYPE = 0;
const uint8_t TIMER_TYPE = 1;
//#define DATA_TYPE 0
//#define TIMER_TYPE 1
#endif
//#ifndef DATA_TYPE
//#define DATA_TYPE 0
//#define TIMER_TYPE 1
//#endif

std::map<uint32_t, sem_t*> IpcReader::listOfGoSemaphores;
std::map<uint32_t, sem_t*> IpcReader::listOfDoneSemaphores;
std::map<Time, std::list<uint32_t> > IpcReader::nodesToWakeUp;
SystemMutex IpcReader::m_controlNodesToWakeUp;
sem_t IpcReader::m_sem_time;
int IpcReader::m_time_Memory_Id;

size_t IpcReader::m_traffic_size = 1500;
size_t IpcReader::m_time_size = 8;
int* IpcReader::m_shm_time = NULL;

IpcReader::IpcReader() :
		m_nodeId(0), m_pid(0), m_readCallback(0), m_readThread(0), m_stop(
				false), m_destroyEvent() {

}

IpcReader::~IpcReader() {
	Stop();
}

void* alocateSharedMemory(int uniqueKey, int sizeOfMemory, int *id) {
	//NS_ASSERT_MSG (m_writeThread == 0, "ipc write thread already exists");
	int shMemSegID; // shared memory segment id
	void* shMemSeg; //ptr to shared memory segment

	// gets the memory segment
	if ((shMemSegID = shmget((key_t)(uniqueKey), sizeOfMemory,
			IPC_CREAT | SHM_R | SHM_W)) < 0) {
		perror("shmget error \n");
	}
	if ((shMemSeg = shmat(shMemSegID, NULL, 0)) == (void*) ((-1))) {
		perror("shmat error \n");
		exit (EXIT_FAILURE);
	}
	*id = shMemSegID;
	return shMemSeg;
}

void IpcReader::initIpc() {
	// Allocates structure space to share among processes
	int id = 0;
	void* shMemSeg = alocateSharedMemory(
			(START_CONST + (m_nodeId * 10) + MAIN_SH_SRTUCTURE),
			sizeof(semaphores_t), &id);
	sharedSemaphores = (semaphores_t*) ((shMemSeg));
	sharedSemaphores->id = id;

	// ****************
	// Alocate shared memory segments
	// ****************
	shMemSeg = alocateSharedMemory((START_CONST + (m_nodeId * 10) + TRAFFIC_IN),
			m_traffic_size + sizeof(size_t), &sharedSemaphores->traffic_in_id);
	sharedSemaphores->traffic_in = (unsigned char*) ((shMemSeg));

	shMemSeg = alocateSharedMemory((START_CONST + (m_nodeId * 10) + TIMER_IN),
			m_time_size + 1, &sharedSemaphores->traffic_timer_id);
	sharedSemaphores->traffic_timer = (unsigned char*) ((shMemSeg));

	shMemSeg = alocateSharedMemory(
			(START_CONST + (m_nodeId * 10) + TRAFFIC_OUT),
			sizeof(size_t) + m_traffic_size + sizeof(size_t),
			&sharedSemaphores->traffic_out_id);
	sharedSemaphores->traffic_out = (unsigned char*) ((shMemSeg));

	// Special case the time, that is unique!
	// Instantiates only once, it is a shared memory
	if (m_shm_time == NULL) {
		shMemSeg = alocateSharedMemory((START_CONST + (m_nodeId * 10) + TIME),
				m_time_size, &m_time_Memory_Id);
		m_shm_time = (int *) (shMemSeg);

		if (sem_init(&m_sem_time, 1, 1))
			NS_FATAL_ERROR(
					"ns -3 sem_init(sharedSemaphores->sem_time) failed: " << strerror(errno));
	}

	// saves the static values of the semaphores on the specific node register
	sharedSemaphores->sem_time = m_sem_time;
	sharedSemaphores->shm_time = m_shm_time;
	sharedSemaphores->shm_time_id = m_time_Memory_Id;

	if (sem_init(&sharedSemaphores->sem_in, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_in) failed: " << strerror(errno));

	if (sem_init(&sharedSemaphores->sem_out, 1, 1))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_out) failed: " << strerror(errno));

	if (sem_init(&sharedSemaphores->sem_timer, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_timer) failed: " << strerror(errno));

	if (sem_init(&sharedSemaphores->sem_timer_done, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_timer_done) failed: " << strerror(errno));

	if (sem_init(&sharedSemaphores->sem_timer_go, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_timer_go) failed: " << strerror(errno));

	if (sem_init(&sharedSemaphores->sem_traffic_go, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_traffic_go) failed: " << strerror(errno));

	if (sem_init(&sharedSemaphores->sem_traffic_done, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_traffic_done) failed: " << strerror(errno));

	if (sem_init(&sharedSemaphores->sem_go, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_go) failed: " << strerror(errno));

	// saves the value into the global list of go semaphores
	listOfGoSemaphores.insert(
			std::pair<uint32_t, sem_t*>(m_nodeId, &sharedSemaphores->sem_go));

	if (sem_init(&sharedSemaphores->sem_done, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_done) failed: " << strerror(errno));

	// saves the value into the global  list of done semaphores
	listOfDoneSemaphores.insert(
			std::pair<uint32_t, sem_t*>(m_nodeId, &sharedSemaphores->sem_done));
}

semaphores_t * IpcReader::Start(Callback<void, uint8_t*, ssize_t> readCallback,
		uint32_t nodeId, pid_t pid) {
	NS_ASSERT_MSG(m_readThread == 0, "ipc read thread already exists");
	//NS_ASSERT_MSG (m_writeThread == 0, "ipc write thread already exists");
	m_readCallback = readCallback;
	m_nodeId = nodeId;
	m_pid = pid;

	// Allocates structure and initializes semaphores and shared memory to share
	// among processes
	initIpc();

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
	;
	setSchedule(MilliSeconds(0), nodeId);
	m_readThread = Create<SystemThread>(MakeCallback(&IpcReader::Run, this));
	m_readThread->Start();
	return sharedSemaphores;
}

void IpcReader::DestroyEvent(void) {
	Stop();
	this->Unref();
}

void IpcReader::Stop(void) {
	m_stop = true;
	// join the read thread
	if (m_readThread != 0) {
		if (sem_post(&sharedSemaphores->sem_traffic_done) == -1)
			NS_FATAL_ERROR("sem_wait(): error" << strerror (errno));
		else
			m_readThread->Join();
		m_readThread = 0;

	}

	// Cleans the sem_time, a special case
	// Free alocated Shared memory
	if(m_shm_time != NULL){
		sem_destroy(&sharedSemaphores->sem_time);
		shmdt(&m_shm_time);
		shmdt(&sharedSemaphores->shm_time);
		shmctl(sharedSemaphores->shm_time_id, IPC_RMID, 0);
		sharedSemaphores->shm_time_id = -1;
	}

	// Free semaphores
	sem_destroy(&sharedSemaphores->sem_in);
	sem_destroy(&sharedSemaphores->sem_out);
	sem_destroy(&sharedSemaphores->sem_timer);
	sem_destroy(&sharedSemaphores->sem_timer_go);
	sem_destroy(&sharedSemaphores->sem_timer_done);
	sem_destroy(&sharedSemaphores->sem_traffic_go);
	sem_destroy(&sharedSemaphores->sem_traffic_done);
	sem_destroy(&sharedSemaphores->sem_go);
	sem_destroy(&sharedSemaphores->sem_done);


	shmdt(&sharedSemaphores->traffic_in);
	shmctl(sharedSemaphores->traffic_in_id, IPC_RMID, 0);

	shmdt(&sharedSemaphores->traffic_timer);
	shmctl(sharedSemaphores->traffic_timer_id, IPC_RMID, 0);

	shmdt(&sharedSemaphores->traffic_out);
	shmctl(sharedSemaphores->traffic_out_id, IPC_RMID, 0);

	int id = sharedSemaphores->id;
	shmdt(&sharedSemaphores);
	shmctl(id, IPC_RMID, 0);


	// reset everything else
	m_readCallback.Nullify();
	m_stop = false;


	NS_LOG_LOGIC("Cleared semaphores and Shared memories\n");
}


// This runs in a separate thread
void IpcReader::Run(void) {

	// Never ending loop to wait for Contiki requests for new packets and timers
	for (;;) {

		/////////////////////////////////////////////////////////
		// Checking if contiki requested to schedule a timer
		uint8_t typeOfInfo = 0;
		uint64_t timerval = 0;
		uint8_t timertype = 0;

		sem_post(&sharedSemaphores->sem_in);

		if (sem_wait(&sharedSemaphores->sem_traffic_done) == -1)
			NS_FATAL_ERROR("sem_wait(): error" << strerror (errno));

		if (m_stop) {
			// this thread is done
			break;
		}

		// Finds out what is in the pipe (Data or timer request)
		memcpy(&typeOfInfo, sharedSemaphores->traffic_in, 1);

		// 0 = Data transfer
		// 1 = Timer
		if (typeOfInfo == TIMER_TYPE) {

			NS_LOG_LOGIC("contiki requested a timer\n" << m_nodeId);

			NS_LOG_LOGIC(" ---Wait ipcReader  sharedSemaphores->sem_timer Run at "
					<< m_nodeId << " \n");

			memcpy(&timertype, sharedSemaphores->traffic_in + 1, 1);
			memcpy(&timerval, sharedSemaphores->traffic_in + 2, 8);

			memset(sharedSemaphores->traffic_in, 0, 10);

			NS_LOG_LOGIC("contiki's timer handled " << m_nodeId);
			NS_LOG_LOGIC("releasing contiki after timer request " << m_nodeId);

			if (timertype != 0 && timertype != 1)
				NS_FATAL_ERROR("wrong timertype " << timertype);

			if (timerval > 0) {
				SetTimer(timerval, timertype);
			}

		} else {

			//////////////////////////////////////////////////////////

			// Processing traffic sent by contiki
			uint8_t* buf = (uint8_t*) ((malloc(m_traffic_size)));
			memset(buf, 0, m_traffic_size);
			NS_ABORT_MSG_IF(buf == 0, "malloc() failed");
			size_t input_size = 0;

			NS_LOG_LOGIC("ns3 reading " << m_nodeId);
			// First read input size
			memcpy(&input_size, sharedSemaphores->traffic_in + 1,
					sizeof(size_t));

			// Now read input
			memcpy(buf, sharedSemaphores->traffic_in + sizeof(size_t) + 1,
					input_size);
			memset(sharedSemaphores->traffic_in, 0,
					m_traffic_size + sizeof(size_t));

			if (input_size == 0) {
				//NS_LOG_LOGIC ("ContikiNetDeviceFdReader::DoRead(): done" << m_nodeId);
				free(buf);
				buf = 0;
				//len = 0;
			}

			// the callback is only called when m_len is positive (data
			// is ignored if m_len is negative)
			if (input_size > 0) {
				NS_LOG_LOGIC("read data of length " << input_size);
				m_readCallback(buf, input_size);
			} else
				NS_LOG_INFO("read data of size 0");

		}

		// Remove this step could increase performance, but could happen that the contiki
		// machine would be released BEFORE the ns-3 thread have the time to
		// process the message, what could cause problems afterwards with missing
		// messages
		std::cout
				<< " ---Post ipcReader  sharedSemaphores->sem_traffic_go Run at "
				<< m_nodeId << " \n";
		sem_post(&sharedSemaphores->sem_traffic_go);
		std::cout
				<< " ---Post ipcReader  sharedSemaphores->sem_traffic_go Run at "
				<< m_nodeId << " \n";

	}
}

void IpcReader::CheckTimer(void) {
}

void IpcReader::SendAlarm(void) {
	if (kill(m_pid, SIGALRM) == -1)
		NS_FATAL_ERROR("kill(SIGALRM) failed: " << strerror(errno));
}

void IpcReader::SetRelativeTimer() {
	void (*f)(void) = 0;
	Time incrementTime = NanoSeconds(1);
	Simulator::ScheduleWithContext(m_nodeId, incrementTime, f);
	setSchedule((incrementTime + Simulator::Now()), m_nodeId);
}

void IpcReader::SetTimer(Time time, int type) {
	if (type == 0) {
		void (*f)(void) = 0;
		//XXX schedule 1 millisecond after the requested time
		// so that contiki timers can expire
		Simulator::ScheduleWithContext(m_nodeId, time, f);
	} else {
		Simulator::ScheduleWithContext(m_nodeId, time, &IpcReader::SendAlarm,
				this);
	};
	// registers the event
//	Time timeToShedule = (time + Simulator::Now());
	setSchedule(time, m_nodeId);
}

void IpcReader::SetTimer(uint64_t time, int type) {
	SetTimer(MilliSeconds(time), type);
}

void IpcReader::setSchedule(Time time, uint32_t nodeId) {
	CriticalSection cs(m_controlNodesToWakeUp);
	// adds the node id to the specific time scheduling list
	std::cout << " --- #### To schedule: " << time << " : " << nodeId << "\n";
	if (nodesToWakeUp.find(time) != nodesToWakeUp.end()) {
		nodesToWakeUp[time].push_back(nodeId);
		nodesToWakeUp[time].unique();
	} else {
		// it is the first node to be added to this time schedule
		std::list < uint32_t > scheduledNodes;
		scheduledNodes.push_back(nodeId);
		std::pair<std::map<Time, std::list<uint32_t> >::iterator, bool> ret;
		ret = nodesToWakeUp.insert(
				std::pair<Time, std::list<uint32_t> >(time, scheduledNodes));
		if (ret.second == false) {
			NS_LOG_ERROR("Could not register scheduled event");
		}

	}

}

std::list<uint32_t> IpcReader::getReleaseSchedule(Time time) {

	CriticalSection cs(m_controlNodesToWakeUp);
	std::list < uint32_t > returnValue;

	std::map<Time, std::list<uint32_t> >::iterator it;

	// Return all that should have been awaken prior than the present time
	// Waits for the nodes to finnish their jobs
	for (it = nodesToWakeUp.begin();
			it != nodesToWakeUp.end() && it->first <= time; it++) {
		if (it->first <= time) {
			(it->second).sort();
			returnValue.merge(it->second);
		}
	}

	// Clean the lists
	if (returnValue.size() > 0) {
		nodesToWakeUp.erase(nodesToWakeUp.begin(), it);

		// Remove the possible duplicated node Ids from the list
		returnValue.unique();
	}

	return returnValue;
}

} // namespace ns3
