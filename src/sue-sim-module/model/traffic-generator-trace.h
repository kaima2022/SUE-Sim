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

#ifndef TRAFFIC_GENERATOR_TRACE_H
#define TRAFFIC_GENERATOR_TRACE_H

#include "ns3/application.h"
#include "ns3/traffic-generator.h"
#include "ns3/sue-header.h"
#include "ns3/load-balancer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"
#include <vector>
#include <string>
#include <fstream>

namespace ns3 {

/**
 * \brief Trace entry structure for storing parsed trace data
 */
struct TraceEntry
{
  uint64_t timestamp;        ///< Timestamp in nanoseconds
  uint32_t gpu_id;          ///< GPU ID (maps to XPU ID)
  uint32_t die_id;          ///< DIE ID (maps to SUE instance)
  std::string operation;    ///< Operation type (LOAD/STORE)
  uint32_t tile_id;         ///< Tile ID (filter: only process tile_id=3)

  TraceEntry()
    : timestamp(0), gpu_id(0), die_id(0), tile_id(0) {}
};

/**
 * \ingroup point-to-point-sue
 * \class TraceTrafficGenerator
 * \brief Trace-based traffic generator for SUE simulation.
 *
 * This TraceTrafficGenerator class generates traffic based on trace file patterns.
 * It parses trace files, extracts transaction information, and generates
 * transactions with SUE headers based on the trace data. Traffic is distributed
 * through a LoadBalancer to SUE clients.
 *
 * Key features:
 * - Parse trace files with timestamp-based scheduling
 * - Support LOAD operations with tile_id filtering
 * - Single XPU mode where only XPU 0 generates traffic
 * - Time-based transaction scheduling
 * - Compatible with existing LoadBalancer infrastructure
 */
class TraceTrafficGenerator : public TrafficGenerator
{
public:
  /**
   * \brief Constructor
   */
  TraceTrafficGenerator ();

  /**
   * \brief Destructor
   */
  virtual ~TraceTrafficGenerator ();

  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Set the load balancer for traffic distribution
   *
   * \param loadBalancer Pointer to the load balancer
   */
  void SetLoadBalancer (Ptr<LoadBalancer> loadBalancer);

  /**
   * \brief Set the transaction size
   *
   * \param size Transaction size in bytes
   */
  void SetTransactionSize (uint32_t size);

  
  /**
   * \brief Set the XPU ID range for destination selection
   *
   * \param minXpu Minimum XPU ID
   * \param maxXpu Maximum XPU ID
   */
  void SetXpuIdRange (uint32_t minXpu, uint32_t maxXpu);

  /**
   * \brief Set the VC ID range for virtual channel selection
   *
   * \param minVc Minimum VC ID
   * \param maxVc Maximum VC ID
   */
  void SetVcIdRange (uint8_t minVc, uint8_t maxVc);

  /**
   * \brief Set the local XPU ID
   *
   * \param localXpuId Local XPU identifier
   */
  void SetLocalXpuId (uint32_t localXpuId);

  /**
   * \brief Check if transmission is complete
   *
   * \return true if transmission is complete
   */
  bool CheckTransmissionComplete (void) const;

  /**
   * \brief Get remaining bytes to transmit
   *
   * \return Remaining bytes
   */
  uint64_t GetRemainingBytes (void) const;

  /**
   * \brief Get client CBFC enable status
   *
   * \return true if client CBFC is enabled
   */
  bool GetEnableClientCBFC (void) const;

  /**
   * \brief Pause traffic generation
   *
   * Called by LoadBalancer when all SUEs run out of credits
   */
  void PauseGeneration () override;

  /**
   * \brief Resume traffic generation
   *
   * Called by LoadBalancer when credits become available again
   */
  void ResumeGeneration () override;

  /**
   * \brief Check if traffic generation is currently paused
   *
   * \return true if traffic generation is paused
   */
  bool IsGenerationPaused () const;

  /**
   * \brief Load trace data from file
   *
   * \param traceFile Path to the trace file
   * \return true if trace loaded successfully
   */
  bool LoadTraceFile (const std::string& traceFile);

  
private:
  /**
   * \brief Application start method
   */
  void StartApplication (void) override;

  /**
   * \brief Application stop method
   */
  void StopApplication (void) override;

  /**
   * \brief Generate a transaction packet
   */
  void GenerateTransaction ();

  /**
   * \brief Schedule next transaction based on trace timestamp
   */
  void ScheduleNextTraceTransaction ();

  
  // Configuration parameters
  Ptr<LoadBalancer> m_loadBalancer;     //!< Load balancer for traffic distribution
  uint32_t m_transactionSize;           //!< Transaction size in bytes
  uint32_t m_minXpuId;                  //!< Minimum XPU ID for destination selection
  uint32_t m_maxXpuId;                  //!< Maximum XPU ID for destination selection
  uint8_t m_minVcId;                    //!< Minimum VC ID for virtual channel selection
  uint8_t m_maxVcId;                    //!< Maximum VC ID for virtual channel selection
  uint32_t m_localXpuId;                //!< Local XPU identifier

  // Traffic control variables
  uint64_t m_bytesSent;                 //!< Bytes already sent (for statistics)
  bool m_enableClientCBFC;              //!< Application layer CBFC enable flag
  uint32_t m_appInitCredit;             //!< Application layer initial credit
  uint32_t m_maxBurstSize;              //!< Maximum burst size
  bool m_transmissionComplete;          //!< Transmission completion flag

  // Internal state
  uint32_t m_psn;                       //!< Packet sequence number
  EventId m_generateEvent;              //!< Next packet generation event

  // Credit-based flow control
  bool m_generationPaused;              //!< Flag indicating if generation is paused

  // Trace mode support
  std::vector<TraceEntry> m_traceEntries; //!< Loaded trace entries
  size_t m_currentTraceIndex;           //!< Current trace entry index
  std::string m_traceFilePath;          //!< Path to trace file
  uint64_t m_startTimestamp;            //!< Start timestamp for time offset calculation
  uint64_t m_lastTimestamp;             //!< Last processed trace timestamp for delay calculation
};

} // namespace ns3

#endif /* TRAFFIC_GENERATOR_TRACE_H */