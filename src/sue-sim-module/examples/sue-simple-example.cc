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

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/sue-sim-module-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SueSimpleExample");

int
main (int argc, char *argv[])
{
  // Disable all logging to avoid spam
  // LogComponentDisableAll (LogLevel::LOG_ALL);
  LogComponentEnable ("SueSimpleExample", LOG_LEVEL_INFO);
  LogComponentEnable ("PointToPointSueNetDevice", LOG_LEVEL_INFO);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (2);

  // Install network stack
  InternetStackHelper stack;
  stack.Install (nodes);

  // Create simple Point-to-Point link (not SUE) to avoid complex logging
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Gbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  // Install devices
  NetDeviceContainer devices = p2p.Install (nodes);

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
  sinkApps.Stop (Seconds (3.0)); // Short simulation

  // Install sender application
  OnOffHelper onOffHelper ("ns3::UdpSocketFactory", sinkAddress);
  onOffHelper.SetConstantRate (DataRate ("1Gbps"), 512); // Smaller packets
  onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

  ApplicationContainer clientApps = onOffHelper.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (3.0));

  // Enable routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  // Run simulation
  NS_LOG_INFO ("Starting SUE Simple Example simulation...");
  Simulator::Run ();
  Simulator::Destroy ();

  NS_LOG_INFO ("SUE Simple Example completed successfully!");

  return 0;
}