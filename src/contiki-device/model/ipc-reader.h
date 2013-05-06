/*
 * ipc-reader.h
 *
 *  Created on: Apr 2, 2013
 *      Author: kentux
 */

#ifndef IPC_READER_H_
#define IPC_READER_H_

#include <stdint.h>

#include "ns3/callback.h"
#include "ns3/system-thread.h"
#include "ns3/event-id.h"

#include <sstream>

namespace ns3 {

/**
 * \brief A class that asynchronously reads from a file descriptor.
 *
 * This class can be used to start a system thread that reads from a
 * given file descriptor and invokes a given callback when data is
 * received.  This class handles thread management automatically but
 * the \p DoRead() method must be implemented by a subclass.
 */
class IpcReader : public SimpleRefCount<IpcReader>
{
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
  void Start (Callback<void, char *, ssize_t> readCallback, uint32_t nodeId, pid_t pid);

  /**
   * Stop the read thread and reset internal state.  This does not
   * close the file descriptor used for reading.
   */
  void Stop (void);

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
  void SetTimer (uint64_t time, int type);

  /**
   * Callback that is called upon timer expiration , that signals contiki fork
   * that its time.
   */
  void SendAlarm (void);

protected:

  /**
   * \internal
   * \brief A structure representing data read.
   */
  struct Data
  {
    Data () : m_buf (0), m_len (0) {}
    Data (char *buf, ssize_t len) : m_buf (buf), m_len (len) {}
    char *m_buf;
    ssize_t m_len;
  };

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
  virtual IpcReader::Data DoRead (void) = 0;

  std::stringstream m_shm_in_name;
  std::stringstream m_shm_timer_name;

  std::stringstream m_sem_in_name;
  std::stringstream m_sem_timer_name;

  sem_t *m_sem_in;
  sem_t *m_sem_timer;

  int m_shm_in;
  int m_shm_timer;

  char *m_traffic_in;
  char *m_traffic_timer;

  uint32_t m_nodeId;
  pid_t m_pid;

private:

  void Run (void);
  void DestroyEvent (void);

  Callback<void, char *, ssize_t> m_readCallback;
  Ptr<SystemThread> m_readThread, m_writeThread;
  int m_evpipe[2];           // pipe used to signal events between threads
  bool m_stop;               // true means the read thread should stop
  EventId m_destroyEvent;
};

} // namespace ns3


#endif /* IPC_READER_H_ */
