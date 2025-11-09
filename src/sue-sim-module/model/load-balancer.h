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

#ifndef LOAD_BALANCER_H
#define LOAD_BALANCER_H

#include <ns3/object.h>
#include <ns3/ptr.h>
#include <ns3/packet.h>
#include <ns3/random-variable-stream.h>
#include <ns3/ipv4-address.h>
#include <ns3/enum.h>
#include <ns3/event-id.h>
#include <ns3/nstime.h>
#include <ns3/traced-callback.h>
#include <map>
#include <vector>
#include <queue>

namespace ns3 {

/**
 * \brief The SueClient class forward declaration
 */
class SueClient;

/**
 * \brief The TrafficGenerator class forward declaration
 */
class TrafficGenerator;

/**
 * \brief The PerformanceLogger class forward declaration
 */
class PerformanceLogger;



/**
 * \brief LoadBalancer BufferQueueChange trace signature
 *
 * Traces changes in buffer queue size
 * \param bufferSize Current buffer queue size
 * \param xpuId XPU identifier
 */
typedef void (*BufferQueueChangeTracedCallback)(uint32_t bufferSize, uint32_t xpuId);

/**
 * \brief Load balancing algorithm types
 */
enum LoadBalanceAlgorithm
{
  SIMPLE_MOD = 0,      ///< Simple modulo algorithm: sueId = destXpuId % sueCount
  MOD_WITH_SEED = 1,   ///< Modulo with seed: sueId = (destXpuId + seed) % sueCount
  PRIME_HASH = 2,      ///< Prime number hash: sueId = (destXpuId * prime + seed) % sueCount
  ENHANCED_HASH = 3,   ///< Enhanced hash with VC and bit operations
  ROUND_ROBIN = 4,     ///< Round-robin algorithm (not implemented yet)
  CONSISTENT_HASH = 5  ///< Consistent hashing (not implemented yet)
};

/**
 * \brief Hash algorithm parameters
 */
struct HashAlgorithmParams
{
  uint32_t prime1;           ///< First prime number for hash algorithms
  uint32_t prime2;           ///< Second prime number for enhanced hash
  bool useVcInHash;          ///< Whether to include VC ID in hash calculation
  bool enableBitOperations;  ///< Whether to use bit mixing operations
  uint32_t roundRobinCounter; ///< Counter for round-robin algorithm

  HashAlgorithmParams ()
    : prime1 (7919), prime2 (9973), useVcInHash (true),
      enableBitOperations (true), roundRobinCounter (0) {}
};

/**
 * \ingroup point-to-point-sue
 * \brief Load Balancer for distributing traffic to SUE clients
 *
 * This class implements load balancing functionality to distribute
 * traffic from TrafficGenerator to multiple SUE clients based on
 * hash algorithm using destination XPU and VC ID. It also manages
 * credit-based flow control to prevent buffer overflow and ensure
 * efficient traffic distribution.
 */
class LoadBalancer : public Object
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Constructor
   */
  LoadBalancer ();

  /**
   * \brief Destructor
   */
  virtual ~LoadBalancer ();

  /**
   * \brief Set the local XPU ID
   * \param xpuId the local XPU identifier
   */
  void SetLocalXpuId (uint32_t xpuId);

  /**
   * \brief Get the local XPU ID
   * \return the local XPU identifier
   */
  uint32_t GetLocalXpuId () const;

  /**
   * \brief Set the maximum XPU ID
   * \param maxXpuId the maximum XPU identifier
   */
  void SetMaxXpuId (uint32_t maxXpuId);

  /**
   * \brief Add a SUE client to the load balancer
   * \param sueClient pointer to the SUE client
   * \param sueId the SUE identifier
   */
  void AddSueClient (Ptr<SueClient> sueClient, uint32_t sueId);

  /**
   * \brief Distribute transaction to appropriate SUE client
   * \param packet the packet to distribute
   * \param destXpuId the destination XPU ID
   * \param vcId the virtual channel ID (still used for hash calculation)
   */
  void DistributeTransaction (Ptr<Packet> packet, uint32_t destXpuId, uint8_t vcId);

  /**
   * \brief Get the SUE client for a specific destination and VC
   * \param destXpuId the destination XPU ID
   * \param vcId the virtual channel ID
   * \return pointer to the selected SUE client
   */
  Ptr<SueClient> GetSueClientForDestination (uint32_t destXpuId, uint8_t vcId);

  /**
   * \brief Set the hash seed for load balancing
   * \param seed the hash seed value
   */
  void SetHashSeed (uint32_t seed);

  /**
   * \brief Set the load balancing algorithm
   * \param algorithm the algorithm type to use
   */
  void SetLoadBalanceAlgorithm (LoadBalanceAlgorithm algorithm);

