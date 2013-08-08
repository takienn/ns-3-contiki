/*
 * ipc-reader.h
 *
 *  Created on: Apr 2, 2013
 *      Author: kentux
 */

#ifndef IPC_READER_H_
#define IPC_READER_H_

#include <stdint.h>
#include <string.h>
#include <map>

#include "ns3/callback.h"
#include "ns3/event-id.h"
#include "ns3/system-thread.h"
#include "ns3/system-mutex.h"

#include <sstream>
#include <sys/ipc.h>
#include <sys/shm.h>


#define START_CONST 1092
#define MAIN_SH_SRTUCTURE  0
#define TRAFFIC_IN  1
#define TRAFFIC_OUT  2
#define TIMER_IN  3
#define TIME  4
#define IN  5

/**
 * \brief Union to store the shared semaphores.
 *
 * Shared semaphores storage type
 */

typedef struct semaphores_t {
	int id;

	sem_t sem_time;
	sem_t sem_timer;

	sem_t sem_timer_go;
	sem_t sem_timer_done;

	sem_t sem_in;
	sem_t sem_out;

	sem_t sem_traffic_go;
	sem_t sem_traffic_done;

	sem_t sem_go;
	sem_t sem_done;

	/**
	 * \internal
	 * The pointer to a shared memory address where to receive traffic from.
	 */
	unsigned char *traffic_in;
	int traffic_in_id;
	/**
	 * \internal
	 * The pointer to a shared memory address where to send traffic to.
	 */
	unsigned char *traffic_out;
	int traffic_out_id;
	/**
	 * \internal
	 * The pointer to a shared memory address where to store the timers.
	 */
	unsigned char *traffic_timer;
	int traffic_timer_id;


	/**
	 * \internal
	 * The pointer to a shared memory address where to store the time.
	 */
	int *shm_time;
	int shm_time_id;
//	unsigned char *traffic_time;
//	int traffic_time_id;
//	/**
//	 * \internal
//	 * Shared Memory Object for input traffic
//	 */
//	int *shm_in;
//	int shm_in_id;
//	/**
//	 * \internal
//	 * Shared Memory Object for output traffic
//	 */
//	int *shm_out;
//	int shm_out_id;

} semaphores_t;

namespace ns3 {
class ContikiNetDevice;

/**
 * \brief A class that asynchronously reads from a file descriptor.
 *
 * This class can be used to start a system thread that reads from a
 * given file descriptor and invokes a given callback when data is
 * received.  This class handles thread management automatically but
 * the \p DoRead() method must be implemented by a subclass.
 */
class IpcReader: public SimpleRefCount<IpcReader> {
public:

	IpcReader();
	virtual ~IpcReader();

	/**
	 * Start a new read thread.
	 *
	 * \param fd A valid file descriptor open for reading.
	 *
	 * \param readCallback A callback to invoke when new data is
	 * available.
	 */
	semaphores_t * Start(Callback<void, unsigned char *, ssize_t> readCallback,
			uint32_t nodeId, pid_t pid);

	/**
	 * Stop the read thread and reset internal state.  This does not
	 * close the file descriptor used for reading.
	 */
	void Stop(void);

	/**
	 * Check if contiki requested to schedule a timer
	 */
	void CheckTimer(void);

	/**
	 * Schedules a NS-3 timer upon request from contiki to implement its
	 * rtimer module
	 *\param time value of the interval to set timer to
	 *\param type type of the timer as requested by contiki, 0 for event timer, 1 for reattime timer
	 */
	void SetTimer(uint64_t time, int type);

	/**
	 * Schedules a NS-3 timer upon request from contiki to implement its
	 * rtimer module
	 *\param time value of the interval to set timer to
	 *\param type type of the timer as requested by contiki, 0 for event timer, 1 for reattime timer
	 */
	void SetTimer(Time time, int type);

