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

#include "load-balancer.h"
#include "sue-client.h"
#include "traffic-generator.h"
#include "performance-logger.h"
#include <ns3/log.h>
#include <ns3/assert.h>
#include <ns3/config.h>
#include <ns3/uinteger.h>
#include <ns3/boolean.h>
#include <ns3/simulator.h>
#include <tuple>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("LoadBalancer");

NS_OBJECT_ENSURE_REGISTERED (LoadBalancer);

TypeId
LoadBalancer::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LoadBalancer")
    .SetParent<Object> ()
    .SetGroupName ("PointToPointSue")
    .AddConstructor<LoadBalancer> ()
    .AddAttribute ("LocalXpuId",
                   "The local XPU identifier",
                   UintegerValue (0),
                   MakeUintegerAccessor (&LoadBalancer::SetLocalXpuId,
                                         &LoadBalancer::GetLocalXpuId),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxXpuId",
                   "The maximum XPU identifier",
                   UintegerValue (3),
                   MakeUintegerAccessor (&LoadBalancer::SetMaxXpuId),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("HashSeed",
                   "The seed value for hash calculation",
                   UintegerValue (12345),
                   MakeUintegerAccessor (&LoadBalancer::SetHashSeed),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("LoadBalanceAlgorithm",
                   "The load balancing algorithm to use",
                   EnumValue (ENHANCED_HASH),
                   MakeUintegerAccessor (&LoadBalancer::SetLoadBalanceAlgorithmUinteger,
                                        &LoadBalancer::GetLoadBalanceAlgorithmUinteger),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Prime1",
                   "First prime number for hash algorithms",
                   UintegerValue (7919),
                   MakeUintegerAccessor (&LoadBalancer::SetPrime1),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Prime2",
                   "Second prime number for enhanced hash",
                   UintegerValue (9973),
                   MakeUintegerAccessor (&LoadBalancer::SetPrime2),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("UseVcInHash",
                   "Whether to include VC ID in hash calculation",
                   BooleanValue (true),
                   MakeBooleanAccessor (&LoadBalancer::SetUseVcInHash,
                                       &LoadBalancer::GetUseVcInHash),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableBitOperations",
                   "Whether to use bit mixing operations in hash",
                   BooleanValue (true),
                   MakeBooleanAccessor (&LoadBalancer::SetEnableBitOperations,
                                       &LoadBalancer::GetEnableBitOperations),
                   MakeBooleanChecker ())
    .AddTraceSource ("BufferQueueChange",
                     "Buffer queue size change",
                     MakeTraceSourceAccessor (&LoadBalancer::m_bufferQueueChangeTrace),
                     "ns3::LoadBalancer::BufferQueueChangeTracedCallback")
  ;
  return tid;
}

LoadBalancer::LoadBalancer ()
  : m_localXpuId (0),
    m_maxXpuId (3),
    m_hashSeed (12345),
    m_algorithm (ENHANCED_HASH),
    m_hashParams (),
    m_trafficGenerationPaused (false)
{
  NS_LOG_FUNCTION (this);
  m_rand = CreateObject<UniformRandomVariable> ();
}

LoadBalancer::~LoadBalancer ()
{
  NS_LOG_FUNCTION (this);
}

void
LoadBalancer::SetLocalXpuId (uint32_t xpuId)
{
  NS_LOG_FUNCTION (this << xpuId);
  m_localXpuId = xpuId;
}

uint32_t
LoadBalancer::GetLocalXpuId () const
{
  NS_LOG_FUNCTION (this);
  return m_localXpuId;
}

void
LoadBalancer::SetMaxXpuId (uint32_t maxXpuId)
{
  NS_LOG_FUNCTION (this << maxXpuId);
  m_maxXpuId = maxXpuId;
}

void
LoadBalancer::AddSueClient (Ptr<SueClient> sueClient, uint32_t sueId)
{
  NS_LOG_FUNCTION (this << sueClient << sueId);
  NS_ASSERT_MSG (sueClient != nullptr, "SueClient pointer cannot be null");

  m_sueClients[sueId] = sueClient;
  NS_LOG_INFO ("Added SUE client with ID " << sueId << " to load balancer");
}

void
LoadBalancer::DistributeTransaction (Ptr<Packet> packet, uint32_t destXpuId, uint8_t vcId)
{
  NS_LOG_FUNCTION (this << packet << destXpuId << vcId);

  // Ensure destination XPU is not local XPU
  if (destXpuId == m_localXpuId)
    {
      NS_LOG_WARN ("Destination XPU " << destXpuId << " is same as local XPU, regenerating");
      destXpuId = GenerateRandomDestinationXpu ();
    }

  // Get packet size
  uint32_t packetSize = packet->GetSize ();

  // Use destination queue space aware SUE selection algorithm
  uint32_t selectedSueId = SelectSueWithDestQueueSpace (destXpuId, vcId, packetSize);

  if (selectedSueId != UINT32_MAX)
    {
      // Found SUE with queue space, distribute transaction
      auto it = m_sueClients.find (selectedSueId);
      if (it != m_sueClients.end ())
        {
          NS_LOG_DEBUG ("Distributing packet to SUE " << selectedSueId << " for destination XPU " << destXpuId);

          // Record LoadBalancer assignment
          PerformanceLogger::GetInstance ().LogLoadBalance (m_localXpuId, destXpuId, vcId, selectedSueId);

          // Call SUE client's AddTransaction method
          it->second->AddTransaction (packet, destXpuId);

          NS_LOG_DEBUG ("Successfully distributed to SUE " << selectedSueId);
        }
      else
        {
          NS_LOG_ERROR ("SUE client " << selectedSueId << " not found in client registry");
        }
    }
  else
    {
      // All SUE destination queues are full, put in buffer queue
      m_bufferQueue.push (std::make_tuple (packet, destXpuId, vcId));

      // If this is the first buffered transaction, pause traffic generator
      if (m_bufferQueue.size () == 1 && !m_trafficGenerationPaused)
        {
          NotifyTrafficGeneratorToPause ();
        }

      NS_LOG_INFO ("All SUE destination queues are full! Transaction buffered. Total buffered: "
                   << m_bufferQueue.size ());

      // // Record buffer status
      // PerformanceLogger::GetInstance ().LogLoadBalance (m_localXpuId, destXpuId, vcId, UINT32_MAX);
    }
}

Ptr<SueClient>
LoadBalancer::GetSueClientForDestination (uint32_t destXpuId, uint8_t vcId)
{
  NS_LOG_FUNCTION (this << destXpuId << vcId);

  uint32_t sueId = CalculateHash (destXpuId, vcId);
  auto it = m_sueClients.find (sueId);

  if (it != m_sueClients.end ())
    {
      return it->second;
    }
  else
    {
      NS_LOG_WARN ("No SUE client found for destination XPU " << destXpuId << " and VC " << vcId);
      return nullptr;
    }
}

void
LoadBalancer::SetHashSeed (uint32_t seed)
{
  NS_LOG_FUNCTION (this << seed);
  m_hashSeed = seed;
}

uint32_t
LoadBalancer::GetSueClientCount () const
{
  NS_LOG_FUNCTION (this);
  return m_sueClients.size ();
}

uint32_t
LoadBalancer::CalculateHash (uint32_t destXpuId, uint8_t vcId) const
{
  NS_LOG_FUNCTION (this << destXpuId << vcId);

  if (m_sueClients.size () == 0)
    {
      NS_LOG_ERROR ("No SUE clients registered for load balancing");
      return 0;
    }

  // Calculate SUE ID based on selected algorithm
  uint32_t sueId = 0;
  switch (m_algorithm)
    {
    case SIMPLE_MOD:
      sueId = destXpuId % m_sueClients.size ();
      break;
    case MOD_WITH_SEED:
      sueId = (destXpuId + m_hashSeed) % m_sueClients.size ();
      break;
    case PRIME_HASH:
      sueId = (destXpuId * m_hashParams.prime1 + m_hashSeed) % m_sueClients.size ();
      break;
    case ENHANCED_HASH:
      {
      sueId = (destXpuId + vcId) % m_sueClients.size ();
      break;
      }
    case ROUND_ROBIN:
      // Round-robin algorithm uses counter for polling distribution
      sueId = (destXpuId + m_hashParams.roundRobinCounter) % m_sueClients.size();
      m_hashParams.roundRobinCounter = (m_hashParams.roundRobinCounter + 1) % m_sueClients.size();
      break;
    case CONSISTENT_HASH:
      sueId = destXpuId % m_sueClients.size (); // Temporarily use simple modulo algorithm
      break;
    default:
      sueId = destXpuId % m_sueClients.size ();
      break;
    }

  NS_LOG_DEBUG ("Load balancing: algorithm=" << m_algorithm
                << ", destXpuId=" << destXpuId
                << ", vcId=" << static_cast<int>(vcId)
                << ", selectedSueId=" << sueId);

  return sueId;
}

uint32_t
LoadBalancer::GenerateRandomDestinationXpu ()
{
  NS_LOG_FUNCTION (this);

  // Generate random destination XPU ID, excluding local XPU
  uint32_t destXpuId;
  do
    {
      destXpuId = m_rand->GetInteger (0, m_maxXpuId);
    }
  while (destXpuId == m_localXpuId);

  NS_LOG_DEBUG ("Generated random destination XPU: " << destXpuId);
  return destXpuId;
}

// Set load balancing algorithm
void
LoadBalancer::SetLoadBalanceAlgorithm (LoadBalanceAlgorithm algorithm)
{
  NS_LOG_FUNCTION (this << algorithm);
  m_algorithm = algorithm;
}

LoadBalanceAlgorithm
LoadBalancer::GetLoadBalanceAlgorithm () const
{
  NS_LOG_FUNCTION (this);
  return m_algorithm;
}

void
LoadBalancer::SetHashAlgorithmParams (const HashAlgorithmParams& params)
{
  NS_LOG_FUNCTION (this);
  m_hashParams = params;
}

const HashAlgorithmParams&
LoadBalancer::GetHashAlgorithmParams () const
{
  NS_LOG_FUNCTION (this);
  return m_hashParams;
}

// Simplified parameter setting method
void
LoadBalancer::SetPrime1 (uint32_t prime)
{
  NS_LOG_FUNCTION (this << prime);
  m_hashParams.prime1 = prime;
}

void
LoadBalancer::SetPrime2 (uint32_t prime)
{
  NS_LOG_FUNCTION (this << prime);
  m_hashParams.prime2 = prime;
}

void
LoadBalancer::SetUseVcInHash (bool useVc)
{
  NS_LOG_FUNCTION (this << useVc);
  m_hashParams.useVcInHash = useVc;
}

bool
LoadBalancer::GetUseVcInHash () const
{
  NS_LOG_FUNCTION (this);
  return m_hashParams.useVcInHash;
}

void
LoadBalancer::SetEnableBitOperations (bool enable)
{
  NS_LOG_FUNCTION (this << enable);
  m_hashParams.enableBitOperations = enable;
}

bool
LoadBalancer::GetEnableBitOperations () const
{
  NS_LOG_FUNCTION (this);
  return m_hashParams.enableBitOperations;
}

    // Check if all SUE clients have completed transmission
bool
LoadBalancer::CheckAllClientsComplete () const
{
  NS_LOG_FUNCTION (this);

  // If no SUE clients, consider as completed
  if (m_sueClients.empty()) {
    return true;
  }

  // Check if all SUE clients have empty queues
  for (const auto& pair : m_sueClients) {
    Ptr<SueClient> client = pair.second;
    if (client && client->HasPendingTransactions()) {
      return false; // Some clients still have pending transactions
    }
  }

  return true; // All clients completed
}

uint64_t
LoadBalancer::GetTotalRemainingBytes () const
{
  NS_LOG_FUNCTION (this);

  uint64_t totalRemaining = 0;

  // Calculate remaining bytes for all SUE clients
  for (const auto& pair : m_sueClients) {
    Ptr<SueClient> client = pair.second;
    if (client) {
      totalRemaining += client->GetPendingTransactionCount();
    }
  }

  return totalRemaining;
}

void
LoadBalancer::StopAllSueLogging () const
{
  NS_LOG_FUNCTION (this);

  // Iterate through all SUE clients and stop their log statistics
  for (const auto& pair : m_sueClients) {
    Ptr<SueClient> client = pair.second;
    if (client) {
      NS_LOG_INFO ("Stopping logging for SUE client " << pair.first);
      client->SetLoggingEnabled (false);
    }
  }

  // Also stop statistics logs for all network devices on this XPU
  // Set StatLoggingEnabled to false for all PointToPointSueNetDevice through configuration path
  Config::Set ("/NodeList/*/DeviceList/*/$ns3::PointToPointSueNetDevice/StatLoggingEnabled", BooleanValue (false));

  NS_LOG_INFO ("All logging events stopped for XPU " << m_localXpuId);
  std::cout << "All logging events stopped for XPU " << m_localXpuId << std::endl;
}

// Queue Space-Aware SUE Selection Implementation

uint32_t
LoadBalancer::SelectSueWithDestQueueSpace (uint32_t destXpuId, uint8_t vcId, uint32_t packetSize)
{
  NS_LOG_FUNCTION (this << destXpuId << static_cast<uint32_t> (vcId) << packetSize);

  // First, try the original hash-based SUE
  uint32_t targetSueId = CalculateHash (destXpuId, vcId);

  if (CheckSueDestQueueSpace (targetSueId, destXpuId, vcId, packetSize)) {
    NS_LOG_DEBUG ("Target SUE " << targetSueId << " has available destination queue space");
    return targetSueId;
  }

  NS_LOG_DEBUG ("Target SUE " << targetSueId << " destination queue is full, trying alternatives");
  // Try to find another SUE with available destination queue space
  return TryNextAvailableSueWithSpace (targetSueId, destXpuId, vcId, packetSize);
}

uint32_t
LoadBalancer::TryNextAvailableSueWithSpace (uint32_t startSueId, uint32_t destXpuId, uint8_t vcId, uint32_t packetSize)
{
  NS_LOG_FUNCTION (this << startSueId << destXpuId << static_cast<uint32_t> (vcId) << packetSize);

  // Try all SUEs in a round-robin fashion starting from startSueId
  for (auto const& clientPair : m_sueClients) {
    uint32_t sueId = clientPair.first;
    if (CheckSueDestQueueSpace (sueId, destXpuId, vcId, packetSize)) {
      NS_LOG_DEBUG ("Found available SUE " << sueId << " as alternative");
      return sueId;
    }
  }

  NS_LOG_DEBUG ("No SUE with available destination queue space found");
  return UINT32_MAX;  // No SUE has available destination queue space
}

void
LoadBalancer::NotifyTrafficGeneratorToPause ()
{
  NS_LOG_FUNCTION (this);

  if (!m_trafficGenerationPaused) {
    m_trafficGenerationPaused = true;
    NS_LOG_WARN ("Traffic generation paused due to credit exhaustion");

    // Actually pause the TrafficGenerator
    if (m_trafficGenerator) {
      m_trafficGenerator->PauseGeneration ();
      NS_LOG_DEBUG ("TrafficGenerator pause notification sent");
    } else {
      NS_LOG_WARN ("TrafficGenerator not set, cannot pause generation");
    }
  }
}

void
LoadBalancer::NotifyTrafficGeneratorToResume ()
{
  NS_LOG_FUNCTION (this);

  if (m_trafficGenerationPaused) {
    m_trafficGenerationPaused = false;
    NS_LOG_INFO ("Traffic generation resumed - credits available");

    // Actually resume the TrafficGenerator
    if (m_trafficGenerator) {
      m_trafficGenerator->ResumeGeneration ();
      NS_LOG_DEBUG ("TrafficGenerator resume notification sent");
    } else {
      NS_LOG_WARN ("TrafficGenerator not set, cannot resume generation");
    }
  }
}

void
LoadBalancer::ProcessBufferedTransactions ()
{
  NS_LOG_FUNCTION (this);

  if (m_bufferQueue.empty ()) {
    return;
  }

  NS_LOG_DEBUG ("Processing " << m_bufferQueue.size () << " buffered transactions");

  // Process buffered transactions while we have available destination queue space
  while (!m_bufferQueue.empty ()) {
    Ptr<Packet> packet = std::get<0>(m_bufferQueue.front ());
    uint32_t destXpuId = std::get<1>(m_bufferQueue.front ());
    uint8_t vcId = std::get<2>(m_bufferQueue.front ());
    uint32_t packetSize = packet->GetSize ();
    uint32_t availableSueId = SelectSueWithDestQueueSpace (destXpuId, vcId, packetSize);

    if (availableSueId != UINT32_MAX) {
      // Found a SUE with available destination queue space
      m_bufferQueue.pop ();

      // Trigger trace for buffer queue size decrease
      m_bufferQueueChangeTrace (m_bufferQueue.size (), m_localXpuId+1);

      auto it = m_sueClients.find (availableSueId);
      if (it != m_sueClients.end ()) {
        it->second->AddTransaction (packet, destXpuId);

        NS_LOG_DEBUG ("Processed buffered transaction to SUE " << availableSueId
                     << " for destination XPU " << destXpuId);
      }
    } else {
      // No SUE has available destination queue space, stop processing
      NS_LOG_DEBUG ("No available destination queue space, stopping buffer processing");
      break;
    }
  }

  // If buffer is cleared and traffic generation is paused, resume it
  if (m_bufferQueue.empty () && m_trafficGenerationPaused) {
    NotifyTrafficGeneratorToResume ();
  }
}

// Credit Management Implementation







uint32_t
LoadBalancer::GetBufferedTransactionCount () const
{
  return m_bufferQueue.size ();
}

bool
LoadBalancer::IsTrafficGenerationPaused () const
{
  return m_trafficGenerationPaused;
}

uint32_t
LoadBalancer::CalculateDestQueueCapacity (uint32_t sueId)
{
  NS_LOG_FUNCTION (this << sueId);

  auto it = m_sueClients.find (sueId);
  if (it == m_sueClients.end ()) {
    NS_LOG_WARN ("SUE " << sueId << " not found, using default queue capacity");
    return 30 * 1024;  // Default queue capacity (30KB)
  }

  Ptr<SueClient> sueClient = it->second;

  // Directly return destination queue maximum capacity of SUE client
  return sueClient->GetDestQueueMaxBytes ();
}

void
LoadBalancer::SetTrafficGenerator (Ptr<TrafficGenerator> trafficGenerator)
{
  NS_LOG_FUNCTION (this << trafficGenerator);
  m_trafficGenerator = trafficGenerator;
}

// Queue Management Implementation

bool
LoadBalancer::CheckSueDestQueueSpace (uint32_t sueId, uint32_t destXpuId, uint8_t vcId, uint32_t packetSize) const
{
  NS_LOG_FUNCTION (this << sueId << destXpuId << static_cast<uint32_t> (vcId) << packetSize);

  auto it = m_sueClients.find (sueId);
  if (it == m_sueClients.end ())
    {
      NS_LOG_WARN ("SUE client " << sueId << " not found");
      return false;
    }

  uint32_t availableSpace = GetSueDestQueueAvailableSpace (sueId, destXpuId, vcId);
  bool hasSpace = availableSpace >= packetSize;

  NS_LOG_DEBUG ("SUE " << sueId << " destination queue for XPU " << destXpuId 
               << "-VC" << static_cast<uint32_t> (vcId) << " has " << availableSpace 
               << " bytes available, need " << packetSize << " bytes: " << (hasSpace ? "OK" : "FULL"));

  return hasSpace;
}

uint32_t
LoadBalancer::GetSueDestQueueAvailableSpace (uint32_t sueId, uint32_t destXpuId, uint8_t vcId) const
{
  NS_LOG_FUNCTION (this << sueId << destXpuId << static_cast<uint32_t> (vcId));

  auto it = m_sueClients.find (sueId);
  if (it == m_sueClients.end ())
    {
      NS_LOG_WARN ("SUE client " << sueId << " not found");
      return 0;
    }

  // Use SueClient's public interface to get destination queue information
  uint32_t maxQueueSize = it->second->GetDestQueueMaxBytes ();
  uint32_t currentUsage = it->second->GetDestQueueCurrentUsage (destXpuId, vcId);
  
  return maxQueueSize - currentUsage;
}

void
LoadBalancer::NotifyDestQueueSpaceAvailable (uint32_t sueId, uint32_t destXpuId, uint8_t vcId)
{
  NS_LOG_FUNCTION (this << sueId << destXpuId << static_cast<uint32_t> (vcId));

  NS_LOG_DEBUG ("Destination queue space available notification for SUE " << sueId 
               << ", destination XPU " << destXpuId << "-VC" << static_cast<uint32_t> (vcId));

  // Process transactions in buffer queue
  ProcessBufferedTransactions ();
}

void
LoadBalancer::SetDestQueueSpaceCallback (Callback<void, uint32_t, uint32_t, uint8_t> callback)
{
  NS_LOG_FUNCTION (this);
  m_destQueueSpaceCallback = callback;
}

void
LoadBalancer::SetLoadBalanceAlgorithmUinteger (uint32_t algorithm)
{
  NS_LOG_FUNCTION (this << algorithm);
  SetLoadBalanceAlgorithm (static_cast<LoadBalanceAlgorithm> (algorithm));
}

uint32_t
LoadBalancer::GetLoadBalanceAlgorithmUinteger () const
{
  NS_LOG_FUNCTION (this);
  return static_cast<uint32_t> (GetLoadBalanceAlgorithm ());
}




} // namespace ns3