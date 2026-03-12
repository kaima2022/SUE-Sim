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

#include "sue-utils.h"
#include "ns3/log.h"
#include "sue-cbfc-header.h"
#include "sue-header.h"
#include "sue-ppp-header.h"
#include "sue-tag.h"
#include "ns3/ipv4-header.h"
#include "ns3/udp-header.h"
#include "ns3/ethernet-header.h"
#include "performance-logger.h"
#include "sue-cbfc.h"
#include "sue-queue-manager.h"
#include <stdexcept>

namespace ns3 {

// Define static constants
const uint16_t SuePacketUtils::PROT_CBFC_UPDATE = 0xCBFC;
const uint16_t SuePacketUtils::PROT_CBFC_SYNC = 0xCBFD;
const uint16_t SuePacketUtils::ACK_REV = 0x1111;
const uint16_t SuePacketUtils::NACK_REV = 0x2222;

NS_LOG_COMPONENT_DEFINE("SueUtils");

// Static member definitions
std::map<Ipv4Address, Mac48Address> SuePacketUtils::s_ipToMacMap;

/******************************************************************************/
// SueStringUtils
/******************************************************************************/

DataRate
SueStringUtils::ParseDataRateString(const std::string& rateStr)
{
    NS_LOG_FUNCTION("ParseDataRateString" << rateStr);

    if (rateStr.empty()) {
        NS_LOG_WARN("Empty rate string provided");
        return DataRate(0);
    }

    try {
        if (rateStr.find("Gbps") != std::string::npos) {
            size_t pos = rateStr.find("Gbps");
            std::string number = rateStr.substr(0, pos);
            double value = SafeStringToDouble(number);
            uint64_t bps = static_cast<uint64_t>(value * 1000000000); // Convert Gbps to bps
            DataRate result(bps);
            NS_LOG_INFO("Parsed Gbps rate: " << rateStr << " -> " << result.GetBitRate() << " bps");
            return result;
        }
        else if (rateStr.find("Mbps") != std::string::npos) {
            size_t pos = rateStr.find("Mbps");
            std::string number = rateStr.substr(0, pos);
            double value = SafeStringToDouble(number);
            uint64_t bps = static_cast<uint64_t>(value * 1000000); // Convert Mbps to bps
            DataRate result(bps);
            NS_LOG_INFO("Parsed Mbps rate: " << rateStr << " -> " << result.GetBitRate() << " bps");
            return result;
        }
        else if (rateStr.find("Kbps") != std::string::npos) {
            size_t pos = rateStr.find("Kbps");
            std::string number = rateStr.substr(0, pos);
            double value = SafeStringToDouble(number);
            uint64_t bps = static_cast<uint64_t>(value * 1000); // Convert Kbps to bps
            DataRate result(bps);
            NS_LOG_INFO("Parsed Kbps rate: " << rateStr << " -> " << result.GetBitRate() << " bps");
            return result;
        }
        else if (rateStr.find("bps") != std::string::npos) {
            size_t pos = rateStr.find("bps");
            std::string number = rateStr.substr(0, pos);
            double value = SafeStringToDouble(number);
            uint64_t bps = static_cast<uint64_t>(value);
            DataRate result(bps);
            NS_LOG_INFO("Parsed bps rate: " << rateStr << " -> " << result.GetBitRate() << " bps");
            return result;
        }
        else {
            NS_LOG_WARN("Unknown rate format: " << rateStr);
            return DataRate(0);
        }
    }
    catch (const std::exception& e) {
        NS_LOG_ERROR("Exception parsing rate string '" << rateStr << "': " << e.what());
        return DataRate(0);
    }
}

Time
SueStringUtils::ParseTimeIntervalString(const std::string& timeStr)
{
    NS_LOG_FUNCTION("ParseTimeIntervalString" << timeStr);

    if (timeStr.empty()) {
        NS_LOG_WARN("Empty time string provided");
        return Seconds(-1.0); // Return invalid time
    }

    try {
        std::string processedTimeStr = timeStr;

        // Convert microseconds to nanoseconds for ns3 compatibility
        if (timeStr.find("us") != std::string::npos) {
            size_t pos = timeStr.find("us");
            std::string number = timeStr.substr(0, pos);
            double value = SafeStringToDouble(number);
            processedTimeStr = std::to_string(static_cast<uint64_t>(value * 1000)) + "ns";
            NS_LOG_INFO("Converted time: " << timeStr << " -> " << processedTimeStr);
        }

        Time result(processedTimeStr);
        NS_LOG_INFO("Parsed time interval: " << timeStr << " -> " << result.GetNanoSeconds() << " ns");
        return result;
    }
    catch (const std::exception& e) {
        NS_LOG_ERROR("Exception parsing time string '" << timeStr << "': " << e.what());
        return Seconds(-1.0); // Return invalid time
    }
}

double
SueStringUtils::SafeStringToDouble(const std::string& numStr, double defaultValue)
{
    NS_LOG_FUNCTION("SafeStringToDouble" << numStr << defaultValue);

    if (numStr.empty()) {
        NS_LOG_WARN("Empty number string, returning default value: " << defaultValue);
        return defaultValue;
    }

    try {
        double result = std::stod(numStr);
        NS_LOG_DEBUG("Converted string '" << numStr << "' to double: " << result);
        return result;
    }
    catch (const std::invalid_argument& e) {
        NS_LOG_WARN("Invalid number format: '" << numStr << "', returning default: " << defaultValue);
        return defaultValue;
    }
    catch (const std::out_of_range& e) {
        NS_LOG_WARN("Number out of range: '" << numStr << "', returning default: " << defaultValue);
        return defaultValue;
    }
}

/******************************************************************************/
// SueLogUtils
/******************************************************************************/

void
SueLogUtils::LogDeviceInfo(const std::string& operation,
                          Ptr<NetDevice> device,
                          const std::string& details)
{
    // Placeholder implementation
    NS_LOG_FUNCTION(operation << device << details);
}

void
SueLogUtils::LogTransmissionStats(const std::string& operation,
                                 uint32_t nodeId,
                                 uint32_t deviceId,
                                 uint32_t vcId,
                                 uint32_t sequence,
                                 const std::string& status)
{
    // Placeholder implementation
    NS_LOG_FUNCTION(operation << nodeId << deviceId << vcId << sequence << status);
}

/******************************************************************************/
// SuePacketUtils
/******************************************************************************/

uint8_t
SuePacketUtils::ExtractVcIdFromPacket(Ptr<const Packet> packet)
{
    NS_LOG_FUNCTION(packet);

    if (!packet)
    {
        NS_LOG_WARN("ExtractVcIdFromPacket: null packet");
        return 0;
    }

    Packet p = *packet;

    SuePppHeader ppp;
    const uint32_t pppSize = ppp.GetSerializedSize();
    const uint16_t pppProtoIpv4 = SuePacketUtils::EtherToPpp(0x0800);
    const uint16_t pppProtoIpv6 = SuePacketUtils::EtherToPpp(0x86DD);
    const uint16_t pppProtoCbfc = SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_UPDATE);
    const uint16_t pppProtoCbfcSync = SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_SYNC);
    const uint16_t pppProtoAck = SuePacketUtils::EtherToPpp(SuePacketUtils::ACK_REV);
    const uint16_t pppProtoNack = SuePacketUtils::EtherToPpp(SuePacketUtils::NACK_REV);

    bool hasValidPppHeader = false;
    uint16_t pppProto = 0;

    if (p.GetSize() >= pppSize)
    {
        p.PeekHeader(ppp);
        pppProto = ppp.GetProtocol();
        if (pppProto == pppProtoIpv4 || pppProto == pppProtoIpv6 ||
            pppProto == pppProtoCbfc || pppProto == pppProtoCbfcSync ||
            pppProto == pppProtoAck || pppProto == pppProtoNack)
        {
            hasValidPppHeader = true;
            p.RemoveHeader(ppp);
        }
    }

    // Control packets: PPP + CBFC header (+ Ethernet header), extract VC directly from CBFC header.
    if (hasValidPppHeader &&
        (pppProto == pppProtoCbfc || pppProto == pppProtoCbfcSync ||
         pppProto == pppProtoAck || pppProto == pppProtoNack))
    {
        SueCbfcHeader cbfcHeader;
        if (p.GetSize() < cbfcHeader.GetSerializedSize())
        {
            NS_LOG_WARN("ExtractVcIdFromPacket: control packet too small for CBFC header, size=" << p.GetSize());
            return 0;
        }
        p.RemoveHeader(cbfcHeader);
        return cbfcHeader.GetVcId();
    }

    // Data packet structure (with or without PPP):
    //   [PPP] + Ethernet + IPv4 + [UDP] + SueHeader
    EthernetHeader eth;
    if (p.GetSize() < eth.GetSerializedSize())
    {
        NS_LOG_WARN("ExtractVcIdFromPacket: packet too small for Ethernet header, size=" << p.GetSize());
        return 0;
    }
    p.RemoveHeader(eth);

    Ipv4Header ipv4;
    if (p.GetSize() < ipv4.GetSerializedSize())
    {
        NS_LOG_WARN("ExtractVcIdFromPacket: packet too small for IPv4 header, size=" << p.GetSize());
        return 0;
    }
    p.RemoveHeader(ipv4);

    // UDP is expected for normal traffic, but avoid crashing on malformed/control packets.
    if (ipv4.GetProtocol() == 17) // UDP
    {
        UdpHeader udp;
        if (p.GetSize() < udp.GetSerializedSize())
        {
            NS_LOG_WARN("ExtractVcIdFromPacket: packet too small for UDP header, size=" << p.GetSize());
            return 0;
        }
        p.RemoveHeader(udp);
    }

    SueHeader sueHeader;
    if (p.GetSize() < sueHeader.GetSerializedSize())
    {
        NS_LOG_WARN("ExtractVcIdFromPacket: packet too small for SueHeader, size=" << p.GetSize());
        return 0;
    }
    p.RemoveHeader(sueHeader);
    return sueHeader.GetVc();
}

