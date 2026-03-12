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

#include "performance-logger.h"
#include "ns3/simulator.h"
#include <cstdlib>
#include <cerrno>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PerformanceLogger");

namespace {
bool EnvFlagIsOne (const char* name)
{
    const char* value = std::getenv (name);
    return value && std::string (value) == "1";
}

uint64_t EnvU64Or (const char* name, uint64_t defaultValue)
{
    const char* value = std::getenv (name);
    if (!value || *value == '\0')
    {
        return defaultValue;
    }
    errno = 0;
    char* end = nullptr;
    unsigned long long parsed = std::strtoull (value, &end, 10);
    if (errno != 0 || end == value)
    {
        return defaultValue;
    }
    return static_cast<uint64_t> (parsed);
}
} // namespace


void PerformanceLogger::Initialize(const std::string& filename) {
    // Define new directory structure
    std::string baseDir = "performance-data";
    std::string dataDir = baseDir + "/data";

    // Create main directory
    if (access(baseDir.c_str(), F_OK) != 0) {
        if (mkdir(baseDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << baseDir);
        }
    }

    // Create data directory
    if (access(dataDir.c_str(), F_OK) != 0) {
        if (mkdir(dataDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << dataDir);
        }
    }

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream timestamp;
    timestamp << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");

    m_runId = timestamp.str();
    m_totalTxBits.clear();
    m_totalRxBits.clear();

    const bool minimal = EnvFlagIsOne ("SUE_PERF_MINIMAL");
    m_progressThreadStop.store (true);
    if (m_progressThread.joinable ())
    {
        m_progressThread.join ();
    }
    m_progressIntervalMs = 0;
    m_progressLastSimTimeNs.store (0);
    m_progressTxBytesTotal.store (0);
    m_progressRxBytesTotal.store (0);
    m_progressDropsTotal.store (0);

    // Throughput data file - separate directory (can be disabled to avoid huge I/O)
    if (!minimal && !EnvFlagIsOne ("SUE_PERF_DISABLE_THROUGHPUT_LOG"))
    {
        std::string throughputLogDir = dataDir + "/throughput_logs";
        if (access(throughputLogDir.c_str(), F_OK) != 0) {
            if (mkdir(throughputLogDir.c_str(), 0777) != 0) {
                NS_FATAL_ERROR("Failed to create directory: " << throughputLogDir);
            }
        }
        m_filename = throughputLogDir + "/throughput_" + m_runId + ".csv";

        m_file.open(m_filename, std::ios::out | std::ios::trunc);
        if (!m_file.is_open()) {
            NS_FATAL_ERROR("Could not open throughput log file: " << m_filename);
        }
        // Write CSV header
        m_file << "Time,NodeId,DeviceId,VCId,Direction,DataSize\n";
    }

    if (!minimal)
    {
    // Packing delay log file - separate directory
    std::string waitTimeLogDir = dataDir + "/pack_wait_time_logs";
    if (access(waitTimeLogDir.c_str(), F_OK) != 0) {
        if (mkdir(waitTimeLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << waitTimeLogDir);
        }
    }
    std::ostringstream packDelayFilename;
    packDelayFilename << waitTimeLogDir << "/pack_wait_time_" << timestamp.str() << ".csv";
    m_packDelayLog.open(packDelayFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_packDelayLog.is_open()) {
        NS_FATAL_ERROR("Could not open pack delay log file: " << packDelayFilename.str());
    }
    m_packDelayLog << "XpuId,SueId,DestXpuId,VcId,WaitTime(ns)" << std::endl; // CSV header

    // Packing quantity log file - separate directory
    std::string packNumLogDir = dataDir + "/pack_num_logs";
    if (access(packNumLogDir.c_str(), F_OK) != 0) {
        if (mkdir(packNumLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << packNumLogDir);
        }
    }
    std::ostringstream packNumFilename;
    packNumFilename << packNumLogDir << "/pack_num_" << timestamp.str() << ".csv";
    m_packNumLog.open(packNumFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_packNumLog.is_open()) {
        NS_FATAL_ERROR("Could not open pack num log file: " << packNumFilename.str());
    }
    m_packNumLog << "XpuId,SueId,DestXpuId,VcId,PackNums" << std::endl; // CSV header

    // LoadBalancer log file - separate directory
    std::string loadBalanceLogDir = dataDir + "/load_balance_logs";
    if (access(loadBalanceLogDir.c_str(), F_OK) != 0) {
        if (mkdir(loadBalanceLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << loadBalanceLogDir);
        }
    }
    std::ostringstream loadBalanceFilename;
    loadBalanceFilename << loadBalanceLogDir << "/load_balance_" << timestamp.str() << ".csv";
    m_loadBalanceLog.open(loadBalanceFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_loadBalanceLog.is_open()) {
        NS_FATAL_ERROR("Could not open load balance log file: " << loadBalanceFilename.str());
    }
    m_loadBalanceLog << "LocalXpuId,DestXpuId,VcId,SueId" << std::endl; // CSV header

    // Destination queue utilization log file - separate directory
    std::string destQueueLogDir = dataDir + "/destination_queue_logs";
    if (access(destQueueLogDir.c_str(), F_OK) != 0) {
        if (mkdir(destQueueLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << destQueueLogDir);
        }
    }
    std::ostringstream destQueueFilename;
    destQueueFilename << destQueueLogDir << "/destination_queue_" << timestamp.str() << ".csv";
    m_destinationQueueLog.open(destQueueFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_destinationQueueLog.is_open()) {
        NS_FATAL_ERROR("Could not open destination queue log file: " << destQueueFilename.str());
    }
    m_destinationQueueLog << "TimeNs,XpuId,SueId,DestXpuId,VcId,CurrentSize,MaxSize,Utilization(%)" << std::endl;

    
    // Main queue utilization log file - separate directory
    std::string mainQueueLogDir = dataDir + "/main_queue_logs";
    if (access(mainQueueLogDir.c_str(), F_OK) != 0) {
        if (mkdir(mainQueueLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << mainQueueLogDir);
        }
    }
    std::ostringstream mainQueueFilename;
    mainQueueFilename << mainQueueLogDir << "/main_queue_" << timestamp.str() << ".csv";
    m_mainQueueLog.open(mainQueueFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_mainQueueLog.is_open()) {
        NS_FATAL_ERROR("Could not open main queue log file: " << mainQueueFilename.str());
    }
    m_mainQueueLog << "TimeNs,NodeId,DeviceId,CurrentSize,MaxSize,Utilization(%)" << std::endl;

    // VC queue utilization log file - separate directory
    std::string vcQueueLogDir = dataDir + "/vc_queue_logs";
    if (access(vcQueueLogDir.c_str(), F_OK) != 0) {
        if (mkdir(vcQueueLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << vcQueueLogDir);
        }
    }
    std::ostringstream vcQueueFilename;
    vcQueueFilename << vcQueueLogDir << "/vc_queue_" << timestamp.str() << ".csv";
    m_vcQueueLog.open(vcQueueFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_vcQueueLog.is_open()) {
        NS_FATAL_ERROR("Could not open VC queue log file: " << vcQueueFilename.str());
    }
    m_vcQueueLog << "TimeNs,NodeId,DeviceId,VCId,CurrentSize,MaxSize,Utilization(%)" << std::endl;

    // Link layer processing queue utilization log file - separate directory
    std::string processingQueueLogDir = dataDir + "/processing_queue_logs";
    if (access(processingQueueLogDir.c_str(), F_OK) != 0) {
        if (mkdir(processingQueueLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << processingQueueLogDir);
        }
    }
    std::ostringstream processingQueueFilename;
    processingQueueFilename << processingQueueLogDir << "/processing_queue_" << timestamp.str() << ".csv";
    m_processingQueueLog.open(processingQueueFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_processingQueueLog.is_open()) {
        NS_FATAL_ERROR("Could not open processing queue log file: " << processingQueueFilename.str());
    }
    m_processingQueueLog << "TimeNs,NodeId,DeviceId,QueueLength,MaxSize,Utilization(%)" << std::endl;

    // XPU delay monitoring log file - separate directory
    std::string xpuDelayLogDir = dataDir + "/xpu_delay_logs";
    if (access(xpuDelayLogDir.c_str(), F_OK) != 0) {
        if (mkdir(xpuDelayLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << xpuDelayLogDir);
        }
    }
    std::ostringstream xpuDelayFilename;
    xpuDelayFilename << xpuDelayLogDir << "/xpu_delay_" << timestamp.str() << ".csv";
    m_xpuDelayLog.open(xpuDelayFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_xpuDelayLog.is_open()) {
        NS_FATAL_ERROR("Could not open XPU delay log file: " << xpuDelayFilename.str());
    }
    m_xpuDelayLog << "TimeNs,NodeId,PortId,Delay(ns),Location" << std::endl;

    // SUE buffer queue monitoring log file - separate directory
    std::string sueBufferQueueLogDir = dataDir + "/sue_buffer_queue_logs";
    if (access(sueBufferQueueLogDir.c_str(), F_OK) != 0) {
        if (mkdir(sueBufferQueueLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << sueBufferQueueLogDir);
        }
    }
    std::ostringstream sueBufferQueueFilename;
    sueBufferQueueFilename << sueBufferQueueLogDir << "/sue_buffer_queue_" << timestamp.str() << ".csv";
    m_sueBufferQueueLog.open(sueBufferQueueFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_sueBufferQueueLog.is_open()) {
        NS_FATAL_ERROR("Could not open SUE buffer queue log file: " << sueBufferQueueFilename.str());
    }
    m_sueBufferQueueLog << "TimeNs,XpuId,BufferSize" << std::endl;

    // Link layer credit monitoring log file - separate directory (can be disabled to avoid huge I/O)
    if (!EnvFlagIsOne ("SUE_PERF_DISABLE_LINK_CREDIT_LOG"))
    {
        std::string linkCreditLogDir = dataDir + "/link_credit_logs";
        if (access(linkCreditLogDir.c_str(), F_OK) != 0) {
            if (mkdir(linkCreditLogDir.c_str(), 0777) != 0) {
                NS_FATAL_ERROR("Failed to create directory: " << linkCreditLogDir);
            }
        }
        std::ostringstream linkCreditFilename;
        linkCreditFilename << linkCreditLogDir << "/link_credit_" << timestamp.str() << ".csv";
        m_linkCreditLog.open(linkCreditFilename.str(), std::ios::out | std::ios::trunc);
        if (!m_linkCreditLog.is_open()) {
            NS_FATAL_ERROR("Could not open link credit log file: " << linkCreditFilename.str());
        }
        m_linkCreditLog << "TimeNs,NodeId,DeviceId,VCId,Direction,Credits,MacAddress" << std::endl;
    }
    }

    // Drop logging can be extremely I/O heavy under lossy scenarios (e.g., drop+LLR storms).
    // In minimal mode, disable per-packet drop CSV by default and rely on drop summary instead.
    m_dropTotal = 0;
    m_dropByReason.clear();
    m_dropLogLinesWritten = 0;
    const uint64_t defaultDropMaxLines =
        minimal ? 0 : std::numeric_limits<uint64_t>::max();
    m_dropLogMaxLines =
        EnvU64Or("SUE_PERF_DROP_LOG_MAX_LINES", defaultDropMaxLines);
    m_dropLogEnabled =
        (!EnvFlagIsOne("SUE_PERF_DISABLE_DROP_LOG") && m_dropLogMaxLines > 0);

    if (m_dropLogEnabled)
    {
        std::string dropLogDir = "performance-data/data/drop_logs";
        if (access(dropLogDir.c_str(), F_OK) != 0) {
            if (mkdir(dropLogDir.c_str(), 0777) != 0) {
                NS_FATAL_ERROR("Failed to create directory: " << dropLogDir);
            }
        }
        std::ostringstream dropFilename;
        dropFilename << dropLogDir << "/packet_drop_" << timestamp.str() << ".csv";
        m_dropLog.open(dropFilename.str(), std::ios::out | std::ios::trunc);
        if (!m_dropLog.is_open()) {
            NS_FATAL_ERROR("Could not open packet drop log file: " << dropFilename.str());
        }
        m_dropLog << "TimeNs,NodeId,DeviceId,VCId,DropReason,PacketSize" << std::endl;
    }

    if (!minimal)
    {
    // Create application layer transmission log directory
    std::string appLayerTxLogDir = dataDir + "/app_layer_tx";
    if (access(appLayerTxLogDir.c_str(), F_OK) != 0) {
        if (mkdir(appLayerTxLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << appLayerTxLogDir);
        }
    }
    std::stringstream appLayerTxFilename;
    appLayerTxFilename << appLayerTxLogDir << "/app_layer_tx_" << timestamp.str() << ".csv";
    m_appLayerTxLog.open(appLayerTxFilename.str(), std::ios::out | std::ios::trunc);
    if (!m_appLayerTxLog.is_open()) {
        NS_FATAL_ERROR("Could not open application layer transmission log file: " << appLayerTxFilename.str());
    }
    m_appLayerTxLog << "TimeNs,NodeId,VcId,PacketSize\n";
    }

    // Optional: Output debug information to standard output
    // std::cout << "PerformanceLogger initialized with directories:" << std::endl;
    // std::cout << "  Performance logs: " << performanceLogDir << std::endl;
    // std::cout << "  Packing logs: " << packingLogDir << std::endl;
    // std::cout << "  Main data file: " << m_filename << std::endl;

    // Optional: wall-clock progress snapshots (small file, useful for watchdog termination).
    //
    // Controlled via env:
    //   - SUE_PERF_PROGRESS_INTERVAL_MS (0 disables; default 0)
    //
    // Output path (within run_dir):
    //   performance-data/data/progress/progress.csv
    const uint64_t progressIntervalMs = EnvU64Or ("SUE_PERF_PROGRESS_INTERVAL_MS", 0);
    if (progressIntervalMs > 0)
    {
        std::string progressDir = dataDir + "/progress";
        if (access(progressDir.c_str(), F_OK) != 0)
        {
            if (mkdir(progressDir.c_str(), 0777) != 0)
            {
                NS_FATAL_ERROR("Failed to create directory: " << progressDir);
            }
        }

        const std::string progressFile = progressDir + "/progress.csv";
        m_progressLog.open(progressFile, std::ios::out | std::ios::trunc);
        if (!m_progressLog.is_open())
        {
            NS_FATAL_ERROR("Could not open progress log file: " << progressFile);
        }
        m_progressLog << "SimTimeNs,TxBytesTotal,RxBytesTotal,DropsTotal\n";
        m_progressLog << "0,0,0,0\n";
        m_progressLog.flush();

        m_progressIntervalMs = progressIntervalMs;
        m_progressThreadStop.store (false);
        m_progressThread = std::thread (&PerformanceLogger::ProgressThreadMain, this);
    }
}

