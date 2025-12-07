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

#include "performance-logger.h"
#include "ns3/simulator.h"
#include <sys/stat.h>
#include <unistd.h>
#include <chrono>
#include <sstream>
#include <iomanip>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PerformanceLogger");


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

    // Throughput data file - separate directory
    std::string throughputLogDir = dataDir + "/throughput_logs";
    if (access(throughputLogDir.c_str(), F_OK) != 0) {
        if (mkdir(throughputLogDir.c_str(), 0777) != 0) {
            NS_FATAL_ERROR("Failed to create directory: " << throughputLogDir);
        }
    }
    m_filename = throughputLogDir + "/throughput_" + timestamp.str() + ".csv";

    m_file.open(m_filename, std::ios::out | std::ios::trunc);
    if (!m_file.is_open()) {
        NS_FATAL_ERROR("Could not open throughput log file: " << m_filename);
    }
    // Write CSV header
    m_file << "Time,NodeId,DeviceId,VCId,Direction,DataSize\n";

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

    // Link layer credit monitoring log file - separate directory
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

    // Initialize event-driven packet drop log file
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
    m_dropLog << "TimeNs,NodeId,DeviceId,VCId,DropReason,PacketSize,QueueSize" << std::endl;

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
    m_appLayerTxLog << "TimeNs,NodeId,VcId,PacketSize" << std::endl;

    // Optional: Output debug information to standard output
    // std::cout << "PerformanceLogger initialized with directories:" << std::endl;
    // std::cout << "  Performance logs: " << performanceLogDir << std::endl;
    // std::cout << "  Packing logs: " << packingLogDir << std::endl;
    // std::cout << "  Main data file: " << m_filename << std::endl;
}

void PerformanceLogger::LogDropStat(int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                                   const std::string& direction, uint32_t count) {
    if (m_file.is_open()) {
        m_file << nanoTime << "," << XpuId << "," << devId << "," << static_cast<int>(vcId)
               << "," << direction << "," << count << "\n";
        m_file.flush(); // Ensure data is written to disk
    }
}


// === EVENT-DRIVEN STATISTICS FUNCTIONS ===

void PerformanceLogger::LogPacketTx(int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                                   const std::string& direction, uint32_t packetSizeBits) {
    if (m_file.is_open()) {
        // Log individual packet transmission (event-driven)
        m_file << nanoTime << "," << XpuId << "," << devId << "," << static_cast<int>(vcId)
               << "," << direction << "," << packetSizeBits << "\n";  // Use packet size as DataSize
        m_file.flush(); // Ensure data is written to disk immediately
    }
}

void PerformanceLogger::LogPacketRx(int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                                   const std::string& direction, uint32_t packetSizeBits) {
    if (m_file.is_open()) {
        // Log individual packet reception (event-driven)
        m_file << nanoTime << "," << XpuId << "," << devId << "," << static_cast<int>(vcId)
               << "," << direction << "," << packetSizeBits << "\n";  // Use packet size as DataSize
        m_file.flush(); // Ensure data is written to disk immediately
    }
}

