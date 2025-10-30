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

#ifndef SUE_SWITCH_H
#define SUE_SWITCH_H

#include <map>
#include "ns3/mac48-address.h"
#include "ns3/ethernet-header.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/object.h"

namespace ns3 {

// Forward declarations
class PointToPointSueNetDevice;

/**
 * \ingroup sue-sim-module
 * \class SueSwitch
 * \brief SUE Switch module for handling Layer 2 forwarding functionality
 *
 * This class encapsulates the switch functionality that was previously
 * embedded in PointToPointSueNetDevice, providing clean separation of
 * concerns for switch-specific operations.
 */
class SueSwitch : public Object
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Construct a SueSwitch
   */
  SueSwitch ();

  /**
   * \brief Destroy a SueSwitch
   */
  virtual ~SueSwitch ();

  /**
   * \brief Set the forwarding table for switch devices
   *
   * \param table Map of destination MAC addresses to output port indices
   */
  void SetForwardingTable (const std::map<Mac48Address, uint32_t>& table);

  /**
   * \brief Clear the forwarding table
   */
  void ClearForwardingTable (void);

  /**
   * \brief Check if this device is a switch device
   *
   * \return true if this is a switch device
   */
  bool IsSwitchDevice (Mac48Address mac) const;

  /**
   * \brief Process packet forwarding through switch
   *
   * \param packet Packet to forward
   * \param ethHeader Ethernet header of the packet
   * \param currentDevice Current net device processing the packet
   * \param protocol Protocol number
   * \param vcId Virtual Channel ID
   * \return true if packet was forwarded, false otherwise
   */
  bool ProcessSwitchForwarding (Ptr<Packet> packet,
                                const EthernetHeader& ethHeader,
                                Ptr<PointToPointSueNetDevice> currentDevice,
                                uint16_t protocol,
                                uint8_t vcId);

private:
  /**
   * \brief Copy constructor
   *
   * The method is private, so it is DISABLED.
   *
   * \param o Other SueSwitch
   */
  SueSwitch (const SueSwitch &o);

  /**
   * \brief Assignment operator
   *
   * The method is private, so it is DISABLED.
   *
   * \param o Other SueSwitch
   * \return Reference to this SueSwitch
   */
  SueSwitch& operator = (const SueSwitch &o);

  /**
   * \brief Forwarding table for switches
   * Maps destination MAC addresses to output port indices
   */
  std::map<Mac48Address, uint32_t> m_forwardingTable;
};

} // namespace ns3

#endif /* SUE_SWITCH_H */