Ipv4Address
SuePacketUtils::ExtractDestIpFromPacket(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(packet);

    Packet p = *packet;

    try {
        // Get destination address from IPv4 header
        Ipv4Header ipv4;
        p.RemoveHeader(ipv4);
        return ipv4.GetDestination();
    }
    catch (...) {
        NS_LOG_WARN("Failed to extract destination IP from packet");
        return Ipv4Address(); // Return invalid address
    }
}

Mac48Address
SuePacketUtils::ExtractSourceMac(Ptr<Packet> packet, bool modifyHeader,
                                   Mac48Address newSourceMac)
{
    NS_LOG_FUNCTION(packet << modifyHeader << newSourceMac);

    SuePppHeader ppp;
    EthernetHeader ethHeader;

    try {
        packet->RemoveHeader(ppp);
        packet->RemoveHeader(ethHeader);

        Mac48Address sourceMac = ethHeader.GetSource();

        if (modifyHeader) {
            ethHeader.SetSource(newSourceMac);
        }

        packet->AddHeader(ethHeader);
        packet->AddHeader(ppp);

        return sourceMac;
    }
    catch (...) {
        NS_LOG_WARN("Failed to extract source MAC from packet");
        return Mac48Address(); // Return invalid MAC
    }
}

bool
SuePacketUtils::ExtractPppProtocol(Ptr<Packet> packet, uint16_t& protocol)
{
    NS_LOG_FUNCTION(packet << protocol);

    SuePppHeader ppp;

    try {
        packet->PeekHeader(ppp);
        protocol = ppp.GetProtocol();
        return true;
    }
    catch (...) {
        NS_LOG_DEBUG("Failed to extract PPP protocol from packet");
        return false;
    }
}

