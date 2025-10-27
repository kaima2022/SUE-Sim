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

#include "xpu-delay-tag.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("XpuDelayTag");

NS_OBJECT_ENSURE_REGISTERED (XpuDelayTag);

TypeId
XpuDelayTag::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::XpuDelayTag")
                          .SetParent<Tag> ()
                          .SetGroupName ("PointToPointSue")
                          .AddConstructor<XpuDelayTag> ();
  return tid;
}

TypeId
XpuDelayTag::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

uint32_t
XpuDelayTag::GetSerializedSize (void) const
{
  // Time serialization typically uses 8 bytes for int64_t
  return 8;
}

void
XpuDelayTag::Serialize (TagBuffer buf) const
{
  int64_t timeValue = m_timestamp.GetNanoSeconds ();
  buf.WriteU64 (static_cast<uint64_t> (timeValue));
}

void
XpuDelayTag::Deserialize (TagBuffer buf)
{
  uint64_t timeValue = buf.ReadU64 ();
  m_timestamp = NanoSeconds (static_cast<int64_t> (timeValue));
}

void
XpuDelayTag::Print (std::ostream &os) const
{
  os << "XpuTimestamp=" << m_timestamp.GetNanoSeconds () << "ns";
}

XpuDelayTag::XpuDelayTag ()
  : m_timestamp (Time (0))
{
  NS_LOG_FUNCTION (this);
}

XpuDelayTag::XpuDelayTag (Time timestamp)
  : m_timestamp (timestamp)
{
  NS_LOG_FUNCTION (this << timestamp);
}

void
XpuDelayTag::SetTimestamp (Time timestamp)
{
  NS_LOG_FUNCTION (this << timestamp);
  m_timestamp = timestamp;
}

Time
XpuDelayTag::GetTimestamp (void) const
{
  NS_LOG_FUNCTION (this);
  return m_timestamp;
}

} // namespace ns3