/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2025 SUE-Sim Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef SUE_SWITCH_H
#define SUE_SWITCH_H

#include <map>
#include "ns3/mac48-address.h"
#include "ns3/ethernet-header.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/nstime.h"

namespace ns3 {

// Forward declarations
class PointToPointSueNetDevice;
class LlrNodeManager;
class LlrSwitchPortManager;

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

  /**
   * \brief Calculate adaptive forwarding delay based on packet size
   *
   * \param device Current net device
   * \param packetSize Size of the packet in bytes
   * \return Adaptive forwarding delay
   */
  Time CalculateAdaptiveForwardDelay (Ptr<PointToPointSueNetDevice> device, uint32_t packetSize);

  /**
   * \brief Set LLR node manager for switch
   *
   * \param llrNodeManager Pointer to LLR node manager
   */
  void SetLlrNodeManager (Ptr<LlrNodeManager> llrNodeManager);

  /**
   * \brief Set LLR switch port manager for switch
   *
   * \param llrSwitchPortManager Pointer to LLR switch port manager
   */
  void SetLlrSwitchPortManager (Ptr<LlrSwitchPortManager> llrSwitchPortManager);

  /**
   * \brief Get LLR node manager
   *
   * \return Pointer to LLR node manager
   */
  Ptr<LlrNodeManager> GetLlrNodeManager (void) const;

  /**
   * \brief Get LLR switch port manager
   *
   * \return Pointer to LLR switch port manager
   */
  Ptr<LlrSwitchPortManager> GetLlrSwitchPortManager (void) const;

  /**
   * \brief Check if a MAC address belongs to a switch device
   *
   * \param MAC address to check
   * \return true if the MAC address belongs to a switch device
   */
  bool IsSwitchDevice (Mac48Address mac) const;

  /**
   * \brief Handle forwarding completion event
   *
   * \param originalDevice Original device that started forwarding
   * \param targetDevice Target device to enqueue to
   * \param packet Packet to forward
   * \param ethHeader Ethernet header for credit return
   * \param vcId Virtual channel ID
   * \param sourceMac Source MAC for credit return
   */
  void ForwardingComplete (Ptr<PointToPointSueNetDevice> originalDevice,
                           Ptr<PointToPointSueNetDevice> targetDevice,
                           Ptr<Packet> packet,
                           const EthernetHeader& ethHeader,
                           uint8_t vcId,
                           Mac48Address sourceMac);

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

  /// ---- Forwarding State Machine ----
  bool m_forwardingBusy;                                     //!< Forwarding state machine busy flag

  /// ---- LLR managers ----
  Ptr<LlrNodeManager> m_llrNodeManager;         //!< LLR manager for end nodes
  Ptr<LlrSwitchPortManager> m_llrSwitchPortManager; //!< LLR manager for switch ports
};

} // namespace ns3

#endif /* SUE_SWITCH_H */