bool
SuePacketUtils::ExtractIpInfo(Ptr<Packet> packet, Ipv4Address& srcIp, Ipv4Address& dstIp)
{
    NS_LOG_FUNCTION(packet << srcIp << dstIp);

    Packet p = *packet;

    try {
        Ipv4Header ipv4;
        p.RemoveHeader(ipv4);
        srcIp = ipv4.GetSource();
        dstIp = ipv4.GetDestination();
        return true;
    }
    catch (...) {
        NS_LOG_DEBUG("Failed to extract IP info from packet");
        return false;
    }
}

bool
SuePacketUtils::IsInternalPacket(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(packet);

    uint16_t protocol;
    if (ExtractPppProtocol(packet, protocol)) {
        // Check if it's an internal protocol (CBFC update or internal data)
        return (protocol == 0x0800); // CBFC_UPDATE protocol
    }

    // For packets without PPP header, assume internal if they have SueHeader
    try {
        Packet p = *packet;
        EthernetHeader eth;
        p.RemoveHeader(eth);

        Ipv4Header ipv4;
        p.RemoveHeader(ipv4);

        UdpHeader udp;
        p.RemoveHeader(udp);

        SueHeader sueHeader;
        p.RemoveHeader(sueHeader);

        // If we can successfully extract SueHeader, it's an internal packet
        return true;
    }
    catch (...) {
        return false;
    }
}

