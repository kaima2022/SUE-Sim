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

#ifndef SUE_TAG_H
#define SUE_TAG_H

#include "ns3/tag.h"
#include "ns3/nstime.h"

namespace ns3 {
    class Packet;
}

namespace ns3 {

/**
 * \brief Tag to store SUE transmission timestamp and PPP sequence number for delay measurement
 *
 * This tag is added to packets when they are transmitted from SUE devices
 * and is used to measure end-to-end delay from SUE to SUE Server.
 */
class SueTag : public Tag
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Get the instance TypeId.
   * \return the instance TypeId
   */
  virtual TypeId GetInstanceTypeId (void) const;

  /**
   * \brief Get the serialized size of the tag.
   * \return the serialized size of the tag
   */
  virtual uint32_t GetSerializedSize (void) const;

  /**
   * \brief Serialize the tag to the given buffer.
   * \param buf the buffer to serialize to
   */
  virtual void Serialize (TagBuffer buf) const;

  /**
   * \brief Deserialize the tag from the given buffer.
   * \param buf the buffer to deserialize from
   */
  virtual void Deserialize (TagBuffer buf);

  /**
   * \brief Print the tag to the given output stream.
   * \param os the output stream to print to
   */
  virtual void Print (std::ostream &os) const;

  /**
   * \brief Constructor
   */
  SueTag ();

  /**
   * \brief Constructor with timestamp
   * \param timestamp the SUE transmission timestamp
   */
  SueTag (Time timestamp);

  /**
   * \brief Constructor with timestamp and sequence number
   * \param timestamp the SUE transmission timestamp
   * \param seq the PPP sequence number
   */
  SueTag (Time timestamp, uint32_t seq);

  /**
   * \brief Set the SUE transmission timestamp
   * \param timestamp the transmission timestamp
   */
  void SetTimestamp (Time timestamp);

  /**
   * \brief Get the SUE transmission timestamp
   * \return the transmission timestamp
   */
  Time GetTimestamp (void) const;

  /**
   * \brief Set the PPP sequence number
   * \param seq the PPP sequence number
   */
  void SetSequence (uint32_t seq);

  /**
   * \brief Get the PPP sequence number
   * \return the PPP sequence number
   */
  uint32_t GetSequence (void) const;

  /**
   * \brief Set the link type
   * \param linkType the link type (0=NIC, 1=Switch Ingress, 2=Switch Egress)
   */
  void SetLinkType (uint8_t linkType);

  /**
   * \brief Get the link type
   * \return the link type
   */
  uint8_t GetLinkType (void) const;

  /**
   * \brief Update tag timestamp in packet
   * \param packet the packet containing this tag
   * \param newTimestamp the new timestamp
   */
  static void UpdateTimestampInPacket (Ptr<Packet> packet, Time newTimestamp);

  /**
   * \brief Update tag sequence and link type in packet
   * \param packet the packet containing this tag
   * \param newSeq the new sequence number
   * \param newLinkType the new link type
   */
  static void UpdateSequenceAndLinkTypeInPacket (Ptr<Packet> packet, uint32_t newSeq, uint8_t newLinkType);

  // === VC Queue Delay Statistics ===

  /**
   * \brief Add VC queue delay tag to packet with current time
   * \param packet the packet to tag
   * \param nodeId the node ID
   * \param deviceId the device ID
   * \param vcId the VC queue ID
   */
  static void AddVcQueueDelayTag (Ptr<Packet> packet, uint32_t nodeId, uint32_t deviceId, uint8_t vcId);

  /**
   * \brief Extract and calculate VC queue delay from packet tag
   * \param packet the packet containing the tag
   * \param currentTime the current time
   * \param outDelay output parameter for calculated delay
   * \param outNodeId output parameter for node ID
   * \param outDeviceId output parameter for device ID
   * \param outVcId output parameter for VC ID
   * \return true if tag was found and delay calculated, false otherwise
   */
  static bool ExtractVcQueueDelay (Ptr<Packet> packet, Time currentTime,
                                   Time& outDelay, uint32_t& outNodeId, uint32_t& outDeviceId, uint8_t& outVcId);

  // === Processing Queue Delay Statistics ===

  /**
   * \brief Add processing queue delay tag to packet with current time
   * \param packet the packet to tag
   */
  static void AddProcessingQueueDelayTag (Ptr<Packet> packet);

  /**
   * \brief Extract and calculate processing queue delay from packet tag
   * \param packet the packet containing the tag
   * \param currentTime the current time
   * \param outDelay output parameter for calculated delay
   * \return true if tag was found and delay calculated, false otherwise
   */
  static bool ExtractProcessingQueueDelay (Ptr<Packet> packet, Time currentTime, Time& outDelay);

private:
  Time m_timestamp; //!< SUE transmission timestamp
  uint32_t m_sequence; //!< LLR sequence number
  uint8_t m_linkType; //!< Link type: 0=NIC, 1=Switch Ingress, 2=Switch Egress

  // VC Queue Delay Statistics fields
  Time m_vcQueueEnqueueTime; //!< VC queue enqueue time for delay tracking
  uint32_t m_nodeId;        //!< Node ID for VC queue delay tracking
  uint32_t m_deviceId;      //!< Device ID for VC queue delay tracking
  uint8_t m_vcId;           //!< VC queue ID for delay tracking

  // Processing Queue Delay Statistics fields
  Time m_processingQueueEnqueueTime; //!< Processing queue enqueue time for delay tracking
};

} // namespace ns3

#endif /* SUE_TAG_H */