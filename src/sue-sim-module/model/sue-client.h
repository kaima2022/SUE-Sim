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

#ifndef SUE_CLIENT_H
#define SUE_CLIENT_H

#include <list>
#include <map>
#include <queue>
#include <vector>
#include <functional>
#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/ipv4-address.h"
#include "ns3/mac48-address.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/random-variable-stream.h"
#include "ns3/data-rate.h"
#include "ns3/inet-socket-address.h"
#include "ns3/net-device.h"
#include "point-to-point-sue-net-device.h"

namespace ns3 {

/**
 * \defgroup sue-client SUE Client Application
 * This section documents the API of the ns-3 sue-client module.
 * For a functional description, please refer to the ns-3 manual.
 */

/**
 * \ingroup sue-client
 * \class SueClient
 * \brief A client application for SUE protocol with multi-port management and intelligent packing.
 *
 * The SueClient class implements a client application that can manage multiple
 * network devices, perform intelligent packet packing by virtual channel,
 * and handle transaction-based communication with load balancing support.
 * It provides comprehensive statistics collection and monitoring capabilities.
 */

/**
 * \brief Structure representing a destination with XPU ID and VC ID
 */
struct Destination
{
  uint32_t destXpuId; //!< Destination XPU identifier
  uint8_t vcId;       //!< Virtual channel identifier

  /**
   * \brief Default constructor
   */
  Destination () : destXpuId (0), vcId (0) {}

  /**
   * \brief Constructor with XPU ID
   * \param xpuId The XPU identifier
   */
  Destination (uint32_t xpuId) : destXpuId (xpuId), vcId (0) {}

  /**
   * \brief Constructor with XPU ID and VC ID
   * \param xpuId The XPU identifier
   * \param vc The virtual channel identifier
   */
  Destination (uint32_t xpuId, uint8_t vc) : destXpuId (xpuId), vcId (vc) {}

  /**
   * \brief Less than operator for map ordering
   * \param other Other destination
   * \return true if this destination is less than other
   */
  bool operator<(const Destination& other) const
  {
    if (destXpuId != other.destXpuId) {
      return destXpuId < other.destXpuId;
    }
    return vcId < other.vcId;
  }
};

/**
 * \brief Structure for queue state with timestamp
 */
struct QueueState
{
  std::queue<std::pair<Time, Ptr<Packet>>> queue; //!< Queue of <enqueue time, packet> pairs
  uint32_t currentBurstSize = 0;                  //!< Current burst size in bytes
};

class SueClient : public Application
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Construct a SueClient
   */
  SueClient ();

  /**
   * \brief Destroy a SueClient
   */
  virtual ~SueClient ();

  /**
   * \brief Add a transaction to be sent to destination XPU
   *
   * This interface is called by LoadBalancer to add transactions
   *
   * \param transaction Packet containing the transaction
   * \param destXpuId Destination XPU identifier
   */
  void AddTransaction (Ptr<Packet> transaction, uint32_t destXpuId);

  /**
   * \brief Set XPU information
   *
   * \param xpuId XPU identifier
   * \param deviceId Device identifier
   */
  void SetXpuInfo (uint32_t xpuId, uint32_t deviceId);

  /**
   * \brief Set the managed network devices for this SUE client
   *
   * \param managedDevices Vector of managed network devices
   */
  void SetManagedDevices (const std::vector<Ptr<PointToPointSueNetDevice>>& managedDevices);

  /**
   * \brief Set the SUE identifier
   *
   * \param sueId SUE identifier
   */
  void SetSueId (uint32_t sueId);

  /**
   * \brief Log client statistics
   */
  void LogClientStatistics ();

  /**
   * \brief Enable or disable logging
   *
   * \param enabled True to enable logging, false to disable
   */
  void SetLoggingEnabled (bool enabled);

  /**
   * \brief Set the global IP to MAC address mapping
   *
   * \param map Map of IP addresses to MAC addresses
   */
  static void SetGlobalIpMacMap (const std::map<Ipv4Address, Mac48Address>& map);

  /**
   * \brief Check if there are pending transactions
   *
   * \return true if there are pending transactions
   */
  bool HasPendingTransactions () const;

  /**
   * \brief Get the number of pending transactions
   *
   * \return Number of pending transactions
   */
  uint32_t GetPendingTransactionCount () const;

