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

#include "ns3/test.h"
#include "ns3/point-to-point-sue-net-device.h"
#include "ns3/point-to-point-sue-channel.h"
#include "ns3/node.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"

namespace ns3 {

/**
 * \ingroup point-to-point-sue
 * \ingroup tests
 *
 * \brief Test suite for PointToPointSue module
 */
class PointToPointSueTestSuite : public TestSuite
{
public:
  PointToPointSueTestSuite ();
};

class PointToPointSueTestCase : public TestCase
{
public:
  PointToPointSueTestCase ();
  virtual ~PointToPointSueTestCase ();

private:
  virtual void DoRun (void);
  void DataSend (Ptr<Packet> p);
};

PointToPointSueTestCase::PointToPointSueTestCase ()
  : TestCase ("PointToPointSue basic test")
{
}

PointToPointSueTestCase::~PointToPointSueTestCase ()
{
}

void
PointToPointSueTestCase::DataSend (Ptr<Packet> p)
{
  NS_TEST_ASSERT_MSG_NE (p, nullptr, "Packet should not be null");
}

void
PointToPointSueTestCase::DoRun (void)
{
  // Simple smoke test - just create objects and verify they exist
  Ptr<Node> nodeA = CreateObject<Node> ();
  Ptr<Node> nodeB = CreateObject<Node> ();

  NS_TEST_ASSERT_MSG_NE (nodeA, nullptr, "NodeA should not be null");
  NS_TEST_ASSERT_MSG_NE (nodeB, nullptr, "NodeB should not be null");

  // Create SUE devices only - skip complex interactions
  Ptr<PointToPointSueNetDevice> devA = CreateObject<PointToPointSueNetDevice> ();
  Ptr<PointToPointSueNetDevice> devB = CreateObject<PointToPointSueNetDevice> ();

  NS_TEST_ASSERT_MSG_NE (devA, nullptr, "DeviceA should not be null");
  NS_TEST_ASSERT_MSG_NE (devB, nullptr, "DeviceB should not be null");

  // Set MAC addresses only
  devA->SetAddress (Mac48Address ("00:00:00:00:00:01"));
  devB->SetAddress (Mac48Address ("00:00:00:00:00:02"));

  // Add devices to nodes
  nodeA->AddDevice (devA);
  nodeB->AddDevice (devB);

  // Very simple cleanup
  Simulator::Destroy ();
}

PointToPointSueTestSuite::PointToPointSueTestSuite ()
  : TestSuite ("point-to-point-sue", Type::UNIT)
{
  AddTestCase (new PointToPointSueTestCase);
}

static PointToPointSueTestSuite g_pointToPointSueTestSuite;

} // namespace ns3