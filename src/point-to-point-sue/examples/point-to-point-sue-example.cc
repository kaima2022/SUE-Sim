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

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-sue-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PointToPointSueExample");

int
main (int argc, char *argv[])
{
  // Configure logging
  LogComponentEnable ("PointToPointSueExample", LOG_LEVEL_INFO);
  LogComponentEnable ("PointToPointSueNetDevice", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Install network stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Create PointToPointSue helper
  PointToPointSueHelper p2pSue;
  p2pSue.SetDeviceAttribute ("DataRate", StringValue ("5Gbps"));
  p2pSue.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2pSue.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install SUE devices
  NetDeviceContainer devices = p2pSue.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Create simple UDP application
  uint16_t port = 9;  // Discard port
  Address sinkAddress (InetSocketAddress (interfaces.GetAddress (1), port));
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", sinkAddress);

  // Install receiver application
  ApplicationContainer sinkApps = packetSinkHelper.Install (nodes.Get (1));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (Seconds (10.0));

  // Install sender application
  OnOffHelper onOffHelper ("ns3::UdpSocketFactory", sinkAddress);
  onOffHelper.SetConstantRate (DataRate ("4Gbps"), 1024);
  onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer clientApps = onOffHelper.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // TODO: Support Pcap tracing

  // Run simulation
  Simulator::Run ();
  Simulator::Destroy ();

  NS_LOG_INFO ("Point-to-Point SUE Example Completed");

  return 0;
}