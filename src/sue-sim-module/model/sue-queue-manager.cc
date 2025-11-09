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

#include "sue-queue-manager.h"
#include "ns3/log.h"
#include "ns3/drop-tail-queue.h"
#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("SueQueueManager");

NS_OBJECT_ENSURE_REGISTERED(SueQueueManager);

TypeId
SueQueueManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SueQueueManager")
                          .SetParent<Object> ()
                          .SetGroupName ("PointToPointSue")
                          .AddConstructor<SueQueueManager> ()
                          .AddAttribute ("NumVcs", "The number of Virtual Channels.",
                                        UintegerValue (4),
                                        MakeUintegerAccessor (&SueQueueManager::m_numVcs),
                                        MakeUintegerChecker<uint8_t> ())
                          .AddAttribute ("VcQueueMaxBytes", "The VC queue maximum bytes.",
                                        UintegerValue (1048576), // 1MB default
                                        MakeUintegerAccessor (&SueQueueManager::m_vcQueueMaxBytes),
                                        MakeUintegerChecker<uint32_t> ())
                          .AddAttribute ("AdditionalHeaderSize", "Additional header size for capacity reservation.",
                                        UintegerValue (0),
                                        MakeUintegerAccessor (&SueQueueManager::m_additionalHeaderSize),
                                        MakeUintegerChecker<uint32_t> ());
  return tid;
}

SueQueueManager::SueQueueManager ()
  : m_initialized (false),
    m_numVcs (4),
    m_vcQueueMaxBytes (1048576),
    m_additionalHeaderSize (0)
{
  NS_LOG_FUNCTION (this);
}

SueQueueManager::~SueQueueManager ()
{
  NS_LOG_FUNCTION (this);
}

void
SueQueueManager::Initialize (uint8_t numVcs,
                           uint32_t vcQueueMaxBytes,
                           uint32_t additionalHeaderSize,
                           ns3::Callback<void, ns3::Ptr<const ns3::Packet>> dropCallback)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (numVcs) << vcQueueMaxBytes << additionalHeaderSize);

  if (m_initialized)
    {
      return;
    }

  m_numVcs = numVcs;
  m_vcQueueMaxBytes = vcQueueMaxBytes;
  m_additionalHeaderSize = additionalHeaderSize;
  m_dropCallback = dropCallback;

  // Clear existing data structures
  m_vcQueues.clear ();
  m_vcReservedCapacity.clear ();

  // Initialize virtual channel queues
  for (uint8_t i = 0; i < m_numVcs; ++i)
    {
      m_vcQueues[i] = CreateObject<DropTailQueue<Packet>> ();
      std::stringstream vcMaxSize;
      vcMaxSize << m_vcQueueMaxBytes << "B";
      m_vcQueues[i]->SetAttribute ("MaxSize", QueueSizeValue (QueueSize (vcMaxSize.str ())));

      // Connect Drop trace source to callback handler if provided
      if (!m_dropCallback.IsNull())
      {
        m_vcQueues[i]->TraceConnectWithoutContext("Drop", m_dropCallback);
      }

      // Initialize reserved capacity
      m_vcReservedCapacity[i] = 0;
    }

  m_initialized = true;
  NS_LOG_INFO ("SueQueueManager initialized with " << static_cast<uint32_t> (m_numVcs)
               << " VCs and max queue size " << m_vcQueueMaxBytes << " bytes.");
}

bool
SueQueueManager::IsInitialized (void) const
{
  return m_initialized;
}

uint8_t
SueQueueManager::GetNumVcs (void) const
{
  return m_numVcs;
}

uint32_t
SueQueueManager::GetVcQueueMaxBytes (void) const
{
  return m_vcQueueMaxBytes;
}

uint32_t
SueQueueManager::GetAdditionalHeaderSize (void) const
{
  return m_additionalHeaderSize;
}

uint32_t
SueQueueManager::GetVcAvailableCapacity (uint8_t vcId) const
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (vcId));

  if (vcId >= m_numVcs)
    {
      NS_LOG_WARN ("Invalid VC ID: " << static_cast<uint32_t> (vcId));
      return 0;
    }

  // Check if VC queue pointer is valid, if invalid then not initialized yet
  auto it = m_vcQueues.find (vcId);
  if (it == m_vcQueues.end () || !it->second)
    {
      return m_vcQueueMaxBytes;
    }

  // Get current VC queue used bytes
  uint32_t currentBytes = it->second->GetNBytes ();
  // Get VC queue maximum capacity
  uint32_t maxBytes = m_vcQueueMaxBytes;
  // Get reserved capacity
  auto reserveIt = m_vcReservedCapacity.find (vcId);
  uint32_t reservedBytes = (reserveIt != m_vcReservedCapacity.end ()) ? reserveIt->second : 0;

  // Calculate actual available capacity (total capacity - used - reserved)
  uint32_t usedBytes = currentBytes + reservedBytes;

  // Return available capacity (bytes)
  if (usedBytes >= maxBytes)
    {
      return 0;
    }

  return maxBytes - usedBytes;
}

