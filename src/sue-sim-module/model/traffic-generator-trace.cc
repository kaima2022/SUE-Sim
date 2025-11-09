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

#include "traffic-generator-trace.h"
#include "ns3/sue-header.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/ipv4-header.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/pointer.h"
#include "ns3/string.h"
#include <fstream>
#include <sstream>
#include <iomanip>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TraceTrafficGenerator");
NS_OBJECT_ENSURE_REGISTERED(TraceTrafficGenerator);

TypeId TraceTrafficGenerator::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::TraceTrafficGenerator")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<TraceTrafficGenerator>()
        .AddAttribute("TransactionSize",
                      "Size of a single transaction in bytes.",
                      UintegerValue(256),
                      MakeUintegerAccessor(&TraceTrafficGenerator::m_transactionSize),
                      MakeUintegerChecker<uint32_t>())
                .AddAttribute("MinXpuId",
                      "Minimum XPU ID for destination selection.",
                      UintegerValue(0),
                      MakeUintegerAccessor(&TraceTrafficGenerator::m_minXpuId),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("MaxXpuId",
                      "Maximum XPU ID for destination selection.",
                      UintegerValue(3),
                      MakeUintegerAccessor(&TraceTrafficGenerator::m_maxXpuId),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("MinVcId",
                      "Minimum VC ID for SUE header.",
                      UintegerValue(0),
                      MakeUintegerAccessor(&TraceTrafficGenerator::m_minVcId),
                      MakeUintegerChecker<uint8_t>())
        .AddAttribute("MaxVcId",
                      "Maximum VC ID for SUE header.",
                      UintegerValue(3),
                      MakeUintegerAccessor(&TraceTrafficGenerator::m_maxVcId),
                      MakeUintegerChecker<uint8_t>())
        .AddAttribute("EnableClientCBFC",
                     "If enable Client CBFC.",
                      BooleanValue(false),
                      MakeBooleanAccessor(&TraceTrafficGenerator::m_enableClientCBFC),
                      MakeBooleanChecker())
        .AddAttribute("AppInitCredit", "Application layer initial credit",
                      UintegerValue(30),
                      MakeUintegerAccessor(&TraceTrafficGenerator::m_appInitCredit),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("MaxBurstSize", "Maximum packed burst size in bytes",
                      UintegerValue(2048),
                      MakeUintegerAccessor(&TraceTrafficGenerator::m_maxBurstSize),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("TraceFile",
                      "Path to the trace file for trace-based traffic generation.",
                      StringValue(""),
                      MakeStringAccessor(&TraceTrafficGenerator::m_traceFilePath),
                      MakeStringChecker());
    return tid;
}

TraceTrafficGenerator::TraceTrafficGenerator()
    : m_transactionSize(256),
      m_minXpuId(0),
      m_maxXpuId(3),
      m_minVcId(0),
      m_maxVcId(3),
      m_localXpuId(0),
      m_bytesSent(0),
      m_enableClientCBFC(false),
      m_appInitCredit(30),
      m_maxBurstSize(2048),
      m_transmissionComplete(false),
      m_psn(0),
      m_generationPaused(false),
      m_currentTraceIndex(0),
      m_startTimestamp(0),
      m_lastTimestamp(0)
{
}

TraceTrafficGenerator::~TraceTrafficGenerator() {
    if (m_generateEvent.IsPending()) {
        Simulator::Cancel(m_generateEvent);
    }
}

void TraceTrafficGenerator::SetLoadBalancer(Ptr<LoadBalancer> loadBalancer) {
    m_loadBalancer = loadBalancer;
}

void TraceTrafficGenerator::SetTransactionSize(uint32_t size) {
    m_transactionSize = size;
}


void TraceTrafficGenerator::SetXpuIdRange(uint32_t minXpu, uint32_t maxXpu) {
    m_minXpuId = minXpu;
    m_maxXpuId = maxXpu;
}

void TraceTrafficGenerator::SetVcIdRange(uint8_t minVc, uint8_t maxVc) {
    m_minVcId = minVc;
    m_maxVcId = maxVc;
}

void TraceTrafficGenerator::SetLocalXpuId(uint32_t localXpuId) {
    m_localXpuId = localXpuId;
}


void TraceTrafficGenerator::StartApplication(void) {
    NS_LOG_INFO("TraceTrafficGenerator starting for XPU" << m_localXpuId + 1);

    // Initialize state for trace-based generation
    m_bytesSent = 0;
    m_transmissionComplete = false;

    // Load trace file
    NS_LOG_INFO("Loading trace file: '" << m_traceFilePath << "'");
    if (!m_traceFilePath.empty()) {
        if (LoadTraceFile(m_traceFilePath)) {
            NS_LOG_INFO("Trace file loaded successfully: " << m_traceFilePath << " with " << m_traceEntries.size() << " entries");

            // Initialize last timestamp for delay calculation
            if (!m_traceEntries.empty()) {
                m_lastTimestamp = m_traceEntries[0].timestamp;
            }

            // Debug: Print trace entries to console
            std::cout << "\n=== XPU" << (m_localXpuId + 1) << " TRACE LOADING DEBUG ===" << std::endl;
            std::cout << "Total entries loaded: " << m_traceEntries.size() << std::endl;

            if (m_traceEntries.empty()) {
                std::cout << "ERROR: No trace entries found!" << std::endl;
            } else {
                size_t printCount = std::min((size_t)5, m_traceEntries.size());
                for (size_t i = 0; i < printCount; ++i) {
                    const TraceEntry& entry = m_traceEntries[i];
                    std::cout << "Entry[" << i << "]: timestamp=" << entry.timestamp
                              << " gpu_id=" << entry.gpu_id
                              << " die_id=" << entry.die_id
                              << " operation=" << entry.operation
                              << " tile_id=" << entry.tile_id << std::endl;
                }
                if (m_traceEntries.size() > 5) {
                    std::cout << "... and " << (m_traceEntries.size() - 5) << " more entries" << std::endl;
                }
            }
            std::cout << "=== END DEBUG ===\n" << std::endl;

        } else {
            NS_LOG_ERROR("Failed to load trace file: " << m_traceFilePath);
            return;
        }
    } else {
        NS_LOG_ERROR("No trace file specified");
        return;
    }

    // Start first transaction generation based on trace timestamps
    if (!m_traceEntries.empty()) {
        m_currentTraceIndex = 0;
        GenerateTransaction();
    } else {
        NS_LOG_ERROR("No trace entries loaded");
        return;
    }
}

void TraceTrafficGenerator::StopApplication(void) {
    NS_LOG_INFO("TraceTrafficGenerator stopping for XPU" << m_localXpuId);

    // Cancel scheduled events
    if (m_generateEvent.IsPending()) {
        Simulator::Cancel(m_generateEvent);
    }
}


void TraceTrafficGenerator::GenerateTransaction() {
    // Debug: Print generation attempt
    std::cout << Simulator::Now().GetSeconds() << "s [XPU" << (m_localXpuId + 1)
              << "] GenerateTransaction called, m_currentTraceIndex=" << m_currentTraceIndex
              << "/" << m_traceEntries.size() << std::endl;

    // Check if traffic generation is paused
    if (m_generationPaused) {
        // If paused, continue scheduling but don't generate new transactions
        std::cout << "[XPU" << (m_localXpuId + 1) << "] Generation is PAUSED" << std::endl;
        ScheduleNextTraceTransaction();
        return;
    }

    // Check if there are trace entries to process
    if (m_currentTraceIndex >= m_traceEntries.size()) {
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [TraceTrafficGenerator XPU" << m_localXpuId + 1
                   << "] All trace entries processed. Stopping generation.");

        // Stop all Performance-Logger statistics events for SUEs on this XPU
        if (m_loadBalancer) {
            m_loadBalancer->StopAllLogging();
        }
        return;
    }

    // Check if LoadBalancer is available
    if (!m_loadBalancer) {
        NS_LOG_WARN("LoadBalancer not set, skipping transaction generation");
        ScheduleNextTraceTransaction();
        return;
    }

    // Only XPU 0 sends data
    if (m_localXpuId != 0) {
        // If not XPU 0, skip this transaction and schedule next
        std::cout << Simulator::Now().GetSeconds() << "s [XPU" << (m_localXpuId + 1)
                  << "] SKIPPED: Only XPU1 can send data" << std::endl;
        m_currentTraceIndex++;  // Still advance trace index to stay in sync

        // Check if this was the last trace entry
        if (m_currentTraceIndex >= m_traceEntries.size()) {
            std::cout << Simulator::Now().GetSeconds() << "s [XPU" << (m_localXpuId + 1)
                      << "] All trace entries processed. Stopping generation." << std::endl;
            return;  // Don't schedule next - this is the end
        }

        ScheduleNextTraceTransaction();
        return;
    }

    // XPU 0 sends the transaction
    std::cout << Simulator::Now().GetSeconds() << "s [XPU" << (m_localXpuId + 1)
              << "] SENDING TRANSACTION (XPU1 is active)" << std::endl;
    const TraceEntry& entry = m_traceEntries[m_currentTraceIndex];

    uint32_t destXpuId = entry.gpu_id;    // GPU_ID maps to XPU ID

    // Map operation type to VC ID
    uint8_t vcId;
    if (entry.operation == "LOAD") {
        vcId = 0;  // LOAD operations map to VC ID 0
    } else if (entry.operation == "STORE") {
        vcId = 1;  // STORE operations map to VC ID 1
    } else {
        vcId = 2;  // Other operations map to VC ID 2
    }

    // Set transaction size to 128B as specified
    m_transactionSize = 128;

    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [TraceTrafficGenerator XPU" << m_localXpuId + 1
               << "] Trace transaction #" << m_currentTraceIndex
               << " to XPU" << destXpuId + 1
               << " VC" << static_cast<uint32_t>(vcId)
               << " Size:" << m_transactionSize << " bytes (timestamp: " << entry.timestamp << ")");

    // Create transaction packet
    Ptr<Packet> transactionPacket = Create<Packet>(m_transactionSize);

    // Set SUE header
    SueHeader header;
    header.SetPsn(m_psn++);
    header.SetXpuId(destXpuId);
    header.SetVc(vcId);
    header.SetOp(0);  // Data packet

    transactionPacket->AddHeader(header);

    // Distribute transaction to SUE through LoadBalancer
    m_loadBalancer->DistributeTransaction(transactionPacket, destXpuId, vcId);

    // Update sent bytes count
    m_bytesSent += m_transactionSize;

    m_currentTraceIndex++;

    // Check if this was the last trace entry
    if (m_currentTraceIndex >= m_traceEntries.size()) {
        std::cout << Simulator::Now().GetSeconds() << "s [XPU" << (m_localXpuId + 1)
                  << "] All trace entries processed. Stopping generation." << std::endl;

        // If this is XPU 0 (the active one), stop the entire simulation
        if (m_localXpuId == 0) {
            std::cout << Simulator::Now().GetSeconds() << "s [XPU1] All traffic generation complete. Stopping simulation." << std::endl;
            Simulator::Stop();  // Force stop the simulation
        }

        return;  // Don't schedule next - this is the end
    }

    // Schedule next transaction generation
    ScheduleNextTraceTransaction();
}

// Traffic control method implementation
bool TraceTrafficGenerator::CheckTransmissionComplete() const {
    return m_currentTraceIndex >= m_traceEntries.size();
}

uint64_t TraceTrafficGenerator::GetRemainingBytes() const {
    if (m_currentTraceIndex >= m_traceEntries.size()) {
        return 0;
    }
    return m_traceEntries.size() - m_currentTraceIndex;
}

bool TraceTrafficGenerator::GetEnableClientCBFC() const {
    return m_enableClientCBFC;
}

void TraceTrafficGenerator::PauseGeneration() {
    NS_LOG_FUNCTION(this);

    if (!m_generationPaused) {
        m_generationPaused = true;
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [TraceTrafficGenerator XPU" << m_localXpuId + 1
                   << "] Generation paused by LoadBalancer");
    }
}

void TraceTrafficGenerator::ResumeGeneration() {
    NS_LOG_FUNCTION(this);

    if (m_generationPaused) {
        m_generationPaused = false;
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [TraceTrafficGenerator XPU" << m_localXpuId + 1
                   << "] Generation resumed by LoadBalancer");
    }
}

bool TraceTrafficGenerator::IsGenerationPaused() const {
    return m_generationPaused;
}

bool TraceTrafficGenerator::LoadTraceFile(const std::string& traceFile) {
    m_traceFilePath = traceFile;
    m_traceEntries.clear();
    m_currentTraceIndex = 0;
    m_startTimestamp = 0;

    std::ifstream file(traceFile);
    if (!file.is_open()) {
        NS_LOG_ERROR("Cannot open trace file: " << traceFile);
        return false;
    }

    std::string line;
    size_t matchingEntries = 0;
    bool firstEntry = true;
    bool headerSkipped = false;

    while (std::getline(file, line)) {
        // Skip empty lines
        if (line.empty()) {
            continue;
        }

        // Skip header line
        if (!headerSkipped) {
            headerSkipped = true;
            continue;
        }

        // Parse CSV format: Index,Timestamp,GPU_ID,Die_ID,Operation,Tile_ID
        std::istringstream iss(line);
        std::string field;
        std::vector<std::string> fields;

        while (std::getline(iss, field, ',')) {
            fields.push_back(field);
        }

        // Validate we have all required fields
        if (fields.size() < 6) {
            NS_LOG_WARN("Invalid trace line format: " << line);
            continue;
        }

        try {
            // Parse fields
            uint64_t timestamp = std::stoull(fields[1]);
            uint32_t gpu_id = std::stoul(fields[2]);
            uint32_t die_id = std::stoul(fields[3]);
            std::string operation = fields[4];
            uint32_t tile_id = std::stoul(fields[5]);

            // Filter conditions: include LOAD operations and all tiles
            if (operation == "STORE" && tile_id == 3) {
                TraceEntry entry;
                entry.timestamp = timestamp;
                entry.gpu_id = gpu_id;      // Maps to XPU ID
                entry.die_id = die_id;      // Maps to SUE instance
                entry.operation = operation; // LOAD maps to VC ID
                entry.tile_id = tile_id;

                m_traceEntries.push_back(entry);
                matchingEntries++;

                // Set the start timestamp to the first entry
                if (firstEntry) {
                    m_lastTimestamp = timestamp;
                    firstEntry = false;
                }
            }
        } catch (const std::exception& e) {
            NS_LOG_WARN("Error parsing trace line: " << line << " - " << e.what());
            continue;
        }
    }

    file.close();
    NS_LOG_INFO("Loaded " << matchingEntries << " trace entries from " << traceFile
                << " (filtered LOAD operations, all tiles, start timestamp: " << m_startTimestamp << ")");

    return !m_traceEntries.empty();
}


void TraceTrafficGenerator::ScheduleNextTraceTransaction() {
    if (m_currentTraceIndex >= m_traceEntries.size()) {
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [TraceTrafficGenerator XPU" << m_localXpuId + 1
                   << "] All trace entries processed. Stopping generation.");
        return;
    }

    if (m_generateEvent.IsExpired()) {
        const TraceEntry& entry = m_traceEntries[m_currentTraceIndex];

        // Calculate delay as the difference between current and last trace timestamps
        uint64_t delayNs = entry.timestamp - m_lastTimestamp;

        // Update last timestamp to current entry's timestamp
        m_lastTimestamp = entry.timestamp;

        // Convert to Time object for scheduling
        Time delay = NanoSeconds(delayNs);

        // Handle zero or negative delays (should not happen with this logic, but just in case)
        if (delay.IsNegative() || delay.IsZero()) {
            delay = NanoSeconds(1); // Schedule for next nanosecond
            std::cout << Simulator::Now().GetSeconds() << "s [XPU" << (m_localXpuId + 1)
                      << "] WARNING: Zero or negative delay, scheduling immediately" << std::endl;
        }

        // Schedule the transaction
        m_generateEvent = Simulator::Schedule(delay,
                                            &TraceTrafficGenerator::GenerateTransaction,
                                            this);

        std::cout << Simulator::Now().GetSeconds() << "s [XPU" << (m_localXpuId + 1)
                  << "] Scheduled next transaction in " << delay.GetNanoSeconds()
                  << " ns (current timestamp: " << entry.timestamp
                  << ", time interval from last: " << delayNs << " ns)" << std::endl;
    }
}

} // namespace ns3