/*
 * ContikiMac.h
 *
 *  Created on: Mar 29, 2013
 *      Author: kentux
 */

#ifndef CONTIKIMAC_H_
#define CONTIKIMAC_H_

#include "ns3/packet.h"
#include "ns3/mac64-address.h"
#include "ns3/log.h"
#include "ns3/net-device.h"
#include "ns3/trace-source-accessor.h"

#include "contiki-phy.h"
#include "contiki-device.h"

namespace ns3 {

class ContikiPhy;
class ContikiNetDevice;

class ContikiMac : public Object
{
public:
  static TypeId GetTypeId (void);

  ContikiMac ();
  virtual ~ContikiMac ();

  /**
   * \returns the MAC address associated to this MAC layer.
   */
  Mac64Address GetAddress (void);

  /**
   * \param address the current address of this MAC layer.
   */
  void SetAddress (Mac64Address address);

  /**
   * \param packet the packet to send.
   */
  void Enqueue (Ptr<const Packet> packet);

  /**
   * \param phy the physical layer attached to this MAC.
   */
  void SetPhy (Ptr<ContikiPhy> phy);

  /**
   * \param bridge the ContikiNetDevice attached to this MAC.
   */
  void SetBridge (Ptr<ContikiNetDevice> bridge);

  /**
   * Public method used to fire a MacTx trace.  Implemented for encapsulation
   * purposes.
   */
  void NotifyTx (Ptr<const Packet> packet);

  /**
   * Public method used to fire a MacRx trace.  Implemented for encapsulation
   * purposes.
   */
  void NotifyRx (Ptr<const Packet> packet);

  /**
   * Packet received from the channel
   */
  void Receive (Ptr<Packet> packet);

  /**
   * Packet is forwarded through the socket and out of the ns-3 domain
   */
  void ForwardUp (Ptr<Packet> packet);


private:
  /**
   * The trace source fired when packets come into the "top" of the device
   * at the L3/L2 transition, before being queued for transmission.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_macTxTrace;

  /**
   * The trace source fired when packets coming into the "top" of the device
   * are dropped at the MAC layer during transmission.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_macTxDropTrace;

  /**
   * The trace source fired for packets successfully received by the device
   * immediately before being forwarded up to higher layers (at the L2/L3
   * transition).  This is a promiscuous trace.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_macPromiscRxTrace;

  /**
   * The trace source fired for packets successfully received by the device
   * immediately before being forwarded up to higher layers (at the L2/L3
   * transition).  This is a non- promiscuous trace.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_macRxTrace;

  /**
   * The trace source fired when packets coming into the "top" of the device
   * are dropped at the MAC layer during reception.
   *
   * \see class CallBackTraceSource
   */
  TracedCallback<Ptr<const Packet> > m_macRxDropTrace;

  /**
   * Stores the MAC address assigned to this MAC layer
   */
  Mac64Address m_macAddr;

  /**
   * Holds a reference to the PHY layer associated with this MAC layer
   */
  Ptr<ContikiPhy> m_phy;

  /**
   * Holds a reference to the ContikiNetDevice associated with this MAC layer
   */
  Ptr<ContikiNetDevice> m_bridge;


};

} // namespace ns3


#endif /* CONTIKIMAC_H_ */