void PerformanceLogger::LogDropStat(int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                                   const std::string& direction, uint32_t count) {
    if (m_file.is_open()) {
        m_file << nanoTime << "," << XpuId << "," << devId << "," << static_cast<int>(vcId)
               << "," << direction << "," << count << "\n";
    }
}


// === EVENT-DRIVEN STATISTICS FUNCTIONS ===

void PerformanceLogger::LogPacketTx(int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                                   const std::string& direction, uint32_t packetSizeBits) {
    if (direction == "Tx")
    {
        m_totalTxBits[XpuId] += packetSizeBits;
        m_progressTxBytesTotal.fetch_add (packetSizeBits / 8);
        if (nanoTime >= 0)
        {
            m_progressLastSimTimeNs.store (static_cast<uint64_t> (nanoTime));
        }
    }
    if (m_file.is_open()) {
        // Log individual packet transmission (event-driven)
        m_file << nanoTime << "," << XpuId << "," << devId << "," << static_cast<int>(vcId)
               << "," << direction << "," << packetSizeBits << "\n";  // Use packet size as DataSize
    }
}

void PerformanceLogger::LogPacketRx(int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                                   const std::string& direction, uint32_t packetSizeBits) {
    if (direction == "Rx")
    {
        m_totalRxBits[XpuId] += packetSizeBits;
        m_progressRxBytesTotal.fetch_add (packetSizeBits / 8);
        if (nanoTime >= 0)
        {
            m_progressLastSimTimeNs.store (static_cast<uint64_t> (nanoTime));
        }
    }
    if (m_file.is_open()) {
        // Log individual packet reception (event-driven)
        m_file << nanoTime << "," << XpuId << "," << devId << "," << static_cast<int>(vcId)
               << "," << direction << "," << packetSizeBits << "\n";  // Use packet size as DataSize
    }
}

