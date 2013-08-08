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
#define DATA_TYPE 0
#define TIMER_TYPE 1
#endif

std::map<uint32_t, sem_t*> IpcReader::listOfGoSemaphores;
std::map<uint32_t, sem_t*> IpcReader::listOfDoneSemaphores;
std::map<Time, std::list<uint32_t> > IpcReader::nodesToWakeUp;
SystemMutex IpcReader::m_controlNodesToWakeUp;
sem_t IpcReader::m_sem_time;
int IpcReader::m_time_Memory_Id;

size_t IpcReader::m_traffic_size = 1300;
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

//	shMemSeg = alocateSharedMemory((START_CONST + (m_nodeId*10) + IN),
//			m_traffic_size + sizeof(size_t), &sharedSemaphores->shm_in_id);
//	sharedSemaphores->shm_in = (int*) ((shMemSeg));

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

	//
	//	// Preparing shm/sem names
	//	m_shm_in_name << "/ns_contiki_traffic_in_" << m_nodeId;
	//	m_shm_out_name << "/ns_contiki_traffic_out_" << m_nodeId;
	//	m_shm_timer_name << "/ns_contiki_traffic_timer_" << m_nodeId;
	//	m_shm_time_name << "/ns_contiki_traffic_time_";// << m_nodeId;
	//
	//	//Assuring there are no shm/sem leftovers from previous executions
	//	//ClearIpc();
	//Now Assuring the creation of ALL shm/sem objects but traffic_in and traffic_timer
	// are left for the read thread (where they are used) to map.
	//	if ((m_shm_in = shm_open(m_shm_in_name.str().c_str(),
	//			O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))
	//			== -1)
	//		NS_FATAL_ERROR("shm_open(m_shm_in) " << strerror(errno));
	//
	//	if ((m_shm_out = shm_open(m_shm_out_name.str().c_str(),
	//			O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH))
	//			== -1)
	//		NS_FATAL_ERROR("shm_open(m_shm_out)" << strerror(errno));
	//
	//	if ((m_shm_timer = shm_open(m_shm_timer_name.str().c_str(),
	//			O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1)
	//		NS_FATAL_ERROR("shm_open()" << strerror(errno));
	//
	//	if ((m_shm_time = shm_open(m_shm_time_name.str().c_str(), O_RDWR | O_CREAT,
	//			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1)
	//		NS_FATAL_ERROR("shm_open(m_shm_time) " << strerror(errno));
	//
	//	if (ftruncate(m_shm_in, m_traffic_size + sizeof(size_t)) == -1)
	//		NS_FATAL_ERROR("ftruncate(m_shm_in) " << strerror(errno));
	//
	//	if (ftruncate(m_shm_out, m_traffic_size + sizeof(size_t)) == -1)
	//		NS_FATAL_ERROR("shm_open(m_shm_out) " << strerror(errno));
	//
	//	if (ftruncate(m_shm_time, m_time_size) == -1)
	//		NS_FATAL_ERROR("ftruncate(m_shm_time)" << strerror(errno));
	//
	//	if (ftruncate(m_shm_timer, m_time_size + 1) == -1)
	//		NS_FATAL_ERROR("ftruncate(m_shm_timer)" << strerror(errno));
	//
	//	/* XXX A note here: not just data is trafered but also data size
	//	 * it is saved in sizeof(size_t) memory.
	//	 */
	//
	//	m_traffic_out = (uint8_t *) mmap(NULL,
	//			sizeof(size_t) + m_traffic_size + sizeof(size_t),
	//			PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_out, 0);
	//
	//	if (m_traffic_time == NULL)
	//		m_traffic_time = (uint8_t *) mmap(NULL, m_time_size, PROT_READ | PROT_WRITE,
	//				MAP_SHARED, m_shm_time, 0);
	//	m_shm_in_name << "/ns_contiki_traffic_in_" << m_nodeId;
	//	m_shm_timer_name << "/ns_contiki_traffic_timer_" << m_nodeId;
	//	m_sem_in_name << "/ns_contiki_sem_in_" << m_nodeId;
	//	m_sem_timer_name << "/ns_contiki_sem_timer_" << m_nodeId;
	//	m_sem_timer_go_name << "/ns_contiki_sem_timer_go_" << m_nodeId;
	//	m_sem_timer_done_name << "/ns_contiki_sem_timer_done_" << m_nodeId;
	//	m_sem_traffic_go_name << "/ns_contiki_sem_traffic_go_" << m_nodeId;
	//	m_sem_traffic_done_name << "/ns_contiki_sem_traffic_done_" << m_nodeId;
	//
	//	if ((m_shm_in = shm_open(m_shm_in_name.str().c_str(), O_RDWR, 0)) == -1)
	//		NS_FATAL_ERROR("thread shm_open(shm_in) error" << strerror(errno));
	//
	//	if ((m_shm_timer = shm_open(m_shm_timer_name.str().c_str(), O_RDWR, 0))
	//			== -1)
	//		NS_FATAL_ERROR("thread shm_open(shm_timer)" << strerror(errno));
	//
	//	m_traffic_in = (uint8_t*) (mmap(NULL, m_traffic_size + sizeof(size_t),
	//			PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_in, 0));
	//	m_traffic_timer = (uint8_t*) (mmap(NULL, m_time_size + 1,
	//			PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_timer, 0));
	// initializes the time semaphore just once we just need one instance

	// ****************
	// initialize semaphores
	// ****************

	//	if ((m_sem_in = sem_open(m_sem_in_name.str().c_str(), O_CREAT | O_EXCL,
	//			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1)) == SEM_FAILED )
	//		NS_FATAL_ERROR(" ns -3 sem_open(sem_in) failed: " << strerror(errno));
	if (sem_init(&sharedSemaphores->sem_in, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_in) failed: " << strerror(errno));

	//	if ((m_sem_out = sem_open(m_sem_in_name.str().c_str(), O_CREAT | O_EXCL,
	//			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1)) == SEM_FAILED )
	//		NS_FATAL_ERROR(" ns -3 sem_open(sem_in) failed: " << strerror(errno));
	if (sem_init(&sharedSemaphores->sem_out, 1, 1))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_out) failed: " << strerror(errno));

	//	if ((m_sem_timer = sem_open(m_sem_timer_name.str().c_str(), O_CREAT,
	//			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1)) == SEM_FAILED )
	//		NS_FATAL_ERROR("ns -3 sem_open(sem_timer) failed: " << strerror(errno));
	if (sem_init(&sharedSemaphores->sem_timer, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_timer) failed: " << strerror(errno));

	//	if ((m_sem_timer_done = sem_open(m_sem_timer_done_name.str().c_str(),
	//			O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)) == SEM_FAILED )
	//		NS_FATAL_ERROR("ns -3 sem_open(m_sem_timer_done) failed: " << strerror(errno));
	if (sem_init(&sharedSemaphores->sem_timer_done, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_timer_done) failed: " << strerror(errno));

	//	if ((m_sem_timer_go = sem_open(m_sem_timer_done_name.str().c_str(),
	//			O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)) == SEM_FAILED )
	//		NS_FATAL_ERROR("ns -3 sem_open(m_sem_timer_done) failed: " << strerror(errno));
	if (sem_init(&sharedSemaphores->sem_timer_go, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_timer_go) failed: " << strerror(errno));

	//	if ((m_sem_traffic_go = sem_open(m_sem_traffic_go_name.str().c_str(),
	//			O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)) == SEM_FAILED )
	//		NS_FATAL_ERROR(
	//				"ns -3 sem_open(m_sem_traffic_go) failed: " << strerror(errno));
	if (sem_init(&sharedSemaphores->sem_traffic_go, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_traffic_go) failed: " << strerror(errno));

	//	if ((m_sem_traffic_done = sem_open(m_sem_traffic_done_name.str().c_str(),
	//			O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 0)) == SEM_FAILED )
	//		NS_FATAL_ERROR(
	//				"ns -3 sem_open(m_sem_traffic_done_name) failed: " << strerror(errno));
	if (sem_init(&sharedSemaphores->sem_traffic_done, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_traffic_done) failed: " << strerror(errno));

	/// Main loop go and done semaphores
	//	if ((m_sem_go = sem_open(m_sem_go_name.str().c_str(), O_CREAT,
	//			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1)) == SEM_FAILED )
	if (sem_init(&sharedSemaphores->sem_go, 1, 0))
		NS_FATAL_ERROR(
				"ns -3 sem_init(sharedSemaphores->sem_go) failed: " << strerror(errno));

	// saves the value into the global list of go semaphores
	listOfGoSemaphores.insert(
			std::pair<uint32_t, sem_t*>(m_nodeId, &sharedSemaphores->sem_go));

	//	if ((m_sem_done = sem_open(m_sem_done_name.str().c_str(), O_CREAT,
	//			S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, 1)) == SEM_FAILED )
	//		NS_FATAL_ERROR("ns -3 sem_open(sem_done) failed: " << strerror(errno));
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
	if(sharedSemaphores->shm_time_id != -1){
		sem_destroy(&sharedSemaphores->sem_time);
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

//	shmdt(sharedSemaphores->shm_in);
//	shmctl(sharedSemaphores->shm_in_id, IPC_RMID, 0);


	int id = sharedSemaphores->id;
	shmdt(&sharedSemaphores);
	shmctl(id, IPC_RMID, 0);


	// reset everything else
	m_readCallback.Nullify();
	m_stop = false;


	NS_LOG_LOGIC("Cleared semaphores and Shared memories\n");
}

//IpcReader::Data IpcReader::DoRead(void) {
//	uint32_t bufferSize = 1300;
//	uint8_t* buf = (uint8_t*) ((malloc(bufferSize)));
//	memset(buf, 0, bufferSize);
//	NS_ABORT_MSG_IF(buf == 0, "malloc() failed");
//	size_t input_size = 0;
//
//	NS_LOG_LOGIC("ns3 reading " << m_nodeId);
//	// First read input size
//	memcpy(&input_size, sharedSemaphores->traffic_in+1, sizeof(size_t));
//
//	// Now read input
//	memcpy(buf, sharedSemaphores->traffic_in + sizeof(size_t)+1, input_size);
//	memset(sharedSemaphores->traffic_in, 0, bufferSize + sizeof(size_t)+1);
//
//	if (input_size == 0) {
//		//NS_LOG_LOGIC ("ContikiNetDeviceFdReader::DoRead(): done" << m_nodeId);
//		free(buf);
//		buf = 0;
//		//len = 0;
//	}
//	if (input_size > 0)
//		NS_LOG_LOGIC("read data of length " << input_size);
//
//	return IpcReader::Data(buf, input_size);
//}

//IpcReader::Data IpcReader::DoRead(void) {
//	uint32_t bufferSize = 1300;
//	uint8_t* buf = (uint8_t*) ((malloc(bufferSize)));
//	memset(buf, 0, bufferSize);
//	NS_ABORT_MSG_IF(buf == 0, "malloc() failed");
//	size_t input_size = 0;
//	fflush (stdout);
//	int rtval;
//	sem_getvalue(&sharedSemaphores->sem_traffic_done, &rtval);
//	if (rtval == 1) {
//
//		NS_LOG_LOGIC("contiki wrote something " << m_nodeId);
//
//		std::cout << " ---Wait Device  sharedSemaphores->sem_in DoRead at "
//				<< m_nodeId << " \n";
//		if (sem_wait(&sharedSemaphores->sem_in) == -1)
//			NS_FATAL_ERROR("sem_wait() failed: " << strerror(errno));
//		std::cout << " ---Wait Device  sharedSemaphores->sem_in DoRead at "
//				<< m_nodeId << " \n";
//
//		NS_LOG_LOGIC("ns3 reading " << m_nodeId);
//		// First read input size
//		memcpy(&input_size, sharedSemaphores->traffic_in, sizeof(size_t));
//
//		// Now read input
//		memcpy(buf, sharedSemaphores->traffic_in + sizeof(size_t), input_size);
//		memset(sharedSemaphores->traffic_in, 0, bufferSize + sizeof(size_t));
//
//		NS_LOG_LOGIC("ns3 read " << m_nodeId);
//
//		NS_LOG_LOGIC("ns3 releasing contiki after read " << m_nodeId);
//		std::cout
//				<< " ---Wait Device  sharedSemaphores->sem_traffic_done DoRead at "
//				<< m_nodeId << " \n";
//		sem_wait(&sharedSemaphores->sem_traffic_done);
//		std::cout
//				<< " ---Wait Device  sharedSemaphores->sem_traffic_done DoRead at "
//				<< m_nodeId << " \n";
//		std::cout
//				<< " ---Post Device  sharedSemaphores->sem_traffic_go DoRead at "
//				<< m_nodeId << " \n";
//		sem_post(&sharedSemaphores->sem_traffic_go);
//		std::cout
//				<< " ---Post Device  sharedSemaphores->sem_traffic_go DoRead at "
//				<< m_nodeId << " \n";
//		NS_LOG_LOGIC("ns3 released contiki after read " << m_nodeId);
//
//		std::cout << " ---Post Device  sharedSemaphores->sem_in DoRead at "
//				<< m_nodeId << " \n";
//		if (sem_post(&sharedSemaphores->sem_in) == -1)
//			NS_FATAL_ERROR("sem_post() failed: " << strerror(errno));
//		std::cout << " ---Post Device  sharedSemaphores->sem_in DoRead at "
//				<< m_nodeId << " \n";
//	}
//	if (input_size == 0) {
//		//NS_LOG_LOGIC ("ContikiNetDeviceFdReader::DoRead(): done" << m_nodeId);
//		free(buf);
//		buf = 0;
//		//len = 0;
//	}
//	if (input_size > 0)
//		NS_LOG_LOGIC("read data of length " << input_size);
//
//	return IpcReader::Data(buf, input_size);
//}

// This runs in a separate thread
void IpcReader::Run(void) {
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


		memcpy(&typeOfInfo, sharedSemaphores->traffic_in, 1);

		// 0 = Data transfer
		// 1 = Timer
		if (typeOfInfo == TIMER_TYPE) {

			NS_LOG_LOGIC("contiki requested a timer\n" << m_nodeId);

			std::cout
					<< " ---Wait ipcReader  sharedSemaphores->sem_timer Run at "
					<< m_nodeId << " \n";
//			if (sem_wait(&sharedSemaphores->sem_timer) == -1)
//				NS_FATAL_ERROR("sem_wait(): error" << strerror(errno));
//			std::cout
//					<< " ---Wait ipcReader  sharedSemaphores->sem_timer Run at "
//					<< m_nodeId << " \n";

			memcpy(&timertype, sharedSemaphores->traffic_in + 1, 1);
			memcpy(&timerval, sharedSemaphores->traffic_in + 2, 8);

			memset(sharedSemaphores->traffic_in, 0, 10);

			NS_LOG_LOGIC("contiki's timer handled " << m_nodeId);NS_LOG_LOGIC("releasing contiki after timer request " << m_nodeId);

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

//// This runs in a separate thread
//void IpcReader::Run(void) {
//	for (;;) {
//		if (m_stop) {
//			// this thread is done
//			break;
//		}
//
//		/////////////////////////////////////////////////////////
//		// Checking if contiki requested to schedule a timer
//		uint64_t timerval = 0;
//		uint8_t timertype = 0;
//
//		int rtval;
//		sem_getvalue(&sharedSemaphores->sem_timer_done, &rtval);
//		if (rtval == 1) {
//
//			NS_LOG_LOGIC("contiki requested a timer\n" << m_nodeId);
//
//			std::cout
//					<< " ---Wait ipcReader  sharedSemaphores->sem_timer Run at "
//					<< m_nodeId << " \n";
////			if (sem_wait(&sharedSemaphores->sem_timer) == -1)
////				NS_FATAL_ERROR("sem_wait(): error" << strerror(errno));
////			std::cout
////					<< " ---Wait ipcReader  sharedSemaphores->sem_timer Run at "
////					<< m_nodeId << " \n";
//
//			memcpy(&timertype, sharedSemaphores->traffic_timer, 1);
//			memcpy(&timerval, sharedSemaphores->traffic_timer + 1, 8);
//
//			memset(sharedSemaphores->traffic_timer, 0, 9);
//
//			NS_LOG_LOGIC("contiki's timer handled " << m_nodeId);
//
//			if (timertype != 0 && timertype != 1)
//				NS_FATAL_ERROR("wrong timertype " << timertype);
//
//			if (timerval > 0) {
//				SetTimer(timerval, timertype);
//			}
//
//			NS_LOG_LOGIC("releasing contiki after timer request " << m_nodeId);
//
//			std::cout
//					<< " ---Wait ipcReader  sharedSemaphores->sem_timer_done Run at "
//					<< m_nodeId << " Timer Value: " << timerval <<" \n";
//
//			sem_wait(&sharedSemaphores->sem_timer_done);
//			std::cout
//					<< " ---Wait ipcReader  sharedSemaphores->sem_timer_done Run at "
//					<< m_nodeId << " \n";
//			std::cout
//					<< " ---Post ipcReader  sharedSemaphores->sem_timer_go Run at "
//					<< m_nodeId << " \n";
//			sem_post(&sharedSemaphores->sem_timer_go);
//			std::cout
//					<< " ---Post ipcReader  sharedSemaphores->sem_timer_go Run at "
//					<< m_nodeId << " \n";
//
//			NS_LOG_LOGIC("contiki released after timer request" << m_nodeId);
//
//			std::cout
//					<< " ---Post ipcReader  contikiNetDevice::sharedSemaphores->sem_timer Run at "
//					<< m_nodeId << " \n";
////			if (sem_post(&sharedSemaphores->sem_timer) == -1)
////				NS_FATAL_ERROR("sem_post(): error" << strerror(errno));
////			std::cout
////					<< " ---Post ipcReader  sharedSemaphores->sem_timer Run at "
////					<< m_nodeId << " \n";
//		}
//
//		//////////////////////////////////////////////////////////
//
//		// Processing traffic sent by contiki
//
//		struct IpcReader::Data data = DoRead();
//
//		// reading stops when m_len is zero
//		if (data.m_len == 0) {
//			NS_LOG_INFO("read data of size 0");
//			//break;
//		}
//		// the callback is only called when m_len is positive (data
//		// is ignored if m_len is negative)
//		else if (data.m_len > 0) {
//			m_readCallback(data.m_buf, data.m_len);
//		}
//
//	}
//}

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
	Time timeToShedule = (time + Simulator::Now());
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

// Specific value, remove later when we are sure the smaller is better
//	std::list < Time > removeValues;
//
//	// searches for the time, if found some one updates the
//	// return value with the set of nodes to wake up and erase it from the list
//	// since the time for them has arrived
//	if (nodesToWakeUp.find(time) != nodesToWakeUp.end()) {
//		returnValue = nodesToWakeUp[time];
//		nodesToWakeUp.erase(time);
//	}

//  to test: REMOVE
//	Time tmptime = Time::From (1, Time::NS);
//	std::list < uint32_t > scheduledNodes;
//	scheduledNodes.push_back(1);
//	scheduledNodes.push_back(37);
//	nodesToWakeUp.insert(
//					std::pair<Time, std::list<uint32_t> >(tmptime, scheduledNodes));

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
