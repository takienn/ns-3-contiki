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
  void Start (void *addr, Callback<void, uint8_t *, ssize_t> readCallback, uint32_t nodeId);

  /**
   * Stop the read thread and reset internal state.  This does not
   * close the file descriptor used for reading.
   */
  void Stop (void);

protected:

  /**
   * \internal
   * \brief A structure representing data read.
   */
  struct Data
  {
    Data () : m_buf (0), m_len (0) {}
    Data (uint8_t *buf, ssize_t len) : m_buf (buf), m_len (len) {}
    uint8_t *m_buf;
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

  /**
   * \internal
   * \brief The file descriptor to read from.
   */
  //int m_fd;
  void *m_traffic_in;
  uint32_t m_nodeId;

private:

  void Run (void);
  void DestroyEvent (void);

  Callback<void, uint8_t *, ssize_t> m_readCallback;
  Ptr<SystemThread> m_readThread, m_writeThread;
  int m_evpipe[2];           // pipe used to signal events between threads
  bool m_stop;               // true means the read thread should stop
  EventId m_destroyEvent;
};

} // namespace ns3


#endif /* IPC_READER_H_ */
