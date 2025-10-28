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

#ifndef XPU_DELAY_TAG_H
#define XPU_DELAY_TAG_H

#include "ns3/tag.h"
#include "ns3/nstime.h"

namespace ns3 {

/**
 * \brief Tag to store XPU transmission timestamp for delay measurement
 *
 * This tag is added to packets when they are transmitted from XPU devices
 * and is used to measure end-to-end delay from XPU to XPU Server.
 */
class XpuDelayTag : public Tag
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Get the instance TypeId.
   * \return the instance TypeId
   */
  virtual TypeId GetInstanceTypeId (void) const;

  /**
   * \brief Get the serialized size of the tag.
   * \return the serialized size of the tag
   */
  virtual uint32_t GetSerializedSize (void) const;

  /**
   * \brief Serialize the tag to the given buffer.
   * \param buf the buffer to serialize to
   */
  virtual void Serialize (TagBuffer buf) const;

  /**
   * \brief Deserialize the tag from the given buffer.
   * \param buf the buffer to deserialize from
   */
  virtual void Deserialize (TagBuffer buf);

  /**
   * \brief Print the tag to the given output stream.
   * \param os the output stream to print to
   */
  virtual void Print (std::ostream &os) const;

  /**
   * \brief Constructor
   */
  XpuDelayTag ();

  /**
   * \brief Constructor with timestamp
   * \param timestamp the XPU transmission timestamp
   */
  XpuDelayTag (Time timestamp);

  /**
   * \brief Set the XPU transmission timestamp
   * \param timestamp the transmission timestamp
   */
  void SetTimestamp (Time timestamp);

  /**
   * \brief Get the XPU transmission timestamp
   * \return the transmission timestamp
   */
  Time GetTimestamp (void) const;

private:
  Time m_timestamp; //!< XPU transmission timestamp
};

} // namespace ns3

#endif /* XPU_DELAY_TAG_H */