void PerformanceLogger::LogAppStat(int64_t nanoTime, uint32_t xpuId, uint32_t devId, uint8_t vcId, double rate) {
    if (m_file.is_open()) {
        m_file << nanoTime << "," << xpuId << "," << devId  << "," << static_cast<int>(vcId)
               <<",APP," << rate << "\n";
    }
}

void PerformanceLogger::LogCreditStat(int64_t nanoTime, uint32_t NodeId, uint32_t devId, uint8_t vcId,
                                    const std::string& direction, uint32_t credits, const std::string& macAddress) {
    // Write independent link layer credit log file
    if (m_linkCreditLog.is_open()) {
        m_linkCreditLog << nanoTime << "," << NodeId << "," << devId << ","
                << static_cast<int>(vcId) << "," << direction << "," << credits << "," << macAddress << "\n";
    }
}

void PerformanceLogger::LogPackDelay(uint32_t xpuId, uint32_t sueId, uint32_t destXpuId,
                                     uint8_t vcId, int64_t waitTimeNs) {
    if (m_packDelayLog.is_open()) {
        m_packDelayLog << xpuId << "," << sueId << ","
                      << destXpuId << "," << static_cast<int>(vcId) << "," << waitTimeNs << "\n";
    }
}

void PerformanceLogger::LogPackNum(uint32_t xpuId, uint32_t sueId, uint32_t destXpuId,
                                  uint8_t vcId, uint32_t packNums) {
    if (m_packNumLog.is_open()) {
        m_packNumLog << xpuId << "," << sueId << ","
                    << destXpuId << "," << static_cast<int>(vcId) << "," << packNums << "\n";
    }
}