void PerformanceLogger::LogAppStat(int64_t nanoTime, uint32_t xpuId, uint32_t devId, uint8_t vcId, double rate) {
    if (m_file.is_open()) {
        m_file << nanoTime << "," << xpuId << "," << devId  << "," << static_cast<int>(vcId)
               <<",APP," << rate << "\n";
        m_file.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogCreditStat(int64_t nanoTime, uint32_t NodeId, uint32_t devId, uint8_t vcId,
                                    const std::string& direction, uint32_t credits, const std::string& macAddress) {
    // Write independent link layer credit log file
    if (m_linkCreditLog.is_open()) {
        m_linkCreditLog << nanoTime << "," << NodeId << "," << devId << ","
                << static_cast<int>(vcId) << "," << direction << "," << credits << "," << macAddress << "\n";
        m_linkCreditLog.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogPackDelay(uint32_t xpuId, uint32_t sueId, uint32_t destXpuId,
                                     uint8_t vcId, int64_t waitTimeNs) {
    if (m_packDelayLog.is_open()) {
        m_packDelayLog << xpuId << "," << sueId << ","
                      << destXpuId << "," << static_cast<int>(vcId) << "," << waitTimeNs << std::endl;
        m_packDelayLog.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogPackNum(uint32_t xpuId, uint32_t sueId, uint32_t destXpuId,
                                  uint8_t vcId, uint32_t packNums) {
    if (m_packNumLog.is_open()) {
        m_packNumLog << xpuId << "," << sueId << ","
                    << destXpuId << "," << static_cast<int>(vcId) << "," << packNums << std::endl;
        m_packNumLog.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogLoadBalance(uint32_t localXpuId, uint32_t destXpuId, uint8_t vcId, uint32_t sueId) {
    if (m_loadBalanceLog.is_open()) {
        m_loadBalanceLog << localXpuId << "," << destXpuId << "," << static_cast<int>(vcId) << "," << sueId << std::endl;
        m_loadBalanceLog.flush(); // Ensure data is written to disk
    }
}

    // Queue utilization monitoring method implementation
void PerformanceLogger::LogDestinationQueueUsage(uint64_t timeNs, uint32_t xpuId, uint32_t sueId,
                                                   uint32_t destXpuId, uint8_t vcId, uint32_t currentBytes, uint32_t maxBytes) {
    if (m_destinationQueueLog.is_open()) {
        double utilization = (maxBytes > 0) ? (static_cast<double>(currentBytes) / maxBytes * 100.0) : 0.0;
        m_destinationQueueLog << timeNs << "," << xpuId << "," << sueId << ","
                             << destXpuId << "," << static_cast<int>(vcId) << "," << currentBytes << "," << maxBytes << ","
                             << std::fixed << std::setprecision(2) << utilization << std::endl;
        m_destinationQueueLog.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogMainQueueUsage(uint64_t timeNs, uint32_t nodeId, uint32_t deviceId,
                                         uint32_t currentSize, uint32_t maxSize) {
    if (m_mainQueueLog.is_open()) {
        double utilization = (maxSize > 0) ? (static_cast<double>(currentSize) / maxSize * 100.0) : 0.0;
        m_mainQueueLog << timeNs << "," << nodeId << "," << deviceId << ","
                      << currentSize << "," << maxSize << ","
                      << std::fixed << std::setprecision(2) << utilization << std::endl;
        m_mainQueueLog.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogVCQueueUsage(uint64_t timeNs, uint32_t nodeId, uint32_t deviceId,
                                       uint8_t vcId, uint32_t currentSize, uint32_t maxSize) {
    if (m_vcQueueLog.is_open()) {
        double utilization = (maxSize > 0) ? (static_cast<double>(currentSize) / maxSize * 100.0) : 0.0;
        m_vcQueueLog << timeNs << "," << nodeId << "," << deviceId << ","
                    << static_cast<int>(vcId) << "," << currentSize << "," << maxSize << ","
                    << std::fixed << std::setprecision(2) << utilization << std::endl;
        m_vcQueueLog.flush(); // Ensure data is written to disk
    }
}


    // Link layer processing queue monitoring method implementation
void PerformanceLogger::LogProcessingQueueUsage(uint64_t timeNs, uint32_t nodeId, uint32_t deviceId,
                                                 uint32_t currentSize, uint32_t maxSize) {
    if (m_processingQueueLog.is_open()) {
        double utilization = (maxSize > 0) ? (static_cast<double>(currentSize) / maxSize * 100.0) : 0.0;
        m_processingQueueLog << timeNs << "," << nodeId << "," << deviceId << ","
                            << currentSize << "," << maxSize << ","
                            << std::fixed << std::setprecision(2) << utilization << std::endl;
        m_processingQueueLog.flush(); // Ensure data is written to disk
    }
}

    // XPU delay statistics method implementation
void PerformanceLogger::LogXpuDelay(uint64_t timeNs, uint32_t xpuId, uint32_t portId, double delayNs) {
    if (m_xpuDelayLog.is_open()) {
        m_xpuDelayLog << timeNs << "," << xpuId << "," << portId << ","
                      << std::fixed << std::setprecision(3) << delayNs << "," << std::endl;
        m_xpuDelayLog.flush(); // Ensure data is written to disk
    }
}

// XPU delay statistics method with location implementation
void PerformanceLogger::LogXpuDelay(uint64_t timeNs, uint32_t xpuId, uint32_t portId, double delayNs, const std::string& location) {
    if (m_xpuDelayLog.is_open()) {
        m_xpuDelayLog << timeNs << "," << xpuId << "," << portId << ","
                      << std::fixed << std::setprecision(3) << delayNs << "," << location << std::endl;
        m_xpuDelayLog.flush(); // Ensure data is written to disk
    }
}

void PerformanceLogger::LogAppLayerTx(uint64_t timeNs, uint32_t nodeId, uint8_t vcId, uint32_t packetSize) {
    if (m_appLayerTxLog.is_open()) {
        m_appLayerTxLog << timeNs << "," << nodeId << "," << static_cast<int>(vcId) << "," << packetSize << std::endl;
        m_appLayerTxLog.flush(); // Ensure data is written to disk
    }
}

PerformanceLogger::~PerformanceLogger() {
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
}

void
PerformanceLogger::LogPacketDrop (int64_t nanoTime, uint32_t XpuId, uint32_t devId, uint8_t vcId,
                                  const std::string& dropReason, uint32_t packetSize)
{
  NS_LOG_FUNCTION (nanoTime << XpuId << devId << (uint32_t)vcId << dropReason << packetSize);

  // Directly write packet drop data (event-driven)
  if (m_dropLog.is_open()) {
      m_dropLog << nanoTime << "," << XpuId << "," << devId << "," << (uint32_t)vcId << ","
                << dropReason << "," << packetSize << std::endl;
      m_dropLog.flush(); // Ensure data is written to disk
  }
}

void
PerformanceLogger::BufferQueueChangeTraceCallback (uint32_t bufferSize, uint32_t xpuId)
{
  NS_LOG_FUNCTION (this << bufferSize << xpuId);

  uint64_t timeNs = Simulator::Now ().GetNanoSeconds ();

  // Directly write buffer queue data
  if (m_sueBufferQueueLog.is_open()) {
      m_sueBufferQueueLog << timeNs << "," << xpuId << "," << bufferSize << std::endl;
      m_sueBufferQueueLog.flush(); // Ensure data is written to disk
  }
}

PerformanceLogger&
PerformanceLogger::GetInstance ()
{
  static PerformanceLogger instance;
  return instance;
}

} // namespace ns3