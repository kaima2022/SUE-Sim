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

#ifndef PERFORMANCE_LOGGER_H
#define PERFORMANCE_LOGGER_H

#include <map>
#include <fstream>
#include <string>
#include "ns3/nstime.h"

namespace ns3 {

/**
 * \defgroup performance-logger Performance Logger
 * This section documents the API of the ns-3 performance-logger module.
 * For a functional description, please refer to the ns-3 manual.
 */

/**
 * \ingroup performance-logger
 * \class PerformanceLogger
 * \brief Singleton class for logging performance metrics and statistics.
 *
 * The PerformanceLogger class provides a centralized logging system for
 * various performance metrics including device statistics, application statistics,
 * drop statistics, credit statistics, packing statistics, load balancing statistics,
 * and queue usage statistics. It manages multiple log files and provides
 * thread-safe logging operations.
 */
class PerformanceLogger
{
public:
  /**
   * \brief Get the singleton instance
   *
   * \return Reference to the PerformanceLogger singleton instance
   */
  static PerformanceLogger& GetInstance ();

  /**
   * \brief Initialize the logger with a base filename
   *
   * \param filename Base filename for log files
   */
  void Initialize (const std::string& filename);

  
  // === EVENT-DRIVEN STATISTICS FUNCTIONS ===

  /**
   * \brief Log single packet transmission (event-driven)
   * Records individual packet transmission events immediately
   *
   * \param nanoTime Time in nanoseconds
   * \param XpuId XPU identifier
   * \param devId Device identifier
   * \param vcId Virtual channel identifier
   * \param direction Direction ("Tx")
   * \param packetSizeBits Packet size in bits
   */
  void LogPacketTx (int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                    const std::string& direction, uint32_t packetSizeBits);

  /**
   * \brief Log single packet reception (event-driven)
   * Records individual packet reception events immediately
   *
   * \param nanoTime Time in nanoseconds
   * \param XpuId XPU identifier
   * \param devId Device identifier
   * \param vcId Virtual channel identifier
   * \param direction Direction ("Rx")
   * \param packetSizeBits Packet size in bits
   */
  void LogPacketRx (int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                    const std::string& direction, uint32_t packetSizeBits);

  /**
   * \brief Log application statistics
   *
   * \param time Time in nanoseconds
   * \param xpuId XPU identifier
   * \param devId Device identifier
   * \param vcId Virtual channel identifier
   * \param rate Data size
   */
  void LogAppStat (int64_t time, uint32_t xpuId, uint32_t devId, uint8_t vcId, double rate);

  /**
   * \brief Log packet drop statistics
   *
   * \param nanoTime Time in nanoseconds
   * \param XpuId XPU identifier
   * \param devId Device identifier
   * \param vcId Virtual channel identifier
   * \param direction Direction (TX/RX)
   * \param count Number of dropped packets
   */
  void LogDropStat (int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                    const std::string& direction, uint32_t count);

  /**
   * \brief Log packet drop statistics (event-driven)
   *
   * \param nanoTime Time in nanoseconds
   * \param XpuId XPU identifier
   * \param devId Device identifier
   * \param vcId Virtual channel identifier
   * \param dropReason Reason for packet drop
   * \param packetSize Size of dropped packet
   */
  void LogPacketDrop (int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                       const std::string& dropReason, uint32_t packetSize);

  /**
   * \brief Log credit statistics
   *
   * \param nanoTime Time in nanoseconds
   * \param XpuId XPU identifier
   * \param devId Device identifier
   * \param vcId Virtual channel identifier
   * \param direction Direction (TX/RX)
   * \param credits Number of credits
   * \param macAddress MAC address
   */
  void LogCreditStat (int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                      const std::string& direction, uint32_t credits, const std::string& macAddress);

  /**
   * \brief Log packing delay statistics
   *
   * \param xpuId XPU identifier
   * \param waitTimeNs Waiting time in nanoseconds
   */
  void LogPackDelay (uint32_t xpuId, int64_t waitTimeNs);

  /**
   * \brief Log packing number statistics
   *
   * \param xpuId XPU identifier
   * \param packNums Number of packed packets
   */
  void LogPackNum (uint32_t xpuId, uint32_t packNums);

  /**
   * \brief Log load balancing statistics
   *
   * \param localXpuId Local XPU identifier
   * \param destXpuId Destination XPU identifier
   * \param vcId Virtual channel identifier
   * \param sueId SUE identifier
   */
  void LogLoadBalance (uint32_t localXpuId, uint32_t destXpuId, uint8_t vcId, uint32_t sueId);

