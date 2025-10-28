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

#include "sue-header.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SueHeader");
NS_OBJECT_ENSURE_REGISTERED(SueHeader);

SueHeader::SueHeader()
    : m_psn(0),
      m_xpuid(0),
      m_op_ver_rsv(0),
      m_rpsn(0),
      m_vc_part(0)
{
}

SueHeader::~SueHeader()
{
}

TypeId
SueHeader::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::SueHeader")
                            .SetParent<Header>()
                            .SetGroupName("Applications")
                            .AddConstructor<SueHeader>();
    return tid;
}

TypeId
SueHeader::GetInstanceTypeId(void) const
{
    return GetTypeId();
}

// Total size in bytes. The RH is 8 bytes total as per the spec.
// (psn+xpuid+op+vc+rsv...) = 16+10+2+2+... = 64 bits = 8 bytes
uint32_t
SueHeader::GetSerializedSize(void) const
{
    return 8; 
}

void
SueHeader::Serialize(Buffer::Iterator start) const
{
    // Serialize the header fields into the buffer.
    // We follow the layout from Figure 8 as closely as possible.
    // First 32 bits
    uint32_t first_word = 0;
    first_word |= (GetOp() & 0x03) << 30;     // op: 2 bits
    // 'ver' and 'rsv' are ignored for now
    first_word |= (GetXpuId() & 0x03FF) << 16; // xpuid: 10 bits
    first_word |= (GetPsn() & 0xFFFF);        // psn: 16 bits
    start.WriteHtonU32(first_word);

    // Second 32 bits
    uint32_t second_word = 0;
    second_word |= (GetVc() & 0x03) << 30; // vc: 2 bits
    // 'rsvd' and 'partition' are ignored for now
    second_word |= (GetRpsn() & 0xFFFF);   // rpsn: 16 bits
    start.WriteHtonU32(second_word);
}

uint32_t
SueHeader::Deserialize(Buffer::Iterator start)
{
    // Deserialize from the buffer back into the header fields.
    uint32_t first_word = start.ReadNtohU32();
    SetOp((first_word >> 30) & 0x03);
    SetXpuId((first_word >> 16) & 0x03FF);
    SetPsn(first_word & 0xFFFF);

    uint32_t second_word = start.ReadNtohU32();
    SetVc((second_word >> 30) & 0x03);
    SetRpsn(second_word & 0xFFFF);
    
    return GetSerializedSize();
}

void
SueHeader::Print(std::ostream& os) const
{
    os << "SUE Header (PSN=" << GetPsn() << ", RPSN=" << GetRpsn() << ", XPU_ID=" << GetXpuId()
       << ", VC=" << (uint32_t)GetVc() << ", OP=" << (uint32_t)GetOp() << ")";
}

// --- Getters and Setters ---
// Note: These now correctly pack/unpack from the member variables.
void SueHeader::SetPsn(uint16_t psn) { m_psn = psn; }
uint16_t SueHeader::GetPsn(void) const { return m_psn; }
void SueHeader::SetRpsn(uint16_t rpsn) { m_rpsn = rpsn; }
uint16_t SueHeader::GetRpsn(void) const { return m_rpsn; }
void SueHeader::SetXpuId(uint16_t xpuid) { m_xpuid = xpuid & 0x03FF; } // Mask to 10 bits
uint16_t SueHeader::GetXpuId(void) const { return m_xpuid; }
void SueHeader::SetVc(uint8_t vc) { m_vc_part = (m_vc_part & 0x3FFF) | ((vc & 0x03) << 14); }
uint8_t SueHeader::GetVc(void) const { return (m_vc_part >> 14) & 0x03; }
void SueHeader::SetOp(uint8_t op) { m_op_ver_rsv = (m_op_ver_rsv & 0x3F) | ((op & 0x03) << 6); }
uint8_t SueHeader::GetOp(void) const { return (m_op_ver_rsv >> 6) & 0x03; }

} // namespace ns3