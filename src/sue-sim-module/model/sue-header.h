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

#ifndef SUE_HEADER_H
#define SUE_HEADER_H

#include "ns3/header.h"

namespace ns3 {

/**
 * \ingroup point-to-point-sue
 * \class SueHeader
 * \brief The Scale-Up Ethernet (SUE) Reliability Header.
 *
 * This header represents the SUE Reliability Header with an 8-byte size.
 * The header includes fields for packet sequence numbers, XPU identification,
 * virtual channel information, and operation codes for reliability and
 * flow control purposes.
 */
class SueHeader : public Header
{
public:
  /**
   * \brief Constructor
   */
  SueHeader ();

  /**
   * \brief Destructor
   */
  virtual ~SueHeader ();

  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Get the instance TypeId
   *
   * \return The instance TypeId
   */
  TypeId GetInstanceTypeId (void) const override;

  /**
   * \brief Print the header contents
   *
   * \param os The output stream
   */
  void Print (std::ostream &os) const override;

  /**
   * \brief Get the serialized size of the header
   *
   * \return The size in bytes
   */
  uint32_t GetSerializedSize (void) const override;

  /**
   * \brief Serialize the header to a buffer
   *
   * \param start The buffer iterator
   */
  void Serialize (Buffer::Iterator start) const override;

  /**
   * \brief Deserialize the header from a buffer
   *
   * \param start The buffer iterator
   * \return The number of bytes deserialized
   */
  uint32_t Deserialize (Buffer::Iterator start) override;

  /**
   * \brief Set the packet sequence number
   *
   * \param psn Packet sequence number
   */
  void SetPsn (uint16_t psn);

  /**
   * \brief Get the packet sequence number
   *
   * \return Packet sequence number
   */
  uint16_t GetPsn (void) const;

  /**
   * \brief Set the response packet sequence number
   *
   * \param rpsn Response packet sequence number
   */
  void SetRpsn (uint16_t rpsn);

  /**
   * \brief Get the response packet sequence number
   *
   * \return Response packet sequence number
   */
  uint16_t GetRpsn (void) const;

  /**
   * \brief Set the XPU identifier
   *
   * \param xpuid XPU identifier
   */
  void SetXpuId (uint16_t xpuid);

  /**
   * \brief Get the XPU identifier
   *
   * \return XPU identifier
   */
  uint16_t GetXpuId (void) const;

  /**
   * \brief Set the virtual channel ID
   *
   * \param vc Virtual channel ID
   */
  void SetVc (uint8_t vc);
  /**
   * \brief Get the virtual channel ID
   *
   * \return Virtual channel ID
   */
  uint8_t GetVc (void) const;

  /**
   * \brief Set the operation code
   *
   * \param op Operation code (0=data, 1=ACK, 2=NACK)
   */
  void SetOp (uint8_t op);

  /**
   * \brief Get the operation code
   *
   * \return Operation code
   */
  uint8_t GetOp (void) const;

private:
  /**
   * \brief Copy constructor
   *
   * The method is private, so it is DISABLED.
   *
   * \param o Other SueHeader
   */
  SueHeader (const SueHeader &o);

  /**
   * \brief Assignment operator
   *
   * The method is private, so it is DISABLED.
   *
   * \param o Other SueHeader
   * \return Reference to this SueHeader
   */
  SueHeader& operator = (const SueHeader &o);

  // First 4 bytes of the header
  uint16_t m_psn;      //!< Packet Sequence Number (16 bits)
  uint16_t m_xpuid;    //!< XPU Identifier (16 bits, simplified from 10 bits)
  uint8_t m_op_ver_rsv; //!< Operation, Version, and Reserved fields (8 bits)

  // Second 4 bytes of the header
  uint16_t m_rpsn;     //!< Response packet sequence number (16 bits)
  uint16_t m_vc_part;  //!< Virtual Channel, Reserved, and Partition fields (16 bits)
};

} // namespace ns3

#endif /* SUE_HEADER_H */