  /**
   * \brief Log destination queue usage statistics
   */
  void LogDestinationQueueUsage ();

  /**
   * \brief Set destination queue space available callback
   *
   * This callback is called when the SUE destination queue has space available
   * and buffered transactions can be processed.
   *
   * \param callback The callback function to call (sueId, destXpuId, vcId)
   */
  template<typename Callback>
  void SetDestQueueSpaceCallback (Callback callback)
  {
    m_destQueueSpaceCallback = callback;
  }

  /**
   * \brief Get the number of managed devices
   *
   * \return Number of managed devices
   */
  uint32_t GetManagedDeviceCount () const;

  /**
   * \brief Get the destination queue maximum bytes
   *
   * \return Destination queue maximum capacity
   */
  uint32_t GetDestQueueMaxBytes () const;

  /**
   * \brief Get the current usage of destination queue for specific destination
   *
   * \param destXpuId Destination XPU identifier
   * \param vcId Virtual channel identifier
   * \return Current usage in bytes
   */
  uint32_t GetDestQueueCurrentUsage (uint32_t destXpuId, uint8_t vcId) const;

  /**
   * \brief Cancel all logging events
   */
  void CancelAllLogEvents ();



protected:
  /**
   * \brief Dispose of the object
   */
  void DoDispose (void) override;

private:
  /**
   * \brief Copy constructor (disabled)
   *
   * \param other Other SueClient instance
   */
  SueClient (const SueClient &other);

  /**
   * \brief Assignment operator (disabled)
   *
   * \param other Other SueClient instance
   * \return Reference to this instance
   */
  SueClient& operator= (const SueClient &other);

  /**
   * \brief Application specific startup method
   */
  void StartApplication (void) override;

  /**
   * \brief Application specific shutdown method
   */
  void StopApplication (void) override;

  /**
   * \brief Handle received packets
   *
   * \param socket Socket on which data was received
   */
  void HandleRead (Ptr<Socket> socket);

  /**
   * \brief Schedule the next send operation
   */
  void ScheduleNextSend ();

  /**
   * \brief Send a burst of packets
   *
   * \param burstPacket Packet to send
   * \param sendingSocket Socket to use for sending
   * \param remoteAddr Remote address
   * \param dest Destination information
   * \param device Device used for sending (for capacity management)
   */
  void DoSendBurst (Ptr<Packet> burstPacket, Ptr<Socket> sendingSocket,
                    InetSocketAddress remoteAddr, const Destination& dest,
                    Ptr<PointToPointSueNetDevice> device);

  /**
   * \brief Pack transactions by VC ID intelligently
   *
   * \param dest Destination information
   * \return Vector of packed packets
   */
  std::vector<Ptr<Packet>> Packing (const Destination& dest);

  /**
   * \brief Create a combined packet from multiple payloads
   *
   * \param payloads Vector of payload packets
   * \param vcId Virtual channel identifier
   * \param targetXpuId Target XPU identifier
   * \return Combined packet
   */
  Ptr<Packet> CreateCombinedPacket (const std::vector<Ptr<Packet>>& payloads,
                                   uint8_t vcId, uint32_t targetXpuId);

  /**
   * \brief Select device by round-robin algorithm
   *
   * \return Selected network device
   */
  Ptr<PointToPointSueNetDevice> SelectDeviceByRoundRobin ();

  /**
   * \brief Validate socket state
   *
   * \param socket Socket to validate
   * \param context Context description for logging
   * \return true if socket state is valid
   */
  bool ValidateSocketState (Ptr<Socket> socket, const std::string& context);

  /**
   * \brief Select device based on VC queue capacity
   *
   * This function selects a device with sufficient VC queue capacity
   * for the given packet size and VC ID. It checks all managed devices
   * in round-robin order and returns the first one with enough capacity.
   *
   * \param packetSize Size of the packet to send
   * \param vcId Virtual Channel ID
   * \return Selected device or nullptr if no device has sufficient capacity
   */
  Ptr<PointToPointSueNetDevice> SelectDeviceByVcCapacity (uint32_t packetSize, uint8_t vcId);

  /**
   * \brief Remove transactions from destination queue after successful send
   *
   * This function removes the specified number of transactions from the
   * destination queue after a successful send operation.
   *
   * \param dest Destination queue identifier
   * \param count Number of transactions to remove
   */
  void PopTransactionsFromQueue (const Destination& dest, uint32_t count);

