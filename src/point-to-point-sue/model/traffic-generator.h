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

#ifndef TRAFFIC_GENERATOR_H
#define TRAFFIC_GENERATOR_H

#include "ns3/application.h"
#include "ns3/sue-header.h"
#include "ns3/load-balancer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/data-rate.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/socket.h"

namespace ns3 {

/**
 * \ingroup point-to-point-sue
 * \class TrafficGenerator
 * \brief Traffic generator for SUE simulation.
 *
 * This TrafficGenerator class replaces the original TxCallback mechanism
 * with a unified traffic generation system. It generates raw transaction
 * packets, sets SUE headers with randomized VC and XPU IDs, and distributes
 * traffic through a LoadBalancer to SUE clients.
 */
class TrafficGenerator : public Application
{
public:
  /**
   * \brief Constructor
   */
  TrafficGenerator ();

  /**
   * \brief Destructor
   */
  virtual ~TrafficGenerator ();

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
   * \brief Set the data generation rate
   *
   * \param rate Data rate
   */
  void SetDataRate (DataRate rate);

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
  void PauseGeneration ();

  /**
   * \brief Resume traffic generation
   *
   * Called by LoadBalancer when credits become available again
   */
  void ResumeGeneration ();

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
   * \brief Generate a transaction packet
   */
  void GenerateTransaction ();

  /**
   * \brief Schedule the next transaction
   */
  void ScheduleNextTransaction ();

  /**
   * \brief Select a random destination XPU (excluding local)
   *
   * \return Selected XPU ID
   */
  uint32_t SelectRandomDestination ();

  /**
   * \brief Add SUE header to transaction packet
   *
   * \param packet Packet to add header to
   * \param destXpuId Destination XPU ID
   * \param vcId Virtual channel ID
   */
  void AddSueHeader (Ptr<Packet> packet, uint32_t destXpuId, uint8_t vcId);

  // Configuration parameters
  Ptr<LoadBalancer> m_loadBalancer;     //!< Load balancer for traffic distribution
  uint32_t m_transactionSize;           //!< Transaction size in bytes
  DataRate m_dataRate;                  //!< Data generation rate
  uint32_t m_minXpuId;                  //!< Minimum XPU ID for destination selection
  uint32_t m_maxXpuId;                  //!< Maximum XPU ID for destination selection
  uint8_t m_minVcId;                    //!< Minimum VC ID for virtual channel selection
  uint8_t m_maxVcId;                    //!< Maximum VC ID for virtual channel selection
  uint32_t m_localXpuId;                //!< Local XPU identifier

  // Traffic control variables
  uint32_t m_totalBytesToSend;          //!< Total bytes to send (MB)
  uint64_t m_bytesSent;                 //!< Bytes already sent
  bool m_enableClientCBFC;              //!< Application layer CBFC enable flag
  uint32_t m_appInitCredit;             //!< Application layer initial credit
  uint32_t m_maxBurstSize;              //!< Maximum burst size
  bool m_transmissionComplete;          //!< Transmission completion flag

  // Internal state
  Ptr<UniformRandomVariable> m_rand;    //!< Random number generator
  uint32_t m_psn;                       //!< Packet sequence number
  EventId m_generateEvent;              //!< Next packet generation event
  Time m_packetInterval;                //!< Interval between packet generations

  // Credit-based flow control
  bool m_generationPaused;              //!< Flag indicating if generation is paused
};

} // namespace ns3

#endif /* TRAFFIC_GENERATOR_H */