void PerformanceLogger::LogLoadBalance(uint32_t localXpuId, uint32_t destXpuId, uint8_t vcId, uint32_t sueId) {
    if (m_loadBalanceLog.is_open()) {
        m_loadBalanceLog << localXpuId << "," << destXpuId << "," << static_cast<int>(vcId) << "," << sueId << "\n";
    }
}

    // Queue utilization monitoring method implementation
void PerformanceLogger::LogDestinationQueueUsage(uint64_t timeNs, uint32_t xpuId, uint32_t sueId,
                                                   uint32_t destXpuId, uint8_t vcId, uint32_t currentBytes, uint32_t maxBytes) {
    if (m_destinationQueueLog.is_open()) {
        double utilization = (maxBytes > 0) ? (static_cast<double>(currentBytes) / maxBytes * 100.0) : 0.0;
        m_destinationQueueLog << timeNs << "," << xpuId << "," << sueId << ","
                             << destXpuId << "," << static_cast<int>(vcId) << "," << currentBytes << "," << maxBytes << ","
                             << std::fixed << std::setprecision(2) << utilization << "\n";
    }
}

void PerformanceLogger::LogMainQueueUsage(uint64_t timeNs, uint32_t nodeId, uint32_t deviceId,
                                         uint32_t currentSize, uint32_t maxSize) {
    if (m_mainQueueLog.is_open()) {
        double utilization = (maxSize > 0) ? (static_cast<double>(currentSize) / maxSize * 100.0) : 0.0;
        m_mainQueueLog << timeNs << "," << nodeId << "," << deviceId << ","
                      << currentSize << "," << maxSize << ","
                      << std::fixed << std::setprecision(2) << utilization << "\n";
    }
}

