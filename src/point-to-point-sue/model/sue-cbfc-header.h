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