bool
SueQueueManager::ReserveVcCapacity (uint8_t vcId, uint32_t amount)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (vcId) << amount);

  if (vcId >= m_numVcs)
    {
      NS_LOG_WARN ("Invalid VC ID: " << static_cast<uint32_t> (vcId));
      return false;
    }

  // Check if there's enough available capacity for reservation
  // Need to reserve packet size plus additional header size
  uint32_t totalReservationSize = amount + m_additionalHeaderSize;
  uint32_t availableCapacity = GetVcAvailableCapacity (vcId);

  if (availableCapacity >= totalReservationSize)
    {
      // Reserve capacity (including packet size and additional header size)
      m_vcReservedCapacity[vcId] += totalReservationSize;
      NS_LOG_DEBUG ("Reserved " << totalReservationSize << " bytes for VC"
                   << static_cast<uint32_t> (vcId) << " (packet: " << amount
                   << ", headers: " << m_additionalHeaderSize
                   << "), total reserved: " << m_vcReservedCapacity[vcId]);
      return true;
    }

  NS_LOG_DEBUG ("Failed to reserve " << totalReservationSize << " bytes for VC"
               << static_cast<uint32_t> (vcId) << " (available: " << availableCapacity << ")");
  return false;
}

void
SueQueueManager::ReleaseVcCapacity (uint8_t vcId, uint32_t amount)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (vcId) << amount);

  if (vcId >= m_numVcs)
    {
      NS_LOG_WARN ("Invalid VC ID: " << static_cast<uint32_t> (vcId));
      return;
    }

  // Release both packet size and additional header size
  uint32_t totalReleaseSize = amount + m_additionalHeaderSize;

  // Prevent releasing more capacity than reserved
  auto it = m_vcReservedCapacity.find (vcId);
  if (it != m_vcReservedCapacity.end () && it->second >= totalReleaseSize)
    {
      it->second -= totalReleaseSize;
    }
  else
    {
      NS_LOG_WARN ("Attempting to release more capacity than reserved for VC"
                  << static_cast<uint32_t> (vcId) << ", reserved: "
                  << (it != m_vcReservedCapacity.end () ? it->second : 0)
                  << ", attempting to release: " << totalReleaseSize);
      if (it != m_vcReservedCapacity.end ())
        {
          it->second = 0;
        }
    }

  NS_LOG_DEBUG ("Released " << totalReleaseSize << " bytes for VC"
               << static_cast<uint32_t> (vcId) << " (packet: " << amount
               << ", headers: " << m_additionalHeaderSize
               << "), total reserved: " << (it != m_vcReservedCapacity.end () ? it->second : 0));
}

bool
SueQueueManager::EnqueueToVcQueue (Ptr<Packet> packet, uint8_t vcId)
{
  NS_LOG_FUNCTION (this << packet << static_cast<uint32_t> (vcId));

  if (!m_initialized)
    {
      NS_LOG_ERROR ("Queue manager not initialized. Cannot enqueue packet to VC " << static_cast<uint32_t> (vcId));
      return false;
    }

  auto it = m_vcQueues.find (vcId);
  if (it == m_vcQueues.end () || !it->second)
    {
      NS_LOG_ERROR ("VC queue not initialized for VC " << static_cast<uint32_t> (vcId));
      return false;
    }

  return it->second->Enqueue (packet);
}

Ptr<Packet>
SueQueueManager::DequeueFromVcQueue (uint8_t vcId)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (vcId));

  auto it = m_vcQueues.find (vcId);
  if (it == m_vcQueues.end () || !it->second)
    {
      return nullptr;
    }

  return it->second->Dequeue ();
}

bool
SueQueueManager::IsVcQueueEmpty (uint8_t vcId) const
{
  auto it = m_vcQueues.find (vcId);
  if (it == m_vcQueues.end () || !it->second)
    {
      return true;
    }

  return it->second->IsEmpty ();
}

uint32_t
SueQueueManager::GetVcQueueSize (uint8_t vcId) const
{
  auto it = m_vcQueues.find (vcId);
  if (it == m_vcQueues.end () || !it->second)
    {
      return 0;
    }

  return it->second->GetNPackets ();
}

uint32_t
SueQueueManager::GetVcQueueBytes (uint8_t vcId) const
{
  auto it = m_vcQueues.find (vcId);
  if (it == m_vcQueues.end () || !it->second)
    {
      return 0;
    }

  return it->second->GetNBytes ();
}

Ptr<Queue<Packet>>
SueQueueManager::GetVcQueue (uint8_t vcId) const
{
  auto it = m_vcQueues.find (vcId);
  if (it != m_vcQueues.end ())
    {
      return it->second;
    }

  return nullptr;
}

} // namespace ns3