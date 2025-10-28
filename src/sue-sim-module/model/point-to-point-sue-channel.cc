/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2008 University of Washington
 * Copyright 2025 SUE-Sim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "point-to-point-sue-channel.h"
#include "point-to-point-sue-net-device.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PointToPointSueChannel");

NS_OBJECT_ENSURE_REGISTERED (PointToPointSueChannel);

TypeId 
PointToPointSueChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PointToPointSueChannel")
    .SetParent<Channel> ()
    .SetGroupName ("PointToPointSue")
    .AddConstructor<PointToPointSueChannel> ()
    .AddAttribute ("Delay", "Propagation delay through the channel",
                   TimeValue (Seconds (0)),
                   MakeTimeAccessor (&PointToPointSueChannel::m_delay),
                   MakeTimeChecker ())
    .AddTraceSource ("TxRxPointToPoint",
                     "Trace source indicating transmission of packet "
                     "from the PointToPointSueChannel, used by the Animation "
                     "interface.",
                     MakeTraceSourceAccessor (&PointToPointSueChannel::m_txrxPointToPoint),
                     "ns3::PointToPointSueChannel::TxRxAnimationCallback")
  ;
  return tid;
}

//
// By default, you get a channel that 
// has an "infitely" fast transmission speed and zero delay.
PointToPointSueChannel::PointToPointSueChannel()
  :
    Channel (),
    m_delay (Seconds (0.)),
    m_nDevices (0)
{
  NS_LOG_FUNCTION_NOARGS ();
}

PointToPointSueChannel::~PointToPointSueChannel ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

void
PointToPointSueChannel::Attach (Ptr<PointToPointSueNetDevice> device)
{
  NS_LOG_FUNCTION (this << device);
  NS_ASSERT_MSG (m_nDevices < N_DEVICES, "Only two devices permitted");
  NS_ASSERT (device != nullptr);

  m_link[m_nDevices++].m_src = device;
//
// If we have both devices connected to the channel, then finish introducing
// the two halves and set the links to IDLE.
//
  if (m_nDevices == N_DEVICES)
    {
      m_link[0].m_dst = m_link[1].m_src;
      m_link[1].m_dst = m_link[0].m_src;
      m_link[0].m_state = IDLE;
      m_link[1].m_state = IDLE;
    }
}

bool
PointToPointSueChannel::TransmitStart (
  Ptr<const Packet> p,
  Ptr<PointToPointSueNetDevice> src,
  Time txTime)
{
  NS_LOG_FUNCTION (this << p << src);
  NS_LOG_LOGIC ("UID is " << p->GetUid () << ")");

  NS_ASSERT (m_link[0].m_state != INITIALIZING);
  NS_ASSERT (m_link[1].m_state != INITIALIZING);

  uint32_t wire = src == m_link[0].m_src ? 0 : 1;

  Simulator::ScheduleWithContext (m_link[wire].m_dst->GetNode ()->GetId (),
                                  txTime + m_delay, &PointToPointSueNetDevice::Receive,
                                  m_link[wire].m_dst, p->Copy ());

  // Call the tx anim callback on the net device
  m_txrxPointToPoint (p, src, m_link[wire].m_dst, txTime, txTime + m_delay);
  return true;
}

std::size_t
PointToPointSueChannel::GetNDevices (void) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return m_nDevices;
}

Ptr<PointToPointSueNetDevice>
PointToPointSueChannel::GetPointToPointDevice (std::size_t i) const
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_ASSERT (i < 2);
  return m_link[i].m_src;
}

Ptr<NetDevice>
PointToPointSueChannel::GetDevice (std::size_t i) const
{
  NS_LOG_FUNCTION_NOARGS ();
  return GetPointToPointDevice (i);
}

Time
PointToPointSueChannel::GetDelay (void) const
{
  return m_delay;
}

Ptr<PointToPointSueNetDevice>
PointToPointSueChannel::GetSource (uint32_t i) const
{
  return m_link[i].m_src;
}

Ptr<PointToPointSueNetDevice>
PointToPointSueChannel::GetDestination (uint32_t i) const
{
  return m_link[i].m_dst;
}

bool
PointToPointSueChannel::IsInitialized (void) const
{
  NS_ASSERT (m_link[0].m_state != INITIALIZING);
  NS_ASSERT (m_link[1].m_state != INITIALIZING);
  return true;
}

} // namespace ns3
