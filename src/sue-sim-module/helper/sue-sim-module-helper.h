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

#ifndef POINT_TO_POINT_SUE_HELPER_H
#define POINT_TO_POINT_SUE_HELPER_H

#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"

namespace ns3 {

/**
 * \ingroup point-to-point-sue
 * \brief Build a set of PointToPointSueNetDevice objects
 *
 * This helper class encapsulates the creation of SUE-enhanced point-to-point
 * network devices and channels with CBFC and Virtual Channel support.
 */
class PointToPointSueHelper
{
public:
  /**
   * \brief Construct a PointToPointSueHelper
   */
  PointToPointSueHelper ();

  /**
   * \brief Set the data rate of the devices
   * \param dataRate The data rate, typically in bits per second
   */
  void SetDeviceAttribute (std::string n1, const AttributeValue &value1);

  /**
   * \brief Set the channel attribute
   * \param n1 The name of the attribute
   * \param value1 The value of the attribute
   */
  void SetChannelAttribute (std::string n1, const AttributeValue &value1);

  /**
   * \brief Install SUE devices on the specified nodes
   * \param c The NodeContainer containing the nodes to install devices on
   * \return A NetDeviceContainer containing the installed devices
   */
  NetDeviceContainer Install (NodeContainer c);

  /**
   * \brief Install SUE devices on a pair of nodes
   * \param a The first node
   * \param b The second node
   * \return A NetDeviceContainer containing the installed devices
   */
  NetDeviceContainer Install (Ptr<Node> a, Ptr<Node> b);

  /**
   * \brief Install SUE devices on two nodes by name
   * \param aName The name of the first node
   * \param bName The name of the second node
   * \return A NetDeviceContainer containing the installed devices
   */
  NetDeviceContainer Install (std::string aName, std::string bName);

private:
  /**
   * \brief Disable queue and install the device
   * \param node The node to install the device on
   * \return The installed NetDevice
   */
  Ptr<NetDevice> InstallPriv (Ptr<Node> node);

  ObjectFactory m_queueFactory;   ///< Factory for creating queues
  ObjectFactory m_deviceFactory;  ///< Factory for creating devices
  ObjectFactory m_channelFactory; ///< Factory for creating channels
};

} // namespace ns3

#endif /* POINT_TO_POINT_SUE_HELPER_H */