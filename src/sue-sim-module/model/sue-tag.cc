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

#include "sue-tag.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

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
  // Time: 8 bytes + Sequence: 4 bytes + LinkType: 1 byte + VC Queue Time: 8 bytes + NodeId: 4 bytes + DeviceId: 4 bytes + VcId: 1 byte + Processing Queue Time: 8 bytes = 38 bytes
  return 38;
}

void
SueTag::Serialize (TagBuffer buf) const
{
  int64_t timeValue = m_timestamp.GetNanoSeconds ();
  buf.WriteU64 (static_cast<uint64_t> (timeValue));
  buf.WriteU32 (m_sequence);
  buf.WriteU8 (m_linkType);

  // VC Queue Delay Statistics fields
  int64_t vcTimeValue = m_vcQueueEnqueueTime.GetNanoSeconds ();
  buf.WriteU64 (static_cast<uint64_t> (vcTimeValue));
  buf.WriteU32 (m_nodeId);
  buf.WriteU32 (m_deviceId);
  buf.WriteU8 (m_vcId);

  // Processing Queue Delay Statistics fields
  int64_t processingTimeValue = m_processingQueueEnqueueTime.GetNanoSeconds ();
  buf.WriteU64 (static_cast<uint64_t> (processingTimeValue));
}

void
SueTag::Deserialize (TagBuffer buf)
{
  uint64_t timeValue = buf.ReadU64 ();
  m_timestamp = NanoSeconds (static_cast<int64_t> (timeValue));
  m_sequence = buf.ReadU32 ();
  m_linkType = buf.ReadU8 ();

  // VC Queue Delay Statistics fields
  uint64_t vcTimeValue = buf.ReadU64 ();
  m_vcQueueEnqueueTime = NanoSeconds (static_cast<int64_t> (vcTimeValue));
  m_nodeId = buf.ReadU32 ();
  m_deviceId = buf.ReadU32 ();
  m_vcId = buf.ReadU8 ();

  // Processing Queue Delay Statistics fields
  uint64_t processingTimeValue = buf.ReadU64 ();
  m_processingQueueEnqueueTime = NanoSeconds (static_cast<int64_t> (processingTimeValue));
}

void
SueTag::Print (std::ostream &os) const
{
  const char* linkTypeName[] = {"NIC", "SwitchIngress", "SwitchEgress"};
  std::string linkTypeStr = (m_linkType < 3) ? linkTypeName[m_linkType] : "Unknown";

  os << "SueTimestamp=" << m_timestamp.GetNanoSeconds () << "ns"
     << ", Sequence=" << m_sequence
     << ", LinkType=" << linkTypeStr << "(" << (uint32_t)m_linkType << ")"
     << ", VCQueueTime=" << m_vcQueueEnqueueTime.GetNanoSeconds () << "ns"
     << ", ProcessingQueueTime=" << m_processingQueueEnqueueTime.GetNanoSeconds () << "ns"
     << ", NodeId=" << m_nodeId
     << ", DeviceId=" << m_deviceId
     << ", VcId=" << (uint32_t)m_vcId;
}

SueTag::SueTag ()
  : m_timestamp (Time (0)), m_sequence (0), m_linkType (0), m_vcQueueEnqueueTime (Time (0)), m_nodeId (0), m_deviceId (0), m_vcId (0), m_processingQueueEnqueueTime (Time (0))
{
  NS_LOG_FUNCTION (this);
}

SueTag::SueTag (Time timestamp)
  : m_timestamp (timestamp), m_sequence (0), m_linkType (0), m_vcQueueEnqueueTime (Time (0)), m_nodeId (0), m_deviceId (0), m_vcId (0), m_processingQueueEnqueueTime (Time (0))
{
  NS_LOG_FUNCTION (this << timestamp);
}

SueTag::SueTag (Time timestamp, uint32_t seq)
  : m_timestamp (timestamp), m_sequence (seq), m_linkType (0), m_vcQueueEnqueueTime (Time (0)), m_nodeId (0), m_deviceId (0), m_vcId (0), m_processingQueueEnqueueTime (Time (0))
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

// === VC Queue Delay Statistics Implementation ===

void
SueTag::AddVcQueueDelayTag (Ptr<Packet> packet, uint32_t nodeId, uint32_t deviceId, uint8_t vcId)
{
  NS_LOG_FUNCTION (packet << nodeId << deviceId << static_cast<uint32_t> (vcId));

  SueTag tag;
  if (packet->PeekPacketTag(tag)) {
    // Tag already exists, update it
    packet->RemovePacketTag(tag);
    tag.m_vcQueueEnqueueTime = Simulator::Now (); // Update VC queue enqueue time
    tag.m_nodeId = nodeId;
    tag.m_deviceId = deviceId;
    tag.m_vcId = vcId;
    packet->AddPacketTag(tag);
  } else {
    // No existing tag, create new one
    tag.m_vcQueueEnqueueTime = Simulator::Now ();
    tag.m_nodeId = nodeId;
    tag.m_deviceId = deviceId;
    tag.m_vcId = vcId;
    packet->AddPacketTag(tag);
  }
}

bool
SueTag::ExtractVcQueueDelay (Ptr<Packet> packet, Time currentTime,
                             Time& outDelay, uint32_t& outNodeId, uint32_t& outDeviceId, uint8_t& outVcId)
{
  NS_LOG_FUNCTION (packet << currentTime);

  SueTag tag;
  if (packet->PeekPacketTag (tag))
    {
      if (currentTime > tag.m_vcQueueEnqueueTime)
        {
          outDelay = currentTime - tag.m_vcQueueEnqueueTime;
        }
      else
        {
          outDelay = Time (0);
        }
      outNodeId = tag.m_nodeId;
      outDeviceId = tag.m_deviceId;
      outVcId = tag.m_vcId;
      return true;
    }
  return false;
}

// === Processing Queue Delay Statistics Implementation ===

void
SueTag::AddProcessingQueueDelayTag (Ptr<Packet> packet)
{
  NS_LOG_FUNCTION (packet);

  SueTag tag;
  if (packet->PeekPacketTag(tag)) {
    // Tag already exists, update it
    packet->RemovePacketTag(tag);
    tag.m_processingQueueEnqueueTime = Simulator::Now (); // Update processing queue enqueue time
    packet->AddPacketTag(tag);
  } else {
    // No existing tag, create new one
    tag.m_processingQueueEnqueueTime = Simulator::Now ();
    packet->AddPacketTag(tag);
  }
}

bool
SueTag::ExtractProcessingQueueDelay (Ptr<Packet> packet, Time currentTime, Time& outDelay)
{
  NS_LOG_FUNCTION (packet << currentTime);

  SueTag tag;
  if (packet->PeekPacketTag (tag))
    {
      if (currentTime > tag.m_processingQueueEnqueueTime)
        {
          outDelay = currentTime - tag.m_processingQueueEnqueueTime;
        }
      else
        {
          outDelay = Time (0);
        }
      return true;
    }
  return false;
}

} // namespace ns3