// Protocol conversion functions
uint16_t
SuePacketUtils::PppToEther(uint16_t proto)
{
    NS_LOG_FUNCTION(proto);
    switch (proto)
    {
    case 0x0021:
        return 0x0800; // IPv4
    case 0x0057:
        return 0x86DD; // IPv6
    case 0x00FB:
        return PROT_CBFC_UPDATE; // CBFC Update
    case 0x00FC:
        return PROT_CBFC_SYNC; // CBFC Credit Sync
    case 0x1111:
        return ACK_REV; // LLR ACK
    case 0x2222:
        return NACK_REV; // LLR NACK
    default:
        NS_ASSERT_MSG(false, "PPP Protocol number not defined!");
    }
    return 0;
}

uint16_t
SuePacketUtils::EtherToPpp(uint16_t proto)
{
    NS_LOG_FUNCTION(proto);
    switch (proto)
    {
    case 0x0800:
        return 0x0021; // IPv4
    case 0x86DD:
        return 0x0057; // IPv6
    case PROT_CBFC_UPDATE:
        return 0x00FB; // CBFC Update
    case PROT_CBFC_SYNC:
        return 0x00FC; // CBFC Credit Sync
    case ACK_REV:
        return 0x1111; // LLR ACK
    case NACK_REV:
        return 0x2222; // LLR NACK
    default:
        NS_ASSERT_MSG(false, "PPP Protocol number not defined!");
    }
    return 0;
}

// IP-MAC mapping functions
void
SuePacketUtils::SetGlobalIpMacMap(const std::map<Ipv4Address, Mac48Address>& map)
{
    s_ipToMacMap = map;
    NS_LOG_INFO("SetGlobalIpMacMap - added " << map.size() << " IP-MAC entries");
}

Mac48Address
SuePacketUtils::GetMacForIp(Ipv4Address ip)
{
    auto it = s_ipToMacMap.find(ip);
    if (it != s_ipToMacMap.end())
    {
        return it->second;
    }

    NS_LOG_WARN("GetMacForIp - could not find MAC for IP: " << ip << ", returning broadcast");
    return Mac48Address::GetBroadcast();
}

void
SuePacketUtils::AddEthernetHeader(Ptr<Packet> packet, Mac48Address destMac, Mac48Address srcMac)
{
    EthernetHeader ethHeader;
    ethHeader.SetSource(srcMac);
    ethHeader.SetDestination(destMac);
    ethHeader.SetLengthType(0x0800); // IPv4
    packet->AddHeader(ethHeader);
}

/******************************************************************************/
// SueConfigUtils
/******************************************************************************/

void
SueConfigUtils::ReconfigureCbfcWithQueueSize(Ptr<CbfcManager> cbfcManager,
                                            uint8_t numVcs,
                                            uint32_t initialCredits,
                                            bool enableLinkCBFC,
                                            uint32_t creditBatchSize)
{
    NS_LOG_FUNCTION(cbfcManager << static_cast<uint32_t>(numVcs)
                    << initialCredits << enableLinkCBFC << creditBatchSize);

    if (cbfcManager && cbfcManager->IsInitialized())
    {
        cbfcManager->Configure(numVcs, initialCredits, enableLinkCBFC, creditBatchSize);
        NS_LOG_INFO("CBFC manager reconfigured with new queue size parameters");
    }
    else
    {
        NS_LOG_WARN("CBFC manager not initialized, skipping reconfiguration");
    }
}


/******************************************************************************/
// SueStatsUtils
/******************************************************************************/

