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

#ifndef SUE_QUEUE_MANAGER_H
#define SUE_QUEUE_MANAGER_H

#include <map>
#include "ns3/packet.h"
#include "ns3/queue.h"
#include "ns3/ptr.h"
#include "ns3/object.h"
#include "ns3/queue-size.h"

namespace ns3 {

/**
 * \brief Virtual Channel Queue Manager
 *
 * This class manages virtual channel queues and capacity reservation,
 * providing functionality for queue operations and capacity management.
 * It is independent of CBFC credit management.
 */
class SueQueueManager : public Object
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /**
   * Construct a SueQueueManager
   */
  SueQueueManager ();

  /**
   * Destroy a SueQueueManager
   */
  virtual ~SueQueueManager ();

  /**
   * \brief Initialize queue manager with configuration parameters
   *
   * Sets up virtual channel queues and initial state with the given parameters.
   *
   * \param numVcs Number of virtual channels
   * \param vcQueueMaxBytes Maximum VC queue size in bytes
   * \param additionalHeaderSize Additional header size for capacity reservation
   */
  void Initialize (uint8_t numVcs,
                  uint32_t vcQueueMaxBytes,
                  uint32_t additionalHeaderSize,
                  ns3::Callback<void, ns3::Ptr<const ns3::Packet>> dropCallback = ns3::Callback<void, ns3::Ptr<const ns3::Packet>>());

  /**
   * \brief Check if queue manager is initialized
   *
   * \return True if initialized
   */
  bool IsInitialized (void) const;

  /**
   * \brief Get the number of virtual channels
   *
   * \return Number of virtual channels
   */
  uint8_t GetNumVcs (void) const;

  /**
   * \brief Get VC queue maximum size in bytes
   *
   * \return VC queue maximum size
   */
  uint32_t GetVcQueueMaxBytes (void) const;

  /**
   * \brief Get additional header size
   *
   * \return Additional header size
   */
  uint32_t GetAdditionalHeaderSize (void) const;

  /**
   * \brief Get the available capacity for a virtual channel queue
   *
   * \param vcId Virtual channel ID
   * \return Available capacity in bytes
   */
  uint32_t GetVcAvailableCapacity (uint8_t vcId) const;

  /**
   * \brief Reserve capacity in a virtual channel queue
   *
   * \param vcId Virtual channel ID
   * \param amount Amount of capacity to reserve in bytes
   * \return True if capacity was successfully reserved
   */
  bool ReserveVcCapacity (uint8_t vcId, uint32_t amount);

  /**
   * \brief Release reserved capacity in a virtual channel queue
   *
   * \param vcId Virtual channel ID
   * \param amount Amount of capacity to release in bytes
   */
  void ReleaseVcCapacity (uint8_t vcId, uint32_t amount);

  /**
   * \brief Enqueue a packet to a virtual channel queue
   *
   * \param packet Packet to enqueue
   * \param vcId Virtual channel ID
   * \return True if packet was successfully enqueued
   */
  bool EnqueueToVcQueue (Ptr<Packet> packet, uint8_t vcId);

  /**
   * \brief Dequeue a packet from a virtual channel queue
   *
   * \param vcId Virtual channel ID
   * \return Dequeued packet, or null if queue is empty
   */
  Ptr<Packet> DequeueFromVcQueue (uint8_t vcId);

  /**
   * \brief Check if a virtual channel queue is empty
   *
   * \param vcId Virtual channel ID
   * \return True if queue is empty
   */
  bool IsVcQueueEmpty (uint8_t vcId) const;

  /**
   * \brief Get the number of packets in a virtual channel queue
   *
   * \param vcId Virtual channel ID
   * \return Number of packets in the queue
   */
  uint32_t GetVcQueueSize (uint8_t vcId) const;

  /**
   * \brief Get the number of bytes in a virtual channel queue
   *
   * \param vcId Virtual channel ID
   * \return Number of bytes in the queue
   */
  uint32_t GetVcQueueBytes (uint8_t vcId) const;

  /**
   * \brief Get the virtual channel queue for a specific VC
   *
   * \param vcId Virtual channel ID
   * \return Pointer to the VC queue
   */
  Ptr<Queue<Packet>> GetVcQueue (uint8_t vcId) const;

private:
  bool m_initialized;                    //!< Queue manager initialization flag
  uint8_t m_numVcs;                      //!< Number of virtual channels
  uint32_t m_vcQueueMaxBytes;            //!< VC queue maximum bytes
  uint32_t m_additionalHeaderSize;       //!< Additional header size for capacity reservation

  // Virtual channel queues and capacity management
  std::map<uint8_t, Ptr<Queue<Packet>>> m_vcQueues;        //!< Virtual channel queues
  std::map<uint8_t, uint32_t> m_vcReservedCapacity;        //!< VC reserved capacity for pending sends

  // Drop callback handling
  ns3::Callback<void, ns3::Ptr<const ns3::Packet>> m_dropCallback;  //!< Drop packet callback handler
};

} // namespace ns3

#endif /* SUE_QUEUE_MANAGER_H */