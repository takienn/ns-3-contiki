#ifndef CONTIKI_PHY_H_
#define CONTIKI_PHY_H_

#include <stdint.h>
#include "ns3/callback.h"
#include "ns3/event-id.h"
#include "ns3/packet.h"
#include "ns3/object.h"
#include "ns3/traced-callback.h"
#include "ns3/nstime.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/random-variable.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/enum.h"
#include "ns3/pointer.h"
#include "ns3/net-device.h"
#include "ns3/trace-source-accessor.h"

#include <math.h>
#include <stdio.h>

#include "contiki-channel.h"
#include "contiki-mac.h"



namespace ns3 {

class ContikiChannel;
class ContikiMac;

class ContikiPhy : public Object {

  typedef Callback< void,Ptr<Packet> > RxOkCallback;
  typedef std::vector<ContikiMac *> Listeners;

public:
  static TypeId GetTypeId (void);

  enum PhyMode
  {
    DSSS_BPSK,
    DSSS_O_QPSK_MHz,
    DSSS_O_QPSK_GHz,
    PSSS_ASK
  };


  ContikiPhy ();
  virtual ~ContikiPhy ();

  void StartReceivePacket (Ptr<Packet> packet, double rxPowerDbm);
  void SetDevice (Ptr<Object> device);
  void SetMobility (Ptr<Object> mobility);
  void SetEdThreshold (double threshold);
  void SetDataRate (uint64_t dataRate);
  void SetMode (PhyMode mode);
  Ptr<Object> GetDevice (void) const;
  Ptr<Object> GetMobility (void);
  uint64_t GetDataRate (void);
  PhyMode GetMode (void);
  virtual void SendPacket (Ptr<const Packet> packet);

  virtual void RegisterListener (ContikiMac *listener);
  virtual void SetReceiveOkCallback (RxOkCallback callback);

  void SetChannel (Ptr<ContikiChannel> channel);
  Ptr<ContikiChannel> GetChannel (void) const;

  /**
     *
     * Public method used to fire a MonitorSniffer trace for a wifi packet being transmitted.  Implemented for encapsulation
     * purposes.
     *
     * @param packet the packet being transmitted
     */

  void NotifyMonitorSniffTx (Ptr<const Packet> packet);

  /**
     *
     * Public method used to fire a MonitorSniffer trace for a wifi packet being received.  Implemented for encapsulation
     * purposes.
     *
     * @param packet the packet being received
     */

  void NotifyMonitorSniffRx (Ptr<const Packet> packet);

private:
  virtual void DoDispose (void);
  virtual void EndReceive (Ptr<Packet> packet);
  double DbmToW (double dBm) const;

  Time CalculateTxDuration (uint32_t size);

  Ptr<Object> m_device;
  Ptr<Object> m_mobility;
  Ptr<ContikiChannel> m_channel;
  Listeners m_listeners;
  uint64_t m_dataRate;
  PhyMode m_mode;
  double m_edThresholdW;

  RxOkCallback m_rxOkCallback;
  EventId m_endRxEvent;
  UniformVariable m_random;

  TracedCallback<Ptr<const Packet> > m_phyMonitorSniffTxTrace;
  TracedCallback<Ptr<const Packet> > m_phyMonitorSniffRxTrace;
};

} // namespace ns3


#endif /* CONTIKI_PHY_H_ */