void
SueStatsUtils::ProcessSentPacketStats(Ptr<Packet> packet,
                                     uint32_t nodeId, uint32_t deviceId)
{
    NS_LOG_FUNCTION(packet << nodeId << deviceId);

    SuePppHeader ppp;
    packet->PeekHeader(ppp);

    // Control packets (CBFC credit / LLR ACK / LLR NACK) are not counted in VC statistics.
    if (ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_UPDATE) ||
        ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_SYNC) ||
        ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::ACK_REV) ||
        ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::NACK_REV))
    {
        NS_LOG_DEBUG("Control packet sent, not counting in VC statistics");
        return;
    }

    // Extract VC ID from packet (data packets only)
    uint8_t vcId = SuePacketUtils::ExtractVcIdFromPacket(packet);

    // === EVENT-DRIVEN STATISTICS ===
    // For fine-grained testing, log packet immediately when sent
    // Use actual node and device IDs passed from caller

    int64_t timestampNs = Simulator::Now().GetNanoSeconds();
    uint32_t packetSizeBits = packet->GetSize() * 8;

    // Use PerformanceLogger with correct node and device context
    PerformanceLogger::GetInstance().LogPacketTx(
        timestampNs, nodeId, deviceId, vcId, "Tx", packetSizeBits);

    NS_LOG_DEBUG("Data packet sent on VC " << static_cast<uint32_t>(vcId)
                << ", size: " << packet->GetSize() << " bytes");
}

void
SueStatsUtils::ProcessReceivedPacketStats(Ptr<Packet> packet,
                                        uint32_t nodeId, uint32_t deviceId)
{
    NS_LOG_FUNCTION(packet << nodeId << deviceId);

    SuePppHeader ppp;
    packet->PeekHeader(ppp);

    // Control packets (CBFC credit / LLR ACK / LLR NACK) are not counted in VC statistics.
    if (ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_UPDATE) ||
        ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_SYNC) ||
        ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::ACK_REV) ||
        ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::NACK_REV))
    {
        NS_LOG_DEBUG("Control packet received, not counting in VC statistics");
        return;
    }

    // Extract VC ID from packet (data packets only)
    uint8_t vcId = SuePacketUtils::ExtractVcIdFromPacket(packet);

    // === EVENT-DRIVEN STATISTICS ===
    // For fine-grained testing, log packet immediately when received
    // Use actual node and device IDs passed from caller

    int64_t timestampNs = Simulator::Now().GetNanoSeconds();
    uint32_t packetSizeBits = packet->GetSize() * 8;

    // Use PerformanceLogger with correct node and device context
    PerformanceLogger::GetInstance().LogPacketRx(
        timestampNs, nodeId, deviceId, vcId, "Rx", packetSizeBits);

    NS_LOG_DEBUG("Data packet received on VC " << static_cast<uint32_t>(vcId)
                << ", size: " << packet->GetSize() << " bytes");
}



void
SueStatsUtils::ProcessProcessingQueueStats(uint32_t processingQueueBytes,
                                           uint32_t processingQueueMaxBytes,
                                           uint32_t nodeId,
                                           uint32_t deviceId)
{
    NS_LOG_FUNCTION(processingQueueBytes << processingQueueMaxBytes << nodeId << deviceId);

    uint64_t timeNs = Simulator::Now().GetNanoSeconds();

    // Log to PerformanceLogger immediately (event-driven)
    PerformanceLogger::GetInstance().LogProcessingQueueUsage(
        timeNs, nodeId, deviceId, processingQueueBytes, processingQueueMaxBytes);
}

void
SueStatsUtils::ProcessMainQueueStats(Ptr<Queue<Packet>> queue,
                                   uint32_t nodeId,
                                   uint32_t deviceId)
{
    NS_LOG_FUNCTION(queue << nodeId << deviceId);

    uint64_t timeNs = Simulator::Now().GetNanoSeconds();

    // Log main queue usage only
    uint32_t currentSize = 0;
    uint32_t maxSize = 0;
    if (queue) {
        currentSize = queue->GetNBytes();
        maxSize = queue->GetMaxSize().GetValue();
    }

    // Log to PerformanceLogger immediately (event-driven) - main queue only
    PerformanceLogger::GetInstance().LogMainQueueUsage(timeNs, nodeId, deviceId, currentSize, maxSize);
}

