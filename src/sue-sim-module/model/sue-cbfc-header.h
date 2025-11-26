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

#ifndef SUE_CBFC_HEADER_H
#define SUE_CBFC_HEADER_H

#include "ns3/header.h"

namespace ns3 {

/**
 * \ingroup point-to-point-sue
 * \class SueCbfcHeader
 * \brief Header for Credit-Based Flow Control (CBFC) in SUE protocol
 *
 * This header implements the credit-based flow control mechanism
 * used in the SUE protocol. It carries virtual channel ID and
 * credit information for flow control.
 */
class SueCbfcHeader : public Header
{
public:
  SueCbfcHeader ();
  virtual ~SueCbfcHeader ();

  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;

  /**
   * \brief Print header contents to output stream
   * \param os The output stream
   */
  virtual void Print (std::ostream &os) const;

  /**
   * \brief Get the serialized size of the header
   * \return The size in bytes
   */
  virtual uint32_t GetSerializedSize (void) const;

  /**
   * \brief Serialize the header to a buffer
   * \param start The buffer iterator to write to
   */
  virtual void Serialize (Buffer::Iterator start) const;

  /**
   * \brief Deserialize the header from a buffer
   * \param start The buffer iterator to read from
   * \return The number of bytes read
   */
  virtual uint32_t Deserialize (Buffer::Iterator start);

  /**
   * \brief Set the Virtual Channel ID
   * \param vc The virtual channel ID (0-3)
   */
  void SetVcId (uint8_t vc);

  /**
   * \brief Get the Virtual Channel ID
   * \return The virtual channel ID
   */
  uint8_t GetVcId () const;

  /**
   * \brief Set the credit value
   * \param credits The number of credits (0 for data packets, >0 for credit packets)
   */
  void SetCredits (uint8_t credits);

  /**
   * \brief Get the credit value
   * \return The number of credits
   */
  uint8_t GetCredits () const;

private:
  uint8_t m_vcId;     ///< Virtual Channel ID (0-3)
  uint8_t m_credits;  ///< Credit count (0 for data packets, >0 for credit packets)
};

} // namespace ns3

#endif /* SUE_CBFC_HEADER_H */