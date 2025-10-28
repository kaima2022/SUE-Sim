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

#include "sue-cbfc-header.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (SueCbfcHeader);

TypeId SueCbfcHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SueCbfcHeader")
    .SetParent<Header> ()
    .SetGroupName ("PointToPointSue")
    .AddConstructor<SueCbfcHeader> ()
  ;
  return tid;
}
TypeId SueCbfcHeader::GetInstanceTypeId (void) const { return GetTypeId (); }
SueCbfcHeader::SueCbfcHeader () : m_vcId(0), m_credits(0) {}
SueCbfcHeader::~SueCbfcHeader () {}

uint32_t SueCbfcHeader::GetSerializedSize (void) const { return 2; }

void SueCbfcHeader::Serialize (Buffer::Iterator start) const {
  start.WriteU8 (m_vcId);
  start.WriteU8 (m_credits);
}
uint32_t SueCbfcHeader::Deserialize (Buffer::Iterator start) {
  m_vcId = start.ReadU8 ();
  m_credits = start.ReadU8 ();
  return GetSerializedSize ();
}
void SueCbfcHeader::Print (std::ostream &os) const {
  os << "VC=" << (uint32_t)m_vcId << ", Credits=" << (uint32_t)m_credits;
}
void SueCbfcHeader::SetVcId (uint8_t vc) { m_vcId = vc; }
uint8_t SueCbfcHeader::GetVcId () const { return m_vcId; }
void SueCbfcHeader::SetCredits (uint8_t credits) { m_credits = credits; }
uint8_t SueCbfcHeader::GetCredits () const { return m_credits; }

} // namespace ns3