void PerformanceLogger::LogVCQueueUsage(uint64_t timeNs, uint32_t nodeId, uint32_t deviceId,
                                       uint8_t vcId, uint32_t currentSize, uint32_t maxSize) {
    if (m_vcQueueLog.is_open()) {
        double utilization = (maxSize > 0) ? (static_cast<double>(currentSize) / maxSize * 100.0) : 0.0;
        m_vcQueueLog << timeNs << "," << nodeId << "," << deviceId << ","
                    << static_cast<int>(vcId) << "," << currentSize << "," << maxSize << ","
                    << std::fixed << std::setprecision(2) << utilization << "\n";
    }
}


    // Link layer processing queue monitoring method implementation
void PerformanceLogger::LogProcessingQueueUsage(uint64_t timeNs, uint32_t nodeId, uint32_t deviceId,
                                                 uint32_t currentSize, uint32_t maxSize) {
    if (m_processingQueueLog.is_open()) {
        double utilization = (maxSize > 0) ? (static_cast<double>(currentSize) / maxSize * 100.0) : 0.0;
        m_processingQueueLog << timeNs << "," << nodeId << "," << deviceId << ","
                            << currentSize << "," << maxSize << ","
                            << std::fixed << std::setprecision(2) << utilization << "\n";
    }
}

    // XPU delay statistics method implementation
