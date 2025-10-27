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

#include "traffic-generator.h"
#include "ns3/sue-header.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/ipv4-header.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/pointer.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TrafficGenerator");
NS_OBJECT_ENSURE_REGISTERED(TrafficGenerator);

TypeId TrafficGenerator::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::TrafficGenerator")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<TrafficGenerator>()
        .AddAttribute("TransactionSize",
                      "Size of a single transaction in bytes.",
                      UintegerValue(256),
                      MakeUintegerAccessor(&TrafficGenerator::m_transactionSize),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("DataRate",
                      "The data rate for traffic generation.",
                      DataRateValue(DataRate("1Mbps")),
                      MakeDataRateAccessor(&TrafficGenerator::m_dataRate),
                      MakeDataRateChecker())
        .AddAttribute("MinXpuId",
                      "Minimum XPU ID for destination selection.",
                      UintegerValue(0),
                      MakeUintegerAccessor(&TrafficGenerator::m_minXpuId),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("MaxXpuId",
                      "Maximum XPU ID for destination selection.",
                      UintegerValue(3),
                      MakeUintegerAccessor(&TrafficGenerator::m_maxXpuId),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("MinVcId",
                      "Minimum VC ID for SUE header.",
                      UintegerValue(0),
                      MakeUintegerAccessor(&TrafficGenerator::m_minVcId),
                      MakeUintegerChecker<uint8_t>())
        .AddAttribute("MaxVcId",
                      "Maximum VC ID for SUE header.",
                      UintegerValue(3),
                      MakeUintegerAccessor(&TrafficGenerator::m_maxVcId),
                      MakeUintegerChecker<uint8_t>())
        .AddAttribute("TotalBytesToSend",
                      "Total Bytes To Send.(MB)",
                      UintegerValue(1),
                      MakeUintegerAccessor(&TrafficGenerator::m_totalBytesToSend),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("EnableClientCBFC",
                     "If enable Client CBFC.",
                      BooleanValue(false),
                      MakeBooleanAccessor(&TrafficGenerator::m_enableClientCBFC),
                      MakeBooleanChecker())
        .AddAttribute("AppInitCredit", "Application layer initial credit",
                      UintegerValue(30),
                      MakeUintegerAccessor(&TrafficGenerator::m_appInitCredit),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("MaxBurstSize", "Maximum packed burst size in bytes",
                      UintegerValue(2048),
                      MakeUintegerAccessor(&TrafficGenerator::m_maxBurstSize),
                      MakeUintegerChecker<uint32_t>());
    return tid;
}

TrafficGenerator::TrafficGenerator()
    : m_transactionSize(256),
      m_dataRate(DataRate("1Mbps")),
      m_minXpuId(0),
      m_maxXpuId(3),
      m_minVcId(0),
      m_maxVcId(3),
      m_localXpuId(0),
      m_totalBytesToSend(1),
      m_bytesSent(0),
      m_enableClientCBFC(false),
      m_appInitCredit(30),
      m_maxBurstSize(2048),
      m_transmissionComplete(false),
      m_psn(0),
      m_generationPaused(false)
{
    m_rand = CreateObject<UniformRandomVariable>();
}

TrafficGenerator::~TrafficGenerator() {
    if (m_generateEvent.IsPending()) {
        Simulator::Cancel(m_generateEvent);
    }
}

void TrafficGenerator::SetLoadBalancer(Ptr<LoadBalancer> loadBalancer) {
    m_loadBalancer = loadBalancer;
}

void TrafficGenerator::SetTransactionSize(uint32_t size) {
    m_transactionSize = size;
}

void TrafficGenerator::SetDataRate(DataRate rate) {
    m_dataRate = rate;
    // Recalculate packet interval time
    m_packetInterval = Seconds(m_transactionSize * 8.0 / m_dataRate.GetBitRate());
}

void TrafficGenerator::SetXpuIdRange(uint32_t minXpu, uint32_t maxXpu) {
    m_minXpuId = minXpu;
    m_maxXpuId = maxXpu;
}

void TrafficGenerator::SetVcIdRange(uint8_t minVc, uint8_t maxVc) {
    m_minVcId = minVc;
    m_maxVcId = maxVc;
}

void TrafficGenerator::SetLocalXpuId(uint32_t localXpuId) {
    m_localXpuId = localXpuId;
}


void TrafficGenerator::StartApplication(void) {
    NS_LOG_INFO("TrafficGenerator starting for XPU" << m_localXpuId + 1);

    // Calculate packet interval time
    m_packetInterval = Seconds(m_transactionSize * 8.0 / m_dataRate.GetBitRate());

    // Initialize transmission volume (MB to bytes conversion)
    m_totalBytesToSend = m_totalBytesToSend * 1024 * 1024;
    m_bytesSent = 0;
    m_transmissionComplete = false;

    // Schedule first transaction generation
    ScheduleNextTransaction();
}

void TrafficGenerator::StopApplication(void) {
    NS_LOG_INFO("TrafficGenerator stopping for XPU" << m_localXpuId);

    // Cancel scheduled events
    if (m_generateEvent.IsPending()) {
        Simulator::Cancel(m_generateEvent);
    }
}

void TrafficGenerator::ScheduleNextTransaction() {
    if (m_generateEvent.IsExpired()) {
        m_generateEvent = Simulator::Schedule(m_packetInterval,
                                            &TrafficGenerator::GenerateTransaction,
                                            this);
    }
}

void TrafficGenerator::GenerateTransaction() {
    // Check if traffic generation is paused
    if (m_generationPaused) {
        // If paused, continue scheduling but don't generate new transactions
        ScheduleNextTransaction();
        return;
    }

    // Check if transmission task is completed
    if (CheckTransmissionComplete()) {
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [TrafficGenerator XPU" << m_localXpuId + 1
                   << "] Transmission complete. Stopping generation and logging.");

        // Stop all Performance-Logger statistics events for SUEs on this XPU
        if (m_loadBalancer) {
            m_loadBalancer->StopAllSueLogging();
        }

        return;
    }

    // Check if LoadBalancer is available
    if (!m_loadBalancer) {
        NS_LOG_WARN("LoadBalancer not set, skipping transaction generation");
        ScheduleNextTransaction();
        return;
    }

    // Randomly select target XPU (cannot be the same as local XPU)
    uint32_t destXpuId = m_rand->GetInteger(m_minXpuId, m_maxXpuId);
    while (destXpuId == m_localXpuId) {
        destXpuId = (destXpuId + 1) % (m_maxXpuId + 1);
    }

    // Randomly select VC ID
    uint8_t vcId = m_rand->GetInteger(m_minVcId, m_maxVcId);

    // Create transaction packet
    Ptr<Packet> transactionPacket = Create<Packet>(m_transactionSize);

    // Set SUE header
    SueHeader header;
    header.SetPsn(m_psn++);
    header.SetXpuId(destXpuId);
    header.SetVc(vcId);
    header.SetOp(0);  // Data packet

    transactionPacket->AddHeader(header);

    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [TrafficGenerator XPU" << m_localXpuId + 1
               << "] Generated transaction to XPU" << destXpuId + 1
               << " VC" << static_cast<uint32_t>(vcId)
               << " Size:" << m_transactionSize << " bytes");

    // Distribute transaction to SUE through LoadBalancer
    m_loadBalancer->DistributeTransaction(transactionPacket, destXpuId, vcId);

    // Update sent bytes count
    m_bytesSent += m_transactionSize;

    // Schedule next transaction generation
    ScheduleNextTransaction();
}

// Traffic control method implementation
bool TrafficGenerator::CheckTransmissionComplete() const {
    return m_bytesSent >= m_totalBytesToSend;
}

uint64_t TrafficGenerator::GetRemainingBytes() const {
    if (m_bytesSent >= m_totalBytesToSend) {
        return 0;
    }
    return m_totalBytesToSend - m_bytesSent;
}

bool TrafficGenerator::GetEnableClientCBFC() const {
    return m_enableClientCBFC;
}

void TrafficGenerator::PauseGeneration() {
    NS_LOG_FUNCTION(this);

    if (!m_generationPaused) {
        m_generationPaused = true;
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [TrafficGenerator XPU" << m_localXpuId + 1
                   << "] Generation paused by LoadBalancer");
    }
}

void TrafficGenerator::ResumeGeneration() {
    NS_LOG_FUNCTION(this);

    if (m_generationPaused) {
        m_generationPaused = false;
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [TrafficGenerator XPU" << m_localXpuId + 1
                   << "] Generation resumed by LoadBalancer");
    }
}

bool TrafficGenerator::IsGenerationPaused() const {
    return m_generationPaused;
}

} // namespace ns3