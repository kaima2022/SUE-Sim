/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2025 SUE-Sim Contributors
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
  AddTestCase (new PointToPointSueTestCase, Duration::QUICK);
}

static PointToPointSueTestSuite g_pointToPointSueTestSuite;

} // namespace ns3