  /**
   * \brief Log destination queue usage statistics
   *
   * \param timeNs Time in nanoseconds
   * \param xpuId XPU identifier
   * \param sueId SUE identifier
   * \param destXpuId Destination XPU identifier
   * \param vcId Virtual channel identifier
   * \param currentBytes Current queue size in bytes
   * \param maxBytes Maximum queue size in bytes
   */
  void LogDestinationQueueUsage (uint64_t timeNs, uint32_t xpuId, uint32_t sueId,
                                 uint32_t destXpuId, uint8_t vcId, uint32_t currentBytes, uint32_t maxBytes);

  /**
   * \brief Log main queue usage statistics (event-driven)
   *
   * \param timeNs Time in nanoseconds
   * \param nodeId Node identifier
   * \param deviceId Device identifier
   * \param currentSize Current main queue size in bytes
   * \param maxSize Maximum main queue size in bytes
   */
  void LogMainQueueUsage (uint64_t timeNs, uint32_t nodeId, uint32_t deviceId,
                         uint32_t currentSize, uint32_t maxSize);

  /**
   * \brief Log VC queue usage statistics (event-driven)
   *
   * \param timeNs Time in nanoseconds
   * \param nodeId Node identifier
   * \param deviceId Device identifier
   * \param vcId Virtual channel identifier
   * \param currentSize Current VC queue size in bytes
   * \param maxSize Maximum VC queue size in bytes
   */
  void LogVCQueueUsage (uint64_t timeNs, uint32_t nodeId, uint32_t deviceId,
                       uint8_t vcId, uint32_t currentSize, uint32_t maxSize);

  
  /**
   * \brief Log processing queue usage statistics
   *
   * \param timeNs Time in nanoseconds
   * \param nodeId Node identifier
   * \param deviceId Device identifier
   * \param currentSize Current queue size
   * \param maxSize Maximum queue size
   */
  void LogProcessingQueueUsage (uint64_t timeNs, uint32_t nodeId, uint32_t deviceId,
                                uint32_t currentSize, uint32_t maxSize);

  /**
   * \brief Log XPU end-to-end delay statistics
   *
   * \param timeNs Time in nanoseconds
   * \param xpuId XPU identifier
   * \param portId Port identifier
   * \param delayNs Delay in nanoseconds
   */
  void LogXpuDelay (uint64_t timeNs, uint32_t xpuId, uint32_t portId, double delayNs);

  
  /**
   * \brief Buffer queue change trace callback
   *
   * \param bufferSize Current buffer queue size
   * \param xpuId XPU identifier
   */
  void BufferQueueChangeTraceCallback (uint32_t bufferSize, uint32_t xpuId);

private:
  /**
   * \brief Private constructor for singleton pattern
   */
  PerformanceLogger () = default;

  /**
   * \brief Private destructor
   */
  ~PerformanceLogger ();

  /**
   * \brief Copy constructor (disabled)
   *
   * \param other Other PerformanceLogger instance
   */
  PerformanceLogger (const PerformanceLogger &other);

  /**
   * \brief Assignment operator (disabled)
   *
   * \param other Other PerformanceLogger instance
   * \return Reference to this instance
   */
  PerformanceLogger& operator= (const PerformanceLogger &other);

  std::ofstream m_file;                    //!< Main log file stream
  std::string m_filename;                  //!< Base filename for logs

  // Packing log files
  std::ofstream m_packDelayLog;            //!< Packing delay log file stream
  std::ofstream m_packNumLog;              //!< Packing number log file stream

  // LoadBalancer log file
  std::ofstream m_loadBalanceLog;          //!< Load balancing log file stream

  // Queue usage monitoring log files
  std::ofstream m_destinationQueueLog;     //!< Destination queue log file stream
  std::ofstream m_mainQueueLog;            //!< Main queue log file stream
  std::ofstream m_vcQueueLog;              //!< VC queue log file stream

  // Link layer processing queue monitoring log file
  std::ofstream m_processingQueueLog;      //!< Processing queue log file stream

  // XPU delay monitoring log file
  std::ofstream m_xpuDelayLog;             //!< XPU end-to-end delay log file stream

  // Event-driven packet drop monitoring log file
  std::ofstream m_dropLog;                 //!< Packet drop log file stream

  // LoadBalancer monitoring log files
  std::ofstream m_sueBufferQueueLog;       //!< SUE buffer queue statistics log file stream

  // Link layer credit monitoring log file
  std::ofstream m_linkCreditLog;           //!< Link layer credit statistics log file stream
};

} // namespace ns3

#endif /* PERFORMANCE_LOGGER_H */