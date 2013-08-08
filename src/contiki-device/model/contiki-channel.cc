/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2006,2007 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage, <mathieu.lacage@sophia.inria.fr>
 */

#include "contiki-channel.h"

NS_LOG_COMPONENT_DEFINE ("ContikiChannel");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (ContikiChannel);

TypeId
ContikiChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ContikiChannel")
    .SetParent<Channel> ()
    .AddConstructor<ContikiChannel> ()
    .AddAttribute ("PropagationLossModel", "A pointer to the propagation loss model attached to this channel.",
                   PointerValue (),
                   MakePointerAccessor (&ContikiChannel::m_loss),
                   MakePointerChecker<PropagationLossModel> ())
    .AddAttribute ("PropagationDelayModel", "A pointer to the propagation delay model attached to this channel.",
                   PointerValue (),
                   MakePointerAccessor (&ContikiChannel::m_delay),
                   MakePointerChecker<PropagationDelayModel> ())
  ;
  return tid;
}

ContikiChannel::ContikiChannel ()
{
}
ContikiChannel::~ContikiChannel ()
{
  NS_LOG_FUNCTION_NOARGS ();
  m_phyList.clear ();
}

void
ContikiChannel::SetPropagationLossModel (Ptr<PropagationLossModel> loss)
{
  m_loss = loss;
}
void
ContikiChannel::SetPropagationDelayModel (Ptr<PropagationDelayModel> delay)
{
  m_delay = delay;
}

void
ContikiChannel::Send (Ptr<ContikiPhy> sender, Ptr<const Packet> packet, double txPowerDbm)
{
  Ptr<MobilityModel> senderMobility = sender->GetMobility ()->GetObject<MobilityModel> ();
  NS_ASSERT (senderMobility != 0);
  uint32_t j = 0;
  for (PhyList::const_iterator i = m_phyList.begin (); i != m_phyList.end (); i++, j++)
    {
      if (sender != (*i))
        {

          Ptr<MobilityModel> receiverMobility = (*i)->GetMobility()->GetObject<MobilityModel>();
          Time delay = m_delay->GetDelay (senderMobility, receiverMobility);
          double rxPowerDbm = m_loss->CalcRxPower (txPowerDbm, senderMobility, receiverMobility);
          NS_LOG_DEBUG ("propagation: txPower=" << txPowerDbm << "dbm, rxPower=" << rxPowerDbm << "dbm, " <<
                        "distance=" << senderMobility->GetDistanceFrom (receiverMobility) << "m, delay=" << delay);
          Ptr<Packet> copy = packet->Copy ();
          Ptr<Object> dstNetDevice = m_phyList[j]->GetDevice ();
          uint32_t dstNode;
          if (dstNetDevice == 0)
            {
              dstNode = 0xffffffff;
            }
          else
            {
              dstNode = dstNetDevice->GetObject<NetDevice> ()->GetNode ()->GetId ();
            }
          Simulator::ScheduleWithContext (dstNode,
                                          delay, &ContikiChannel::Receive, this,
                                          j, copy, rxPowerDbm);

          // OR SET THE SCHEDULE HERE
        }
    }
}

void
ContikiChannel::Receive (uint32_t i, Ptr<Packet> packet, double rxPowerDbm) const
{
  m_phyList[i]->StartReceivePacket (packet, rxPowerDbm);
}

uint32_t
ContikiChannel::GetNDevices (void) const
{
  return m_phyList.size ();
}
Ptr<NetDevice>
ContikiChannel::GetDevice (uint32_t i) const
{
  return m_phyList[i]->GetDevice ()->GetObject<NetDevice> ();
}

void
ContikiChannel::Add (Ptr<ContikiPhy> phy)
{
  m_phyList.push_back (phy);
}

} // namespace ns3