	/**
	 * Schedules a NS-3 timer 1 nanosecond from now
	 */
	void SetRelativeTimer();

	/**
	 * Callback that is called upon timer expiration , that signals contiki fork
	 * that its time.
	 */
	void SendAlarm(void);

	/**
	 * Sets a new scheduled event.
	 */
	void setSchedule(Time time, uint32_t nodeId);

	/**
	 * Returns the Map with the nodes that should wake up now and release the
	 * time from the scheduling map
	 */
	static std::list<uint32_t> getReleaseSchedule(Time time);

	/*
	 * Map to store the Go semaphores of all nodes.
	 */
	static std::map<uint32_t, sem_t*> listOfGoSemaphores;
	/*
	 * Map to store the Done semaophores of all nodes.
	 */
	static std::map<uint32_t, sem_t*> listOfDoneSemaphores;

//	ContikiNetDevice *contikiNetDevice;
	/**
	 * \internal
	 * The pointer to a shared memory address where to synchronize
	 * current simulation time value.
	 */
	static int *m_shm_time;
//	static unsigned char *m_traffic_time;

	/**
	 * \internal
	 * semaphore for time update operations
	 */
	static sem_t m_sem_time;

	/**
	 * \internal
	 * The size of the packets transfer area
	 */
	static size_t m_traffic_size;

	/**
	 * \internal
	 * The size of the time transfer area
	 */
	static size_t m_time_size;

	/**
	 * \internal
	 * Id of the memory location where the time is stored
	 */
	static int m_time_Memory_Id;


protected:

	/**
	 * \internal
	 * \brief A structure representing data read.
	 */
	struct Data {
		Data() :
				m_buf(0), m_len(0) {
		}
		Data(unsigned char *buf, ssize_t len) :
				m_buf(buf), m_len(len) {
		}
		unsigned char *m_buf;
		ssize_t m_len;
	};

	/**
	 * Initialization of the IPC layer, creates and initializes the semaphores
	 * and the shared memory areas
	 */
	void initIpc();


	/**
	 * \internal
	 * \brief The read implementation.
	 *
	 * The value of \p m_len returned controls further processing.  The
	 * callback function is only invoked when \p m_len is positive; any
	 * data read is not processed when \p m_len is negative; reading
	 * stops when \p m_len is zero.
	 *
	 * The management of memory associated with \p m_buf must be
	 * compatible with the read callback.
	 *
	 * \return A structure representing what was read.
	 */
	IpcReader::Data DoRead(void);

//	std::stringstream m_sem_timer_go_name;
//	std::stringstream m_sem_timer_done_name;
//
//	std::stringstream m_sem_traffic_go_name;
//	std::stringstream m_sem_traffic_done_name;
//
//	std::stringstream m_shm_in_name;
//	std::stringstream m_shm_timer_name;
//
//	std::stringstream m_sem_in_name;
//	std::stringstream m_sem_timer_name;

//	int m_shm_in;
//	int m_shm_timer;
//
//	unsigned char *m_traffic_in;
//	unsigned char *m_traffic_timer;


	uint32_t m_nodeId;
	pid_t m_pid;

private:

	// waked up nodes list to improve performance
//	static std::list<int> nodesToWakeUp;
//	nodesToWakeUp.insert(std::make_pair(5,"Hello"));
	static std::map<Time, std::list<uint32_t> > nodesToWakeUp;
	static SystemMutex m_controlNodesToWakeUp;

//	static std::mutex controlWakeUpList;

	void Run(void);
	void DestroyEvent(void);

	Callback<void, unsigned char *, ssize_t> m_readCallback;
	Ptr<SystemThread> m_readThread, m_writeThread;
	int m_evpipe[2]; // pipe used to signal events between threads
	bool m_stop; // true means the read thread should stop
	EventId m_destroyEvent;
	semaphores_t *sharedSemaphores;

};

} // namespace ns3

#endif /* IPC_READER_H_ */