  /**
   * \brief Get the current load balancing algorithm
   * \return the algorithm type
   */
  LoadBalanceAlgorithm GetLoadBalanceAlgorithm () const;

  /**
   * \brief Set load balancing algorithm using uinteger value
   * \param algorithm the algorithm type as uint32_t
   */
  void SetLoadBalanceAlgorithmUinteger (uint32_t algorithm);

  /**
   * \brief Get load balancing algorithm as uinteger value
   * \return the algorithm type as uint32_t
   */
  uint32_t GetLoadBalanceAlgorithmUinteger () const;

  /**
   * \brief Set hash algorithm parameters
   * \param params the hash algorithm parameters
   */
  void SetHashAlgorithmParams (const HashAlgorithmParams& params);

  /**
   * \brief Get hash algorithm parameters
   * \return the hash algorithm parameters
   */
  const HashAlgorithmParams& GetHashAlgorithmParams () const;

  /**
   * \brief Get the number of registered SUE clients
   * \return the number of SUE clients
   */
  uint32_t GetSueClientCount () const;

  /**
   * \brief Set the first prime number for hash algorithms
   * \param prime the prime number value
   */
  void SetPrime1 (uint32_t prime);

  /**
   * \brief Set the second prime number for enhanced hash
   * \param prime the prime number value
   */
  void SetPrime2 (uint32_t prime);

  /**
   * \brief Set whether to include VC ID in hash calculation
   * \param useVc whether to use VC ID in hash
   */
  void SetUseVcInHash (bool useVc);

  /**
   * \brief Get whether VC ID is included in hash calculation
   * \return true if VC ID is used in hash
   */
  bool GetUseVcInHash () const;

  /**
   * \brief Set whether to enable bit mixing operations
   * \param enable whether to enable bit operations
   */
  void SetEnableBitOperations (bool enable);

  /**
   * \brief Get whether bit mixing operations are enabled
   * \return true if bit operations are enabled
   */
  bool GetEnableBitOperations () const;

  /**
   * \brief Check if all SUE clients have completed transmission
   * \return true if all clients are complete
   */
  bool CheckAllClientsComplete () const;

  /**
   * \brief Get total remaining bytes across all SUE clients
   * \return total remaining bytes
   */
  uint64_t GetTotalRemainingBytes () const;

  /**
   * \brief Stop all logging events for SUE clients on this XPU
   */
  void StopAllLogging () const;

  /**
   * \brief Disable logging only for SUE clients on this XPU without canceling events
   */
  void DisableSueLoggingOnly () const;

  // Queue Management Interface

  /**
   * \brief Check if a SUE client has available space in destination queue
   * \param sueId the SUE identifier
   * \param destXpuId the destination XPU ID
   * \param vcId the virtual channel ID
   * \param packetSize the packet size to check
   * \return true if the SUE has available space in destination queue
   */
  bool CheckSueDestQueueSpace (uint32_t sueId, uint32_t destXpuId, uint8_t vcId, uint32_t packetSize) const;

  /**
   * \brief Get available space in destination queue for a SUE
   * \param sueId the SUE identifier
   * \param destXpuId the destination XPU ID
   * \param vcId the virtual channel ID
   * \return available space in bytes
   */
  uint32_t GetSueDestQueueAvailableSpace (uint32_t sueId, uint32_t destXpuId, uint8_t vcId) const;

  /**
   * \brief Notify LoadBalancer that destination queue space is available
   * \param sueId the SUE identifier
   * \param destXpuId the destination XPU ID
   * \param vcId the virtual channel ID
   */
  void NotifyDestQueueSpaceAvailable (uint32_t sueId, uint32_t destXpuId, uint8_t vcId);

  /**
   * \brief Process buffered transactions when credits become available
   */
  void ProcessBufferedTransactions ();

  /**
   * \brief Get the number of currently buffered transactions
   * \return the buffer queue size
   */
  uint32_t GetBufferedTransactionCount () const;

  /**
   * \brief Check if traffic generation is currently paused
   * \return true if traffic generation is paused
   */
  bool IsTrafficGenerationPaused () const;

  /**
   * \brief Set traffic generator for flow control
   *
   * This method allows the LoadBalancer to control the TrafficGenerator
   * when destination queues are full or become available.
   *
   * \param trafficGenerator Pointer to the traffic generator
   */
  void SetTrafficGenerator (Ptr<TrafficGenerator> trafficGenerator);

  /**
   * \brief Set callback for destination queue space available notification
   * \param callback The callback function
   */
  void SetDestQueueSpaceCallback (Callback<void, uint32_t, uint32_t, uint8_t> callback);

  
  private:
  // Trace Callbacks
  TracedCallback<uint32_t, uint32_t> m_bufferQueueChangeTrace;                 ///< Buffer queue change trace (bufferSize, xpuId)
  