  /**
   * \brief Log network state information
   *
   * \param dest Destination information
   */
  void LogNetworkState (const Destination& dest);

  /**
   * \brief Get MAC address for IP address
   *
   * \param ip IP address
   * \return Corresponding MAC address
   */
  Mac48Address GetMacForIp (Ipv4Address ip);

  /**
   * \brief Analyze packet to count the number of transactions it contains
   *
   * This function extracts and counts the number of individual transactions
   * that were packed into a single packet for accurate credit restoration.
   * The transaction count is directly used as the credit restoration amount.
   *
   * \param packet The packet to analyze
   * \return Number of transactions contained in the packet (also credit amount)
   */
  uint32_t AnalyzeTransactionCount (Ptr<Packet> packet) const;

  /**
   * \brief Process accumulated credits for batch restoration
   *
   * This method is called when the accumulated credits reach the batch size
   * threshold to restore credits with HPC-appropriate delay.
   */
  void ProcessAccumulatedCredits ();

  

  // Transaction management
  std::map<Destination, QueueState> m_destQueues;                         //!< Destination queues
  std::map<Destination, QueueState>::iterator m_currentQueueIt;          //!< Current queue iterator
  uint32_t m_maxBurstSize;                                                //!< Maximum burst size in bytes
  uint16_t m_psn;                                                          //!< Packet sequence number
  uint32_t m_packetsSent;                                                 //!< Packets sent counter
  uint32_t m_acksReceived;                                                //!< ACKs received counter
  uint32_t m_nodeId;                                                      //!< Node identifier
  uint32_t m_xpuId;                                                       //!< XPU identifier
  uint32_t m_lastDeviceIndex;                                             //!< Last device index
  Time m_waitingStartTime;                                                //!< Waiting start time
  uint32_t m_vcNum;                                                       //!< Number of virtual channels
  uint32_t m_destQueueMaxBytes;                                           //!< Destination queue maximum capacity
  uint64_t m_totalBytesSent;                                              //!< Total bytes sent
  Time m_lastStatTime;                                                    //!< Last statistics time
  Time m_clientStatInterval;                                              //!< Client statistics interval
  std::string m_clientStatIntervalString;                               //!< Client statistics interval string for compatibility
  Time m_packingDelayPerPacket;                                           //!< Packing delay per packet
  uint32_t m_XpuDropCounts;                                               //!< XPU drop counter
  bool m_loggingEnabled;                                                  //!< Logging enabled flag
  uint32_t m_deviceId;                                                    //!< Device identifier (base-1)
  uint32_t m_sueId;                                                       //!< SUE identifier
  uint32_t m_portsPerSue;                                                 //!< Number of ports per SUE
  uint32_t m_lastUsedDeviceIndex;                                         //!< Last used device index for round-robin

  // Network device management
  std::map<Ptr<NetDevice>, Ptr<Socket>> m_deviceSockets;                //!< Device to socket mapping
  std::vector<Ptr<PointToPointSueNetDevice>> m_managedDevices;           //!< Managed devices list
  std::vector<Ptr<PointToPointSueNetDevice>> m_p2pDevices;               //!< P2P devices list

  // Statistics and monitoring
  std::vector<int64_t> m_packDelays;                                      //!< Packing delays (nanoseconds)
  std::vector<uint32_t> m_packNumbers;                                    //!< Packing numbers

  // Event management
  EventId m_schedulerEvent;                                               //!< Scheduler event ID
  EventId m_logClientStatisticsEvent;                                     //!< Log statistics event ID

  // Configuration parameters
  Time m_schedulingInterval;                                              //!< Scheduling interval
  uint32_t m_transactionSize;                                             //!< Transaction size in bytes

  // Randomness and timing
  Ptr<UniformRandomVariable> m_rand;                                      //!< Random variable generator

  // Queue Management
  std::function<void(uint32_t, uint32_t, uint8_t)> m_destQueueSpaceCallback; //!< Destination queue space callback (sueId, destXpuId, vcId)

  // Global mappings
  static std::map<Ipv4Address, Mac48Address> s_ipToMacMap;               //!< Global IP to MAC mapping
};

} // namespace ns3

#endif /* SUE_CLIENT_H */