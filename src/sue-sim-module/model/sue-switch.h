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

#include <cstdint>
#include <deque>
#include <map>
#include <unordered_map>
#include "ns3/mac48-address.h"
#include "ns3/ethernet-header.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/nstime.h"

namespace ns3 {

// Forward declarations
class Node;
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
  enum class EgressOverflowPolicy : uint8_t
  {
    RETRY = 0,
    DROP = 1,
  };

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
   * \brief Configure switch egress overflow behavior.
   *
   * RETRY models a lossless switch: if the egress VC queue is full, forwarding
   * is retried later (backpressure). DROP models a lossy switch: if egress is
   * full, the packet is dropped.
   */
  void SetEgressOverflowPolicy (EgressOverflowPolicy policy);

  /**
   * \return Current egress overflow policy.
   */
  EgressOverflowPolicy GetEgressOverflowPolicy (void) const;

  /**
   * \brief Lookup output port index for a destination MAC.
   *
   * \return true on hit; writes to outPortIndex.
   */
  bool LookupOutPortIndex (const Mac48Address &destination, uint32_t *outPortIndex) const;

  /**
   * \brief Find a PointToPointSueNetDevice on a node by GetIfIndex().
   */
  Ptr<PointToPointSueNetDevice> FindPortDevice (Ptr<Node> node, uint32_t outPortIndex) const;

  /**
   * \brief Reserve bytes against an egress VC queue to guarantee later enqueue.
   *
   * This is used when modeling DROP semantics while keeping LLR correctness:
   * we must never emit LLR ACK for a packet that would later be dropped inside
   * the switch due to egress queue overflow.
   *
   * \return true if the reservation succeeded.
   */
  bool TryReserveEgressVcQueueBytes (uint32_t outPortIndex,
                                     uint8_t vcId,
                                     uint32_t bytes,
                                     Ptr<PointToPointSueNetDevice> targetDevice);

  /**
   * \brief Release previously reserved egress VC queue bytes.
   */
  void ReleaseEgressVcQueueBytes (uint32_t outPortIndex, uint8_t vcId, uint32_t bytes);

  /**
   * \brief Return the currently reserved bytes against an egress VC queue.
   *
   * Reservation is used only when EgressOverflowPolicy=DROP to avoid emitting
   * LLR ACK/NACK for packets that would later be dropped due to egress
   * admission failure.
   */
  uint64_t GetReservedEgressVcQueueBytes (uint32_t outPortIndex, uint8_t vcId) const;

  
  /**
   * \brief Process packet forwarding through switch
   *
   * \param packet Packet to forward
   * \param ethHeader Ethernet header of the packet
   * \param currentDevice Current net device processing the packet
   * \param protocol Protocol number
   * \param vcId Virtual Channel ID
   * \param skipSwitchInternalCbfcCredits If true, skip switch-internal CBFC
   *        credit check/consume (credits already consumed during admission).
   * \return true if packet was forwarded, false otherwise
   */
  bool ProcessSwitchForwarding (Ptr<Packet> packet,
                                const EthernetHeader& ethHeader,
                                Ptr<PointToPointSueNetDevice> currentDevice,
                                uint16_t protocol,
                                uint8_t vcId,
                                bool skipSwitchInternalCbfcCredits = false);

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
                           Mac48Address sourceMac,
                           uint32_t outPortIndex);

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

  struct ForwardingRequest {
    Ptr<PointToPointSueNetDevice> originalDevice;
    Ptr<PointToPointSueNetDevice> targetDevice;
    Ptr<Packet> packet;
    EthernetHeader ethHeader;
    uint8_t vcId = 0;
    Mac48Address sourceMac;
  };

  void StartNextOnEgressPort(uint32_t outPortIndex);

	  // Egress port scheduling: one in-flight forwarding per egress port, plus a FIFO
	  // queue to model switch pipeline per output.
	  std::unordered_map<uint32_t, bool> m_egressBusy;
	  std::unordered_map<uint32_t, std::deque<ForwardingRequest>> m_egressQueues;
	  std::unordered_map<uint32_t, std::unordered_map<uint8_t, uint64_t>> m_egressReservedBytes;
	  EgressOverflowPolicy m_egressOverflowPolicy = EgressOverflowPolicy::RETRY;

	  /// ---- LLR managers ----
	  Ptr<LlrNodeManager> m_llrNodeManager;         //!< LLR manager for end nodes
	  Ptr<LlrSwitchPortManager> m_llrSwitchPortManager; //!< LLR manager for switch ports
};

} // namespace ns3

#endif /* SUE_SWITCH_H */