  /**
   * \brief Calculate hash value for load balancing using selected algorithm
   * \param destXpuId the destination XPU ID
   * \param vcId the virtual channel ID
   * \return the hash value
   */
  uint32_t CalculateHash (uint32_t destXpuId, uint8_t vcId) const;

  /**
   * \brief Simple modulo algorithm: sueId = destXpuId % sueCount
   * \param destXpuId the destination XPU ID
   * \param vcId the virtual channel ID (ignored)
   * \return the SUE ID
   */
  uint32_t SimpleModAlgorithm (uint32_t destXpuId, uint8_t vcId) const;

  /**
   * \brief Modulo with seed algorithm: sueId = (destXpuId + seed) % sueCount
   * \param destXpuId the destination XPU ID
   * \param vcId the virtual channel ID (ignored)
   * \return the SUE ID
   */
  uint32_t ModWithSeedAlgorithm (uint32_t destXpuId, uint8_t vcId) const;

  /**
   * \brief Prime number hash algorithm
   * \param destXpuId the destination XPU ID
   * \param vcId the virtual channel ID (ignored)
   * \return the SUE ID
   */
  uint32_t PrimeHashAlgorithm (uint32_t destXpuId, uint8_t vcId) const;

  /**
   * \brief Enhanced hash algorithm with VC and bit operations
   * \param destXpuId the destination XPU ID
   * \param vcId the virtual channel ID
   * \return the SUE ID
   */
  uint32_t EnhancedHashAlgorithm (uint32_t destXpuId, uint8_t vcId) const;

  /**
   * \brief Round-robin algorithm
   * \param destXpuId the destination XPU ID (ignored)
   * \param vcId the virtual channel ID (ignored)
   * \return the SUE ID
   */
  uint32_t RoundRobinAlgorithm (uint32_t destXpuId, uint8_t vcId);

  /**
   * \brief Generate random destination XPU ID (excluding local)
   * \return random destination XPU ID
   */
  uint32_t GenerateRandomDestinationXpu ();

  // Queue Management Private Methods

  /**
   * \brief Select a SUE client with available destination queue space
   * \param destXpuId the destination XPU ID
   * \param vcId the virtual channel ID
   * \param packetSize the packet size to send
   * \return SUE ID with available space, or UINT32_MAX if none available
   */
  uint32_t SelectSueWithDestQueueSpace (uint32_t destXpuId, uint8_t vcId, uint32_t packetSize);

  /**
   * \brief Try to find the next available SUE with destination queue space
   * \param startSueId the SUE ID to start searching from
   * \param destXpuId the destination XPU ID
   * \param vcId the virtual channel ID
   * \param packetSize the packet size to send
   * \return next available SUE ID, or UINT32_MAX if none available
   */
  uint32_t TryNextAvailableSueWithSpace (uint32_t startSueId, uint32_t destXpuId, uint8_t vcId, uint32_t packetSize);

  /**
   * \brief Notify TrafficGenerator to pause generation
   */
  void NotifyTrafficGeneratorToPause ();

  /**
   * \brief Notify TrafficGenerator to resume generation
   */
  void NotifyTrafficGeneratorToResume ();

  /**
   * \brief Calculate max destination queue capacity for a SUE based on its characteristics
   * \param sueId the SUE identifier
   * \return calculated maximum queue capacity value
   */
  uint32_t CalculateDestQueueCapacity (uint32_t sueId);

private:
  uint32_t m_localXpuId;                     ///< Local XPU identifier
  uint32_t m_maxXpuId;                       ///< Maximum XPU identifier
  std::map<uint32_t, Ptr<SueClient>> m_sueClients; ///< Map of SUE ID to SUE client
  uint32_t m_hashSeed;                       ///< Hash seed for load balancing
  Ptr<UniformRandomVariable> m_rand;         ///< Random variable generator
  LoadBalanceAlgorithm m_algorithm;         ///< Current load balancing algorithm
  mutable HashAlgorithmParams m_hashParams;   ///< Hash algorithm parameters (mutable for round-robin)

  // Queue Management Members
  std::queue<std::tuple<Ptr<Packet>, uint32_t, uint8_t>> m_bufferQueue; ///< Buffered transactions (packet, destXpuId, vcId)
  bool m_trafficGenerationPaused;                   ///< Flag indicating if traffic generation is paused
  Callback<void, uint32_t, uint32_t, uint8_t> m_destQueueSpaceCallback; ///< Callback for destination queue space availability

  // Flow Control Members
  Ptr<TrafficGenerator> m_trafficGenerator;         ///< TrafficGenerator for flow control
  bool m_enableAlternativePath;                     ///< Whether to search for alternative SUE path
};

} // namespace ns3

#endif /* LOAD_BALANCER_H */