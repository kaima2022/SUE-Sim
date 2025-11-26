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

private:
  Time m_timestamp; //!< SUE transmission timestamp
  uint32_t m_sequence; //!< LLR sequence number
  uint8_t m_linkType; //!< Link type: 0=NIC, 1=Switch Ingress, 2=Switch Egress
};

} // namespace ns3

#endif /* SUE_TAG_H */