void PerformanceLogger::LogXpuDelay(uint64_t timeNs, uint32_t xpuId, uint32_t portId, double delayNs) {
    if (m_xpuDelayLog.is_open()) {
        m_xpuDelayLog << timeNs << "," << xpuId << "," << portId << ","
                      << std::fixed << std::setprecision(3) << delayNs << ",\n";
    }
}

// XPU delay statistics method with location implementation
void PerformanceLogger::LogXpuDelay(uint64_t timeNs, uint32_t xpuId, uint32_t portId, double delayNs, const std::string& location) {
    if (m_xpuDelayLog.is_open()) {
        m_xpuDelayLog << timeNs << "," << xpuId << "," << portId << ","
                      << std::fixed << std::setprecision(3) << delayNs << "," << location << "\n";
    }
}

void PerformanceLogger::LogAppLayerTx(uint64_t timeNs, uint32_t nodeId, uint8_t vcId, uint32_t packetSize) {
    if (m_appLayerTxLog.is_open()) {
        m_appLayerTxLog << timeNs << "," << nodeId << "," << static_cast<int>(vcId) << "," << packetSize << "\n";
    }
}

PerformanceLogger::~PerformanceLogger() {
    m_progressThreadStop.store (true);
    if (m_progressThread.joinable ())
    {
        m_progressThread.join ();
    }

    // Write a lightweight throughput summary (useful even when per-packet throughput is disabled).
    if (!m_runId.empty())
    {
        std::string summaryDir = "performance-data/data/throughput_summary_logs";
        if (access(summaryDir.c_str(), F_OK) != 0) {
            (void) mkdir(summaryDir.c_str(), 0777);
        }

        std::ostringstream summaryFilename;
        summaryFilename << summaryDir << "/throughput_summary_" << m_runId << ".csv";
        std::ofstream summary(summaryFilename.str(), std::ios::out | std::ios::trunc);
        if (summary.is_open())
        {
            summary << "NodeId,Direction,TotalDataSize\n";
            // Union of nodes seen in Tx/Rx maps
            std::map<uint32_t, bool> seen;
            for (const auto& [nodeId, _] : m_totalTxBits) { seen[nodeId] = true; }
            for (const auto& [nodeId, _] : m_totalRxBits) { seen[nodeId] = true; }

            for (const auto& [nodeId, _] : seen)
            {
                const uint64_t tx = m_totalTxBits.count(nodeId) ? m_totalTxBits.at(nodeId) : 0;
                const uint64_t rx = m_totalRxBits.count(nodeId) ? m_totalRxBits.at(nodeId) : 0;
                summary << nodeId << ",Tx," << tx << "\n";
                summary << nodeId << ",Rx," << rx << "\n";
            }
        }
    }

    // Write a lightweight drop summary (always small; preferred over per-packet drop logs).
    if (!m_runId.empty())
    {
        std::string summaryDir = "performance-data/data/drop_summary_logs";
        if (access(summaryDir.c_str(), F_OK) != 0) {
            (void) mkdir(summaryDir.c_str(), 0777);
        }

        std::ostringstream summaryFilename;
        summaryFilename << summaryDir << "/drop_summary_" << m_runId << ".csv";
        std::ofstream summary(summaryFilename.str(), std::ios::out | std::ios::trunc);
        if (summary.is_open())
        {
            summary << "DropReason,Count\n";
            summary << "TOTAL," << m_dropTotal << "\n";
            for (const auto& [reason, count] : m_dropByReason)
            {
                summary << reason << "," << count << "\n";
            }
        }
    }

    if (m_file.is_open()) {
        m_file.close();
    }
    if (m_packDelayLog.is_open()) {
        m_packDelayLog.close();
    }
    if (m_packNumLog.is_open()) {
        m_packNumLog.close();
    }
    if (m_loadBalanceLog.is_open()) {
        m_loadBalanceLog.close();
    }
    if (m_destinationQueueLog.is_open()) {
        m_destinationQueueLog.close();
    }
        if (m_mainQueueLog.is_open()) {
        m_mainQueueLog.close();
    }
    if (m_vcQueueLog.is_open()) {
        m_vcQueueLog.close();
    }
    if (m_processingQueueLog.is_open()) {
        m_processingQueueLog.close();
    }
    if (m_xpuDelayLog.is_open()) {
        m_xpuDelayLog.close();
    }
    if (m_dropLog.is_open()) {
        m_dropLog.close();
    }
    if (m_sueBufferQueueLog.is_open()) {
        m_sueBufferQueueLog.close();
    }
    if (m_linkCreditLog.is_open()) {
        m_linkCreditLog.close();
    }
    if (m_appLayerTxLog.is_open()) {
        m_appLayerTxLog.close();
    }
    if (m_progressLog.is_open()) {
        m_progressLog.close();
    }
}