void
SueStatsUtils::ProcessVCQueueStats(Ptr<SueQueueManager> queueManager,
                                  Ptr<CbfcManager> cbfcManager,
                                  uint8_t numVcs,
                                  uint32_t vcQueueMaxBytes,
                                  uint32_t nodeId,
                                  uint32_t deviceId)
{
    NS_LOG_FUNCTION(queueManager << cbfcManager << nodeId << deviceId);

    uint64_t timeNs = Simulator::Now().GetNanoSeconds();

    // Log each VC queue usage separately
    if (cbfcManager && queueManager) {
        for (uint8_t vcId = 0; vcId < numVcs; ++vcId) {
            uint32_t currentSize = queueManager->GetVcQueueBytes(vcId);
            uint32_t maxSize = vcQueueMaxBytes;

            // Log to PerformanceLogger immediately (event-driven) - individual VC queue
            PerformanceLogger::GetInstance().LogVCQueueUsage(timeNs, nodeId, deviceId, vcId, currentSize, maxSize);
        }
    }
}

void
SueStatsUtils::ProcessPacketDropStats(Ptr<const Packet> droppedPacket,
                                      uint32_t nodeId,
                                      uint32_t deviceId,
                                      const std::string& dropReason)
{
    NS_LOG_FUNCTION(droppedPacket << nodeId << deviceId << dropReason);

    // Extract VC ID from packet for event-driven statistics
    uint8_t vcId = 0;
    Ptr<Packet> packet = droppedPacket->Copy();

    // Try to extract VC ID from packet
    if (packet->GetSize() > 0) {
        SuePppHeader ppp;
        if (packet->PeekHeader(ppp)) {
            vcId = SuePacketUtils::ExtractVcIdFromPacket(packet);
        }
    }

    // Log event-driven packet drop statistics
    uint64_t timeNs = Simulator::Now().GetNanoSeconds();
    uint32_t packetSize = droppedPacket ? droppedPacket->GetSize() : 0;

    PerformanceLogger::GetInstance().LogPacketDrop(timeNs, nodeId, deviceId, vcId, dropReason, packetSize);
}


void
SueStatsUtils::ProcessCreditChangeStats(Mac48Address targetMac,
                                       uint8_t vcId,
                                       uint32_t credits,
                                       uint32_t nodeId,
                                       uint32_t deviceId)
{
  NS_LOG_FUNCTION (targetMac << static_cast<uint32_t> (vcId) << credits << nodeId << deviceId);

  // Get current time
  Time currentTime = Simulator::Now();
  int64_t nanoseconds = currentTime.GetNanoSeconds();

  // Convert Mac48Address to string format
  std::ostringstream macStream;
  macStream << targetMac;
  std::string macStr = macStream.str();

  // Log credit status change (event-driven)
  PerformanceLogger::GetInstance().LogCreditStat(
    nanoseconds, nodeId, deviceId,
    vcId, "Credits", credits, macStr);
}

void
SueStatsUtils::ProcessCreditReceptionStats(Mac48Address sourceMac,
                                           uint8_t vcId,
                                           uint32_t receivedCredits,
                                           uint32_t nodeId,
                                           uint32_t deviceId)
{
  NS_LOG_FUNCTION (sourceMac << static_cast<uint32_t> (vcId) << receivedCredits << nodeId << deviceId);

  // Get current time
  Time currentTime = Simulator::Now();
  int64_t nanoseconds = currentTime.GetNanoSeconds();

  // Convert Mac48Address to string format
  std::ostringstream macStream;
  macStream << sourceMac;
  std::string macStr = macStream.str();

  // Log credit reception event (event-driven)
  PerformanceLogger::GetInstance().LogCreditStat(
    nanoseconds, nodeId, deviceId,
    vcId, "CreditReceived", receivedCredits, macStr);
}

