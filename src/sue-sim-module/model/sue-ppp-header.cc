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

#include <iostream>
#include "ns3/abort.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/header.h"
#include "sue-ppp-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SuePppHeader");

NS_OBJECT_ENSURE_REGISTERED (SuePppHeader);

SuePppHeader::SuePppHeader ()
{
}

SuePppHeader::~SuePppHeader ()
{
}

TypeId
SuePppHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SuePppHeader")
    .SetParent<Header> ()
    .SetGroupName ("PointToPointSue")
    .AddConstructor<SuePppHeader> ()
  ;
  return tid;
}

TypeId
SuePppHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}

void 
SuePppHeader::Print (std::ostream &os) const
{
  std::string proto;

  switch(m_protocol)
    {
    case 0x0021: /* IPv4 */
      proto = "IP (0x0021)";
      break;
    case 0x0057: /* IPv6 */
      proto = "IPv6 (0x0057)";
      break;
    case 0xCBFC:
      proto = "CBFC (0xCBFC)";
      break;
    case 0x1111:
      proto = "ACK_REV (0x1111)";
      break;
    case 0x2222:
      proto = "NACK_REV (0x2222)";
      break;
    default:
      NS_ASSERT_MSG (false, "PPP Protocol number not defined!");
    }
  os << "Point-to-Point Protocol: " << proto; 
}

uint32_t
SuePppHeader::GetSerializedSize (void) const
{
  return 2; // Only protocol field (2 bytes)
}

void
SuePppHeader::Serialize (Buffer::Iterator start) const
{
  start.WriteHtonU16 (m_protocol);
}

uint32_t
SuePppHeader::Deserialize (Buffer::Iterator start)
{
  m_protocol = start.ReadNtohU16 ();
  return GetSerializedSize ();
}

void
SuePppHeader::SetProtocol (uint16_t protocol)
{
  m_protocol=protocol;
}

uint16_t
SuePppHeader::GetProtocol (void)
{
  return m_protocol;
}




} // namespace ns3