void
PerformanceLogger::ProgressThreadMain ()
{
    while (!m_progressThreadStop.load ())
    {
        if (m_progressLog.is_open ())
        {
            const uint64_t simTimeNs = m_progressLastSimTimeNs.load ();
            const uint64_t txBytes = m_progressTxBytesTotal.load ();
            const uint64_t rxBytes = m_progressRxBytesTotal.load ();
            const uint64_t drops = m_progressDropsTotal.load ();
            m_progressLog << simTimeNs << "," << txBytes << "," << rxBytes << "," << drops << "\n";
            m_progressLog.flush ();
        }

        if (m_progressIntervalMs == 0)
        {
            break;
        }
        std::this_thread::sleep_for (std::chrono::milliseconds (m_progressIntervalMs));
    }
}

void
PerformanceLogger::LogPacketDrop (int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                                  const std::string& dropReason, uint32_t packetSize)
{
  NS_LOG_FUNCTION (nanoTime << XpuId << devId << (uint32_t)vcId << dropReason << packetSize);

  const std::string reason = dropReason.empty() ? "UNKNOWN" : dropReason;
  m_dropTotal++;
  m_dropByReason[reason] += 1;
  m_progressDropsTotal.fetch_add (1);
  if (nanoTime >= 0)
  {
      m_progressLastSimTimeNs.store (static_cast<uint64_t> (nanoTime));
  }

  if (!m_dropLogEnabled || !m_dropLog.is_open())
  {
      return;
  }
  if (m_dropLogLinesWritten >= m_dropLogMaxLines)
  {
      return;
  }
  m_dropLog << nanoTime << "," << XpuId << "," << devId << "," << (uint32_t)vcId << ","
            << reason << "," << packetSize << "\n";
  m_dropLogLinesWritten++;
}

void
PerformanceLogger::BufferQueueChangeTraceCallback (uint32_t bufferSize, uint32_t xpuId)
{
  NS_LOG_FUNCTION (this << bufferSize << xpuId);

  uint64_t timeNs = Simulator::Now ().GetNanoSeconds ();

  // Directly write buffer queue data
  if (m_sueBufferQueueLog.is_open()) {
      m_sueBufferQueueLog << timeNs << "," << xpuId << "," << bufferSize << "\n";
  }
}

PerformanceLogger&
PerformanceLogger::GetInstance ()
{
  static PerformanceLogger instance;
  return instance;
}

} // namespace ns3
