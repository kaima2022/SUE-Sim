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

#ifndef SUE_UTILS_H
#define SUE_UTILS_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include <string>

// Forward declarations
namespace ns3 {
    class CbfcManager;
    class SueQueueManager;
}

namespace ns3 {

/**
 * \brief String parsing utility class
 */
class SueStringUtils {
public:
    /**
     * \brief Parse data rate string (e.g., "200Gbps", "100Mbps")
     * \param rateStr Data rate string to parse
     * \return Corresponding DataRate object, returns DataRate(0) on failure
     */
    static DataRate ParseDataRateString(const std::string& rateStr);

    /**
     * \brief Parse time interval string (supports us to ns conversion)
     * \param timeStr Time string to parse
     * \return Corresponding Time object, returns Time::Max() on failure
     */
    static Time ParseTimeIntervalString(const std::string& timeStr);

    /**
     * \brief Safe string to number conversion
     * \param numStr Number string to convert
     * \param defaultValue Default value on conversion failure
     * \return Converted number value
     */
    static double SafeStringToDouble(const std::string& numStr, double defaultValue = 0.0);
};

/**
 * \brief SUE-specific logging utility class
 */
class SueLogUtils {
public:
    /**
     * \brief Format device information log
     * \param operation Operation type
     * \param device Network device
     * \param details Additional details
     */
    static void LogDeviceInfo(const std::string& operation,
                             Ptr<NetDevice> device,
                             const std::string& details = "");

    /**
     * \brief Format transmission statistics log
     * \param operation Operation type
     * \param nodeId Node ID
     * \param deviceId Device ID
     * \param vcId Virtual channel ID
     * \param sequence Sequence number
     * \param status Status information
     */
    static void LogTransmissionStats(const std::string& operation,
                                    uint32_t nodeId,
                                    uint32_t deviceId,
                                    uint32_t vcId,
                                    uint32_t sequence,
                                    const std::string& status);
};

/**
 * \brief SUE statistics utility class
 */
class SueStatsUtils {
public:
    /**
     * \brief Process sent packet statistics
     * \param packet Packet to process
     * \param vcBytesSentMap Reference to VC bytes sent map to update
     * \param nodeId Node identifier (base-0)
     * \param deviceId Device identifier (base-0)
     */
    static void ProcessSentPacketStats(Ptr<Packet> packet,
                                      std::map<uint8_t, uint64_t>& vcBytesSentMap,
                                      uint32_t nodeId, uint32_t deviceId);

    /**
     * \brief Process received packet statistics
     * \param packet Packet to process
     * \param vcBytesReceivedMap Reference to VC bytes received map to update
     * \param nodeId Node identifier (base-0)
     * \param deviceId Device identifier (base-0)
     */
    static void ProcessReceivedPacketStats(Ptr<Packet> packet,
                                         std::map<uint8_t, uint64_t>& vcBytesReceivedMap,
                                         uint32_t nodeId, uint32_t deviceId);

    // === EVENT-DRIVEN STATISTICS FUNCTIONS ===

    /**
     * \brief Log packet transmission statistics immediately with explicit context
     * Event-driven logging when packets are transmitted
     * \param node Pointer to the node
     * \param deviceId Device ID
     * \param packet The transmitted packet
     * \param vcId Virtual channel ID
     */
    static void LogPacketTxStatsWithNode(Ptr<Node> node, uint32_t deviceId,
                                        Ptr<Packet> packet, uint8_t vcId);

    /**
     * \brief Log packet reception statistics immediately with explicit context
     * Event-driven logging when packets are received
     * \param node Pointer to the node
     * \param deviceId Device ID
     * \param packet The received packet
     * \param vcId Virtual channel ID
     */
    static void LogPacketRxStatsWithNode(Ptr<Node> node, uint32_t deviceId,
                                        Ptr<Packet> packet, uint8_t vcId);

    /**
     * \brief Log device queue usage statistics
     * \param loggingEnabled Whether logging is enabled
     * \param queue Main queue to log
     * \param queueManager Queue manager for VC queues
     * \param cbfcManager CBFC manager for configuration
     * \param numVcs Number of virtual channels
     * \param vcQueueMaxBytes Maximum VC queue size in bytes
     * \param processingQueueBytes Current processing queue bytes
     * \param processingQueueMaxBytes Maximum processing queue size
     * \param nodeId Node ID for logging
     * \param deviceId Device ID for logging
     */
    
        /**
     * \brief Process processing queue statistics (event-driven)
     * \param processingQueueBytes Current processing queue bytes
     * \param processingQueueMaxBytes Maximum processing queue size
     * \param nodeId Node ID for logging
     * \param deviceId Device ID for logging
     */
    static void ProcessProcessingQueueStats(uint32_t processingQueueBytes,
                                           uint32_t processingQueueMaxBytes,
                                           uint32_t nodeId,
                                           uint32_t deviceId);

    /**
     * \brief Process main queue statistics (event-driven)
     * \param queue Main queue pointer
     * \param nodeId Node ID for logging
     * \param deviceId Device ID for logging
     */
    static void ProcessMainQueueStats(Ptr<Queue<Packet>> queue,
                                     uint32_t nodeId,
                                     uint32_t deviceId);

    /**
     * \brief Process VC queue statistics (event-driven)
     * \param queueManager Queue manager pointer
     * \param cbfcManager CBFC manager pointer
     * \param numVcs Number of virtual channels
     * \param vcQueueMaxBytes Maximum VC queue size in bytes
     * \param nodeId Node ID for logging
     * \param deviceId Device ID for logging
     */
    static void ProcessVCQueueStats(Ptr<SueQueueManager> queueManager,
                                   Ptr<CbfcManager> cbfcManager,
                                   uint8_t numVcs,
                                   uint32_t vcQueueMaxBytes,
                                   uint32_t nodeId,
                                   uint32_t deviceId);

