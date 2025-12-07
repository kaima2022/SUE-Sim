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

#ifndef TRAFFIC_GENERATOR_CONFIG_H
#define TRAFFIC_GENERATOR_CONFIG_H

#include "ns3/application.h"
#include "ns3/traffic-generator.h"
#include "ns3/sue-header.h"
#include "ns3/load-balancer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/data-rate.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"
#include "ns3/parameter-config.h"
#include <vector>

namespace ns3 {

/**
 * \ingroup point-to-point-sue
 * \class ConfigurableTrafficGenerator
 * \brief Configurable traffic generator for fine-grained flow control.
 *
 * This ConfigurableTrafficGenerator class provides fine-grained control over traffic
 * generation by reading fine-grained flow configurations from a file. Each flow
 * specifies source XPU, destination XPU, SUE ID, data rate, and total bytes.
 * This enables exact control over which XPU's SUE sends traffic to which destinations.
 */
class ConfigurableTrafficGenerator : public TrafficGenerator
{
public:
  /**
   * \brief Constructor
   */
  ConfigurableTrafficGenerator ();

  /**
   * \brief Destructor
   */
  virtual ~ConfigurableTrafficGenerator ();

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
   * \brief Set the local XPU ID
   *
   * \param localXpuId Local XPU identifier
   */
  void SetLocalXpuId (uint32_t localXpuId);

  /**
   * \brief Set fine-grained traffic flows from configuration
   *
   * \param flows Vector of fine-grained traffic flows
   */
  void SetFineGrainedFlows (const std::vector<FineGrainedTrafficFlow>& flows);

  /**
   * \brief Set client start time for accurate flow timing calculations
   *
   * \param clientStart Client start time in seconds
   */
  void SetClientStartTime (double clientStart);

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
   * \brief Generate transactions for all active flows
   */
  void GenerateTransactions ();

  /**
   * \brief Schedule the next transaction generation
   */
  void ScheduleNextTransaction ();

  /**
   * \brief Generate a transaction for a specific flow
   *
   * \param flowIndex Index of the flow to generate transaction for
   */
  void GenerateTransactionForFlow (uint32_t flowIndex);

  /**
   * \brief Add SUE header to transaction packet
   *
   * \param packet Packet to add header to
   * \param destXpuId Destination XPU ID
   * \param vcId Virtual channel ID
   */
  void AddSueHeader (Ptr<Packet> packet, uint32_t destXpuId, uint8_t vcId);

  /**
   * \brief Initialize active flows for this XPU
   */
  void InitializeActiveFlows ();

  /**
   * \brief Calculate next event time based on all active flows
   *
   * \return Time until next event
   */
  Time CalculateNextEventTime ();

  // Configuration parameters
  Ptr<LoadBalancer> m_loadBalancer;     //!< Load balancer for traffic distribution
  uint32_t m_transactionSize;           //!< Transaction size in bytes
  uint32_t m_localXpuId;                //!< Local XPU identifier
  double m_clientStart;                 //!< Client start time in seconds
  std::vector<FineGrainedTrafficFlow> m_fineGrainedFlows; //!< Fine-grained traffic flows configuration

  // Flow state tracking
  struct FlowState
  {
      uint64_t bytesSent;               //!< Bytes sent for this flow
      uint64_t lastGenerationTime;      //!< Last generation time for this flow (nanoseconds)
      bool isActive;                    //!< Whether this flow is currently active
      DataRate dataRate;                //!< Data rate for this flow
      Time packetInterval;              //!< Interval between packet generations
  };

  std::vector<FlowState> m_flowStates;  //!< State tracking for each flow
  std::vector<uint32_t> m_activeFlowIndices; //!< Indices of active flows for this XPU

  // Traffic control variables
  uint32_t m_maxBurstSize;              //!< Maximum burst size
  bool m_enableClientCBFC;              //!< Application layer CBFC enable flag
  uint32_t m_appInitCredit;             //!< Application layer initial credit
  bool m_transmissionComplete;          //!< Transmission completion flag

  // Internal state
  Ptr<UniformRandomVariable> m_rand;    //!< Random number generator
  uint32_t m_psn;                       //!< Packet sequence number
  EventId m_generateEvent;              //!< Next packet generation event
  uint64_t m_currentTime;               //!< Current simulation time tracking (nanoseconds)

  // Credit-based flow control
  bool m_generationPaused;              //!< Flag indicating if generation is paused
};

} // namespace ns3

#endif /* TRAFFIC_GENERATOR_CONFIG_H */