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

#include "sue-tag.h"
#include "ns3/log.h"
#include "ns3/packet.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SueTag");

NS_OBJECT_ENSURE_REGISTERED (SueTag);

TypeId
SueTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SueTag")
                          .SetParent<Tag> ()
                          .SetGroupName ("PointToPointSue")
                          .AddConstructor<SueTag> ();
  return tid;
}

TypeId
SueTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
SueTag::GetSerializedSize (void) const
{
  // Time: 8 bytes + Sequence: 4 bytes + LinkType: 1 byte = 13 bytes
  return 13;
}

void
SueTag::Serialize (TagBuffer buf) const
{
  int64_t timeValue = m_timestamp.GetNanoSeconds ();
  buf.WriteU64 (static_cast<uint64_t> (timeValue));
  buf.WriteU32 (m_sequence);
  buf.WriteU8 (m_linkType);
}

void
SueTag::Deserialize (TagBuffer buf)
{
  uint64_t timeValue = buf.ReadU64 ();
  m_timestamp = NanoSeconds (static_cast<int64_t> (timeValue));
  m_sequence = buf.ReadU32 ();
  m_linkType = buf.ReadU8 ();
}

void
SueTag::Print (std::ostream &os) const
{
  const char* linkTypeName[] = {"NIC", "SwitchIngress", "SwitchEgress"};
  std::string linkTypeStr = (m_linkType < 3) ? linkTypeName[m_linkType] : "Unknown";

  os << "SueTimestamp=" << m_timestamp.GetNanoSeconds () << "ns"
     << ", Sequence=" << m_sequence
     << ", LinkType=" << linkTypeStr << "(" << (uint32_t)m_linkType << ")";
}

SueTag::SueTag ()
  : m_timestamp (Time (0)), m_sequence (0), m_linkType (0)
{
  NS_LOG_FUNCTION (this);
}

SueTag::SueTag (Time timestamp)
  : m_timestamp (timestamp), m_sequence (0), m_linkType (0)
{
  NS_LOG_FUNCTION (this << timestamp);
}

SueTag::SueTag (Time timestamp, uint32_t seq)
  : m_timestamp (timestamp), m_sequence (seq), m_linkType (0)
{
  NS_LOG_FUNCTION (this << timestamp << seq);
}

void
SueTag::SetTimestamp (Time timestamp)
{
  NS_LOG_FUNCTION (this << timestamp);
  m_timestamp = timestamp;
}

Time
SueTag::GetTimestamp (void) const
{
  NS_LOG_FUNCTION (this);
  return m_timestamp;
}

void
SueTag::SetSequence (uint32_t seq)
{
  NS_LOG_FUNCTION (this << seq);
  m_sequence = seq;
}

uint32_t
SueTag::GetSequence (void) const
{
  NS_LOG_FUNCTION (this);
  return m_sequence;
}

void
SueTag::SetLinkType (uint8_t linkType)
{
  NS_LOG_FUNCTION (this << linkType);
  m_linkType = linkType;
}

uint8_t
SueTag::GetLinkType (void) const
{
  NS_LOG_FUNCTION (this);
  return m_linkType;
}

void
SueTag::UpdateTimestampInPacket (Ptr<Packet> packet, Time newTimestamp)
{
  NS_LOG_FUNCTION (packet << newTimestamp);

  SueTag tag;
  if (packet->PeekPacketTag(tag)) {
    packet->RemovePacketTag(tag);
    tag.SetTimestamp(newTimestamp);
    packet->AddPacketTag(tag);
  }
}

void
SueTag::UpdateSequenceAndLinkTypeInPacket (Ptr<Packet> packet, uint32_t newSeq, uint8_t newLinkType)
{
  NS_LOG_FUNCTION (packet << newSeq << newLinkType);

  SueTag tag;
  if (packet->PeekPacketTag(tag)) {
    packet->RemovePacketTag(tag);
    tag.m_sequence = newSeq; // Directly update the sequence number
    tag.SetLinkType(newLinkType);
    packet->AddPacketTag(tag);
  }
}

} // namespace ns3