void
SueStatsUtils::ProcessCreditSendStats(Mac48Address targetMac,
                                      uint8_t vcId,
                                      uint32_t sentCredits,
                                      uint32_t nodeId,
                                      uint32_t deviceId)
{
  NS_LOG_FUNCTION(targetMac << static_cast<uint32_t>(vcId) << sentCredits << nodeId << deviceId);

  // Get current time
  Time currentTime = Simulator::Now();
  int64_t nanoseconds = currentTime.GetNanoSeconds();

  // Convert Mac48Address to string format
  std::ostringstream macStream;
  macStream << targetMac;
  std::string macStr = macStream.str();

  // Log credit send event (event-driven)
  PerformanceLogger::GetInstance().LogCreditStat(
    nanoseconds, nodeId, deviceId,
    vcId, "CreditSent", sentCredits, macStr);
}

void
SueStatsUtils::ProcessDestinationQueueStats(uint32_t xpuId,
                                           uint32_t sueId,
                                           uint32_t destXpuId,
                                           uint8_t vcId,
                                           uint32_t currentBytes,
                                           uint32_t maxBytes)
{
    NS_LOG_FUNCTION(xpuId << sueId << destXpuId << static_cast<uint32_t>(vcId)
                      << currentBytes << maxBytes);

    // Get current time
    Time currentTime = Simulator::Now();
    uint64_t timeNs = currentTime.GetNanoSeconds();

    // Log destination queue usage (event-driven)
    PerformanceLogger::GetInstance().LogDestinationQueueUsage(
        timeNs, xpuId, sueId, destXpuId, vcId, currentBytes, maxBytes);
}

void
SueStatsUtils::ProcessPackDelayStats(uint32_t xpuId, uint32_t sueId,
                                     uint32_t destXpuId, uint8_t vcId, int64_t waitTimeNs)
{
    NS_LOG_FUNCTION(xpuId << sueId << destXpuId << static_cast<uint32_t>(vcId) << waitTimeNs);

    // Log packing delay (event-driven)
    PerformanceLogger::GetInstance().LogPackDelay(xpuId, sueId, destXpuId, vcId, waitTimeNs);
}

void
SueStatsUtils::ProcessPackNumStats(uint32_t xpuId, uint32_t sueId,
                                   uint32_t destXpuId, uint8_t vcId, uint32_t packNum)
{
    NS_LOG_FUNCTION(xpuId << sueId << destXpuId << static_cast<uint32_t>(vcId) << packNum);

    // Log packing number (event-driven)
    PerformanceLogger::GetInstance().LogPackNum(xpuId, sueId, destXpuId, vcId, packNum);
}

void
SueStatsUtils::ProcessAppLayerTxStats(uint32_t nodeId, uint8_t vcId, uint32_t packetSize)
{
    NS_LOG_FUNCTION(nodeId << static_cast<uint32_t>(vcId) << packetSize);

    // Get current time in nanoseconds
    uint64_t timeNs = Simulator::Now().GetNanoSeconds();

    // Log application layer transmission (event-driven)
    PerformanceLogger::GetInstance().LogAppLayerTx(timeNs, nodeId, vcId, packetSize);
}

void
SueStatsUtils::ProcessVcQueueDelayStats(Ptr<Packet> packet, uint32_t nodeId, uint32_t deviceId)
{
    NS_LOG_FUNCTION(packet << nodeId << deviceId);

    // Calculate and log VC queue delay
    Time delay;
    uint32_t tagNodeId, tagDeviceId;
    uint8_t tagVcId;
    if (SueTag::ExtractVcQueueDelay(packet, Simulator::Now(), delay, tagNodeId, tagDeviceId, tagVcId))
    {
        PerformanceLogger::GetInstance().LogXpuDelay(
            Simulator::Now().GetNanoSeconds(),
            nodeId,
            deviceId,
            delay.GetNanoSeconds(),
            "VC_Queue");
    }
}

void
SueStatsUtils::ProcessProcessingQueueDelayStats(Ptr<Packet> packet, uint32_t nodeId, uint32_t deviceId)
{
    NS_LOG_FUNCTION(packet << nodeId << deviceId);

    // Calculate and log processing queue delay
    Time delay;
    if (SueTag::ExtractProcessingQueueDelay(packet, Simulator::Now(), delay))
    {
        PerformanceLogger::GetInstance().LogXpuDelay(
            Simulator::Now().GetNanoSeconds(),
            nodeId,
            deviceId,
            delay.GetNanoSeconds(),
            "Processing_Queue");
    }
}

} // namespace ns3
