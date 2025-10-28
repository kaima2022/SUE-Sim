/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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

#include "sue-sim-module-helper.h"
#include "ns3/point-to-point-sue-net-device.h"
#include "ns3/point-to-point-sue-channel.h"
#include "ns3/node.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/string.h"
#include "ns3/names.h"

namespace ns3 {

PointToPointSueHelper::PointToPointSueHelper ()
{
  m_queueFactory.SetTypeId ("ns3::DropTailQueue<Packet>");
  m_deviceFactory.SetTypeId ("ns3::PointToPointSueNetDevice");
  m_channelFactory.SetTypeId ("ns3::PointToPointSueChannel");
}

void
PointToPointSueHelper::SetDeviceAttribute (std::string n1, const AttributeValue &value1)
{
  m_deviceFactory.Set (n1, value1);
}

void
PointToPointSueHelper::SetChannelAttribute (std::string n1, const AttributeValue &value1)
{
  m_channelFactory.Set (n1, value1);
}

NetDeviceContainer
PointToPointSueHelper::Install (NodeContainer c)
{
  NetDeviceContainer devs;

  for (uint32_t i = 0; i < c.GetN (); i += 2)
    {
      Ptr<Node> node1 = c.Get (i);
      Ptr<Node> node2 = c.Get (i + 1);

      Ptr<PointToPointSueNetDevice> dev1 = m_deviceFactory.Create<PointToPointSueNetDevice> ();
      Ptr<PointToPointSueNetDevice> dev2 = m_deviceFactory.Create<PointToPointSueNetDevice> ();

      dev1->SetAddress (Mac48Address::Allocate ());
      dev2->SetAddress (Mac48Address::Allocate ());

      Ptr<Queue<Packet> > queue1 = m_queueFactory.Create<Queue<Packet> > ();
      Ptr<Queue<Packet> > queue2 = m_queueFactory.Create<Queue<Packet> > ();
      dev1->SetQueue (queue1);
      dev2->SetQueue (queue2);

      Ptr<PointToPointSueChannel> channel = m_channelFactory.Create<PointToPointSueChannel> ();

      dev1->Attach (channel);
      dev2->Attach (channel);

      node1->AddDevice (dev1);
      node2->AddDevice (dev2);

      devs.Add (dev1);
      devs.Add (dev2);
    }
  return devs;
}

NetDeviceContainer
PointToPointSueHelper::Install (Ptr<Node> a, Ptr<Node> b)
{
  NodeContainer c;
  c.Add (a);
  c.Add (b);
  return Install (c);
}

NetDeviceContainer
PointToPointSueHelper::Install (std::string aName, std::string bName)
{
  Ptr<Node> a = Names::Find<Node> (aName);
  Ptr<Node> b = Names::Find<Node> (bName);
  return Install (a, b);
}

Ptr<NetDevice>
PointToPointSueHelper::InstallPriv (Ptr<Node> node)
{
  Ptr<PointToPointSueNetDevice> device = m_deviceFactory.Create<PointToPointSueNetDevice> ();
  device->SetAddress (Mac48Address::Allocate ());
  Ptr<Queue<Packet> > queue = m_queueFactory.Create<Queue<Packet> > ();
  device->SetQueue (queue);
  node->AddDevice (device);
  return device;
}

} // namespace ns3