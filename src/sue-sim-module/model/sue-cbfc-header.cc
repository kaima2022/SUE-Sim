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