    /**
     * \brief Process packet drop statistics (event-driven)
     * \param droppedPacket The dropped packet
     * \param nodeId Node ID for logging (base-1)
     * \param deviceId Device ID for logging (base-0)
     * \param dropReason Reason for packet drop
     */
    static void ProcessPacketDropStats(Ptr<const Packet> droppedPacket,
                                      uint32_t nodeId,
                                      uint32_t deviceId,
                                      const std::string& dropReason);

    /**
     * \brief Process credit status change statistics (event-driven)
     * \param targetMac Target MAC address
     * \param vcId Virtual channel ID
     * \param credits Current credit count
     * \param nodeId Node ID for logging
     * \param deviceId Device ID for logging
     */
    static void ProcessCreditChangeStats(Mac48Address targetMac,
                                       uint8_t vcId,
                                       uint32_t credits,
                                       uint32_t nodeId,
                                       uint32_t deviceId);
};

/**
 * \brief SUE configuration utility class
 */
class SueConfigUtils {
public:
    /**
     * \brief Reconfigure CBFC manager with new queue size
     * \param cbfcManager CBFC manager to reconfigure
     * \param numVcs Number of virtual channels
     * \param initialCredits Initial credits per VC
     * \param enableLinkCBFC Whether link CBFC is enabled
     * \param creditBatchSize Credit batch size
     */
    static void ReconfigureCbfcWithQueueSize(Ptr<CbfcManager> cbfcManager,
                                           uint8_t numVcs,
                                           uint32_t initialCredits,
                                           bool enableLinkCBFC,
                                           uint32_t creditBatchSize);

    /**
     * \brief Log device queue usage information
     * \param loggingEnabled Whether logging is enabled
     * \param queue Main queue to log
     * \param queueManager Queue manager for VC queues
     * \param cbfcManager CBFC manager for configuration
     * \param numVcs Number of virtual channels
     * \param vcQueueMaxBytes Maximum VC queue size in bytes
     * \param processingQueueBytes Current processing queue bytes
     * \param processingQueueMaxBytes Maximum processing queue size
     * \param nodeId Node ID for logging
     * \param deviceId Device ID for logging
     */
    };

/**
 * \brief Packet analysis utility class
 */
class SuePacketUtils {
public:
    // Protocol constants for CBFC and LLR
    static const uint16_t PROT_CBFC_UPDATE;
    static const uint16_t ACK_REV;  //!< LLR ACK protocol number
    static const uint16_t NACK_REV; //!< LLR NACK protocol number

    /**
     * \brief Extract PPP protocol information from packet
     * \param packet Packet to analyze
     * \param protocol Output parameter for extracted protocol number
     * \return True if extraction was successful
     */
    static bool ExtractPppProtocol(Ptr<Packet> packet, uint16_t& protocol);

    /**
     * \brief Extract IP address information from packet
     * \param packet Packet to analyze
     * \param srcIp Output parameter for source IP address
     * \param dstIp Output parameter for destination IP address
     * \return True if extraction was successful
     */
    static bool ExtractIpInfo(Ptr<Packet> packet, Ipv4Address& srcIp, Ipv4Address& dstIp);

    /**
     * \brief Check if packet is internal packet
     * \param packet Packet to check
     * \return True if packet is internal
     */
    static bool IsInternalPacket(Ptr<Packet> packet);

    /**
     * \brief Extract VC ID from packet (supports both CBFC and data packets)
     * \param packet Packet to analyze
     * \return VC ID, returns 0 if extraction fails
     */
    static uint8_t ExtractVcIdFromPacket(Ptr<const Packet> packet);

    /**
     * \brief Extract destination IP address from packet
     * \param packet Packet to analyze
     * \return Destination IP address, returns Ipv4Address() if extraction fails
     */
    static Ipv4Address ExtractDestIpFromPacket(Ptr<Packet> packet);

    /**
     * \brief Extract and optionally modify source MAC address from packet
     * \param packet Packet to process (will be modified)
     * \param modifyHeader Whether to modify the source MAC in the packet
     * \param newSourceMac New source MAC address to set if modifyHeader is true
     * \return Original source MAC address
     */
    static Mac48Address ExtractSourceMac(Ptr<Packet> packet, bool modifyHeader = false,
                                         Mac48Address newSourceMac = Mac48Address());

    /**
     * \brief Convert PPP protocol number to Ethernet protocol number
     * \param proto PPP protocol number
     * \return Ethernet protocol number
     */
    static uint16_t PppToEther(uint16_t proto);

    /**
     * \brief Convert Ethernet protocol number to PPP protocol number
     * \param proto Ethernet protocol number
     * \return PPP protocol number
     */
    static uint16_t EtherToPpp(uint16_t proto);

    /**
     * \brief Set global IP-MAC mapping table
     * \param map IP to MAC address mapping
     */
    static void SetGlobalIpMacMap(const std::map<Ipv4Address, Mac48Address>& map);

    /**
     * \brief Get MAC address for IP address
     * \param ip IP address to lookup
     * \return MAC address, returns broadcast address if not found
     */
    static Mac48Address GetMacForIp(Ipv4Address ip);

    /**
     * \brief Add Ethernet header to packet
     * \param packet Packet to modify
     * \param destMac Destination MAC address
     * \param srcMac Source MAC address
     */
    static void AddEthernetHeader(Ptr<Packet> packet, Mac48Address destMac, Mac48Address srcMac);

private:
    // Static member for IP-MAC mapping
    static std::map<Ipv4Address, Mac48Address> s_ipToMacMap;
};

} // namespace ns3

#endif /* SUE_UTILS_H */