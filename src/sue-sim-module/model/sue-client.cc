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

#include "sue-client.h"
#include "ns3/sue-header.h"
#include "ns3/core-module.h"
#include "ns3/log.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/simulator.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/random-variable-stream.h"
#include "ns3/performance-logger.h"
#include "sue-utils.h"


namespace ns3 {

std::map<Ipv4Address, Mac48Address> SueClient::s_ipToMacMap;
NS_LOG_COMPONENT_DEFINE("SueClientApplication");
NS_OBJECT_ENSURE_REGISTERED(SueClient);

TypeId SueClient::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::SueClient")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<SueClient>()
        .AddAttribute("TransactionSize", "Size of a single transaction in bytes.",
                      UintegerValue(128), MakeUintegerAccessor(&SueClient::m_transactionSize), 
                      MakeUintegerChecker<uint32_t>())
        // .AddAttribute("RTO", "Retransmission Timeout.",
        //               TimeValue(MilliSeconds(200)), MakeTimeAccessor(&SueClient::m_rto), 
        //               MakeTimeChecker())
        .AddAttribute("MaxBurstSize", "Maximum packed burst size in bytes",
                      UintegerValue(2048), 
                      MakeUintegerAccessor(&SueClient::m_maxBurstSize),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("SchedulingInterval", "Time between queue processing cycles",
                      TimeValue(NanoSeconds(10)),  // Scheduling interval 10ns
                      MakeTimeAccessor(&SueClient::m_schedulingInterval),
                      MakeTimeChecker())
        .AddAttribute("vcNum","Number of VCs, should match link layer settings",
                      UintegerValue(4),
                      MakeUintegerAccessor(&SueClient::m_vcNum),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("DestQueueMaxBytes", "Destination queue maximum size in bytes (default: 30KB)",
                      UintegerValue(30 * 1024),
                      MakeUintegerAccessor(&SueClient::m_destQueueMaxBytes),
                      MakeUintegerChecker<uint32_t>())
        .AddAttribute("PackingDelayPerPacket",
                      "Packing Delay Per Packet",
                      TimeValue(NanoSeconds(3)),
                      MakeTimeAccessor(&SueClient::m_packingDelayPerPacket),
                      MakeTimeChecker())
        .AddAttribute("ClientStatInterval",
                      "Client Statistic Interval",
                      StringValue("10us"), // Default: 10 microseconds
                      MakeStringAccessor(&SueClient::m_clientStatIntervalString),
                      MakeStringChecker());
    return tid;
}

SueClient::SueClient()
    : m_currentQueueIt(m_destQueues.end()),  // Initialize as end
      m_maxBurstSize(2048),
      m_psn(0),
      m_packetsSent(0),
      m_acksReceived(0),
      m_nodeId(0),
      m_xpuId(0),
      m_lastDeviceIndex(0),
      m_waitingStartTime(Time(0)),
      m_vcNum(4),
      m_destQueueMaxBytes(30 * 1024), // Default 30KB
      m_totalBytesSent(0),
      m_lastStatTime(Seconds(0)),
      m_clientStatInterval(MicroSeconds(10)),
          m_clientStatIntervalString("10us"),
      m_packingDelayPerPacket("3ns"),
      m_XpuDropCounts(0),
      m_loggingEnabled(true),
      m_deviceId(0),
      m_sueId(0),
      m_portsPerSue(1),
      m_lastUsedDeviceIndex(0)
{
    m_rand = CreateObject<UniformRandomVariable>();
}

SueClient::~SueClient() {}

void SueClient::DoDispose(void) { 
    Application::DoDispose(); 
}


void SueClient::SetXpuInfo(uint32_t xpuId, uint32_t deviceId) {
    m_xpuId = xpuId;
    m_deviceId = deviceId;
}

// Set SUE managed devices
void SueClient::SetManagedDevices(const std::vector<Ptr<PointToPointSueNetDevice>>& managedDevices) {
    m_managedDevices = managedDevices;
    m_portsPerSue = managedDevices.size();
    m_lastUsedDeviceIndex = 0;

    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                << "] Configured to manage " << m_portsPerSue << " devices");
}

// Set SUE identifier
void SueClient::SetSueId(uint32_t sueId) {
    m_sueId = sueId;
}

void SueClient::StartApplication(void) {
    m_nodeId = GetNode()->GetId();

    // Convert client stat interval string to Time for compatibility
    if (!m_clientStatIntervalString.empty())
    {
        try
        {
            // Convert "us" to ns3 compatible format
            std::string timeStr = m_clientStatIntervalString;
            if (timeStr.find("us") != std::string::npos)
            {
                // Convert microseconds to nanoseconds for ns3 compatibility
                size_t pos = timeStr.find("us");
                std::string number = timeStr.substr(0, pos);
                try
                {
                    double value = std::stod(number);
                    timeStr = std::to_string(static_cast<uint64_t>(value * 1000)) + "ns";
                }
                catch (...)
                {
                    timeStr = "10000ns"; // Default 10 microseconds in nanoseconds
                }
            }
            m_clientStatInterval = Time(timeStr);
            NS_LOG_INFO("Client stat interval set to: " << m_clientStatIntervalString << " (" << m_clientStatInterval.GetNanoSeconds() << " ns)");
        }
        catch (const std::exception& e)
        {
            NS_LOG_WARN("Invalid client stat interval format: " << m_clientStatIntervalString << ", using default value");
            m_clientStatInterval = MilliSeconds(10);
        }
    }

    Ptr<Node> node = GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    
    // Fix: Only bind SUE managed devices, not all PointToPointSueNetDevice devices
    if (m_managedDevices.empty()) {
        // Compatible with legacy mode: If no managed devices set, use original logic
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << "] No managed devices set, using legacy mode");

        for (uint32_t i = 0; i < node->GetNDevices(); ++i) {
            Ptr<NetDevice> dev = node->GetDevice(i);
            Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice>(dev);
            if (p2pDev) {
                m_p2pDevices.push_back(p2pDev); // Store device pointer

                Ptr<Socket> newSocket = Socket::CreateSocket(node, tid);
                newSocket->BindToNetDevice(p2pDev);

                newSocket->SetRecvCallback(MakeCallback(&SueClient::HandleRead, this));

                m_deviceSockets[p2pDev] = newSocket;
                NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << "] Created and bound socket to specific Device " << p2pDev->GetIfIndex());
            }
        }
    } else {
        // New mode: Only bind SUE-managed devices
        NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                    << "] Binding sockets to " << m_managedDevices.size() << " managed devices");

        for (size_t i = 0; i < m_managedDevices.size(); ++i) {
            Ptr<PointToPointSueNetDevice> p2pDev = m_managedDevices[i];
            if (p2pDev) {
                // Verify device belongs to current node
                if (p2pDev->GetNode() != node) {
                    NS_LOG_WARN(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                                << "] Device " << p2pDev->GetIfIndex() << " does not belong to this node, skipping");
                    continue;
                }

                m_p2pDevices.push_back(p2pDev);

                Ptr<Socket> newSocket = Socket::CreateSocket(node, tid);
                newSocket->BindToNetDevice(p2pDev);

                // Get IP address corresponding to device for binding
                Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
                if (!ipv4) {
                    NS_LOG_ERROR("Node has no IPv4 stack");
                    continue;
                }

                int32_t interfaceIndex = ipv4->GetInterfaceForDevice(p2pDev);
                if (interfaceIndex == -1) {
                    NS_LOG_ERROR("Failed to get interface for device " << p2pDev->GetIfIndex());
                    continue;
                }

                Ipv4Address deviceIp;
                try {
                    Ipv4InterfaceAddress interfaceAddr = ipv4->GetAddress(interfaceIndex, 0);
                    deviceIp = interfaceAddr.GetLocal();
                    if (deviceIp == Ipv4Address::GetZero()) {
                        NS_LOG_ERROR("Invalid interface address for device " << p2pDev->GetIfIndex());
                        continue;
                    }
                } catch (...) {
                    NS_LOG_ERROR("Exception getting interface address for device " << p2pDev->GetIfIndex());
                    continue;
                }

                newSocket->SetRecvCallback(MakeCallback(&SueClient::HandleRead, this));

                m_deviceSockets[p2pDev] = newSocket;
                NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                            << "] Created and bound socket to Device " << p2pDev->GetIfIndex()
                            << " (IP: " << deviceIp << ", Port: auto-assigned)");
            }
        }
    }
    // Initialize statistics variables
    m_totalBytesSent = 0;
    m_lastStatTime = Simulator::Now();
    
    // Start periodic statistics - DISABLED by default for fine-grained mode
    // Uncomment to enable client-level statistics
    // if (m_loggingEnabled) {
    //     m_logClientStatisticsEvent = Simulator::Schedule(m_clientStatInterval, &SueClient::LogClientStatistics, this);
    // }
    
    ScheduleNextSend();

  
    // Note: Packing log files are now managed by PerformanceLogger, no need to create separately here
}

// void
// SueClient::ChangeDataRate(double newRatePercent) {
//     // Iterate through all applications on the node
//     for (uint32_t i = 0; i < node->GetNApplications(); ++i)
//     {
//         Ptr<Application> app = node->GetApplication(i);
//         Ptr<OnOffApplication> onoffApp = DynamicCast<OnOffApplication>(app);
//         if (onoffApp)
//         {
            
//             // NS_LOG_INFO("Found OnOffApplication "<< app->GetInstanceTypeId().GetName() <<" on node " << node->GetId());
//             Simulator::ScheduleNow(&SueClient::ChangeDataRate, onoffApp);
//             // onoffApp->SetAttribute("DataRate", StringValue("0.1bps"));
//             // NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << "] APP Pausing");
//         }
//     }

//     onoffapp->SetAttribute("DataRate", StringValue("0bps"));
//     // onoffapp->SetAttribute("PacketSize", UintegerValue(1));  // Explicitly set packet size to 0
//     // onoffapp->SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));     // Ensure no transmission
//     // onoffapp->SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Long off time
//     NS_LOG_INFO(Simulator::Now().GetSeconds() << "s APP Pausing");
// }

void SueClient::SetLoggingEnabled(bool enabled)
{
  NS_LOG_FUNCTION(this << enabled);
  m_loggingEnabled = enabled;
}

void SueClient::SetGlobalIpMacMap(const std::map<Ipv4Address, Mac48Address>& map){
    s_ipToMacMap = map;
}

void SueClient::LogClientStatistics() {
    // Check if logging is enabled
    if (!m_loggingEnabled) {
        NS_LOG_INFO("Logging disabled for XPU" << m_xpuId << " SUE" << m_sueId);
        return;
    }

    Time now = Simulator::Now();
    int64_t nanoseconds = now.GetNanoSeconds();

    // Calculate transmission rate within current statistics period (Mbps)
    double rate = (m_totalBytesSent * 8) / m_clientStatInterval.GetSeconds() / 1e6;

    // Only record device information managed by current SUEClient
    // Calculate relative device index within current SUE-managed device range
    uint32_t sueDeviceIndex = m_deviceId - (m_sueId * m_portsPerSue);
    uint32_t deviceIndex = m_sueId * m_portsPerSue + sueDeviceIndex;

    // Use PerformanceLogger to record application layer statistics - only record devices within management range
    PerformanceLogger::GetInstance().LogAppStat(nanoseconds, m_xpuId, deviceIndex, 0, rate);

    NS_LOG_INFO("Time " << now << "s XPU" << m_xpuId << " SUE" << m_sueId
                   << " AppTxRate: " << rate << " Mbps (Device " << deviceIndex << ")");

    // Packet loss statistics - only record devices within management range
    if(m_XpuDropCounts>0){
        PerformanceLogger::GetInstance().LogDropStat(nanoseconds, m_xpuId, deviceIndex, 0, "AppXpuSendDrop", m_XpuDropCounts);
    }

    // Packing statistics - uniformly write packing delay and quantity data
    // Only record packing information of devices managed by current SUEClient
    if (!m_packDelays.empty() || !m_packNumbers.empty()) {
        NS_LOG_INFO("Writing pack statistics for XPU" << m_xpuId << " SUE" << m_sueId
                    << " - " << m_packDelays.size() << " delay entries, "
                    << m_packNumbers.size() << " number entries (Managed devices: "
                    << m_portsPerSue << ")");

        // Write all packing delay data (already stored in nanoseconds)
        for (int64_t delayNs : m_packDelays) {
            PerformanceLogger::GetInstance().LogPackDelay(m_xpuId, delayNs);
        }

        // Write all packing quantity data
        for (uint32_t packNum : m_packNumbers) {
            PerformanceLogger::GetInstance().LogPackNum(m_xpuId, packNum);
        }
    }

    // Record destination queue utilization
    LogDestinationQueueUsage();

    // Reset counters and schedule next statistics
    m_totalBytesSent = 0;
    m_XpuDropCounts = 0;
    m_lastStatTime = Simulator::Now();

    // Reset packing data storage
    m_packDelays.clear();
    m_packNumbers.clear();

    // Force flush log files to ensure data is written to disk
    // PerformanceLogger will automatically manage file flushing

    if (m_loggingEnabled) {
        m_logClientStatisticsEvent = Simulator::Schedule(m_clientStatInterval, &SueClient::LogClientStatistics, this);
    }
}

    // Cancel all log events
void SueClient::CancelAllLogEvents() {
    NS_LOG_FUNCTION(this);
    // Disable logging first
    SetLoggingEnabled(false);

    // Cancel own log statistics events
    if (m_logClientStatisticsEvent.IsPending()) {
        Simulator::Cancel(m_logClientStatisticsEvent);
        NS_LOG_INFO("Cancelled client log statistics event");

        std::cout << Simulator::Now() << " Cancelled client log statistics event for XPU" << m_xpuId << std::endl;
    }

    // Disable statistics only for devices managed by this SUE
    for (size_t i = 0; i < m_managedDevices.size(); ++i) {
        Ptr<PointToPointSueNetDevice> p2pDev = m_managedDevices[i];
        if (p2pDev) {
            // Verify device belongs to current node before disabling
            if (p2pDev->GetNode() == GetNode()) {
                // Use Config::Set to disable logging for this specific device
                std::ostringstream configPath;
                configPath << "/NodeList/" << GetNode()->GetId()
                           << "/DeviceList/" << p2pDev->GetIfIndex()
                           << "/$ns3::PointToPointSueNetDevice/StatLoggingEnabled";
                Config::Set(configPath.str(), BooleanValue(false));

                NS_LOG_INFO("Disabled statistics for managed device " << p2pDev->GetIfIndex()
                           << " (Node " << GetNode()->GetId() << ")");
            }
        }
    // Also disable statistics for switch devices
    // Find all switch devices in the system
    for (uint32_t nodeId = 0; nodeId < NodeList::GetNNodes(); ++nodeId) {
            Ptr<Node> node = NodeList::GetNode(nodeId);
            for (uint32_t j = 0; j < node->GetNDevices(); ++j) {
                Ptr<NetDevice> dev = node->GetDevice(j);
                Ptr<PointToPointSueNetDevice> switchP2pDev =
                    DynamicCast<PointToPointSueNetDevice>(dev);

                if (switchP2pDev) {
                    // Try to convert address to Mac48Address
                    Mac48Address macAddr = Mac48Address::ConvertFrom(switchP2pDev->GetAddress());
                    if (switchP2pDev->IsSwitchDevice(macAddr)) {
                        std::ostringstream switchConfigPath;
                        switchConfigPath << "/NodeList/" << nodeId
                                            << "/DeviceList/" << switchP2pDev->GetIfIndex()
                                            << "/$ns3::PointToPointSueNetDevice/StatLoggingEnabled";
                        Config::Set(switchConfigPath.str(), BooleanValue(false));

                        NS_LOG_INFO("Disabled statistics for connected switch device "
                                    << switchP2pDev->GetIfIndex()
                                    << " (Switch Node " << nodeId << ")");
                    }
                }
            }
        }
    }

    std::cout << Simulator::Now() << " Disabled statistics for " << m_managedDevices.size()
              << " managed devices by XPU" << m_xpuId << " SUE" << m_sueId << std::endl;
}




void SueClient::AddTransaction(Ptr<Packet> transaction, uint32_t destXpuId) {
    // Extract SUE header information to get VC ID
    SueHeader sueHeader;
    transaction->PeekHeader(sueHeader);
    uint8_t vcId = sueHeader.GetVc();

    // Create new composite key: VC ID + destination XPU ID
    Destination key(destXpuId, vcId);
    auto& queueInfo = m_destQueues[key];

    // Check destination queue byte-level capacity limit
    uint32_t packetSize = transaction->GetSize();
    if (queueInfo.currentBurstSize + packetSize > m_destQueueMaxBytes) {
        // Log destination queue full drop (event-driven)
        Ptr<Packet> dropPacket = Create<Packet>(packetSize);
        SueStatsUtils::ProcessPacketDropStats(dropPacket, m_xpuId, 0, "DestQueueFull");
        NS_LOG_WARN(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << "] Destination queue for XPU" << destXpuId
                   << "-VC" << (uint32_t)vcId << " is full (" << queueInfo.currentBurstSize << " + " << packetSize << " > " << m_destQueueMaxBytes << " bytes)! Dropping transaction packet.");
        return; // Drop packet
    }

    // Record enqueue timestamp
    Time enqueueTime = Simulator::Now();
    queueInfo.queue.push(std::make_pair(enqueueTime, transaction));
    queueInfo.currentBurstSize += packetSize;

    // If current iterator is not set and queue is not empty, set iterator
    if (m_currentQueueIt == m_destQueues.end() && !m_destQueues.empty()) {
        m_currentQueueIt = m_destQueues.begin();
    }
}


void SueClient::ScheduleNextSend() {
    if (m_loggingEnabled != false){
        m_schedulerEvent = Simulator::Schedule(m_schedulingInterval, &SueClient::ScheduleNextSend, this);
    }

    if (m_destQueues.empty()) return;

    // Improved scheduling algorithm: Use waiting time priority scheduling instead of simple round-robin
    // Find non-empty queue with longest waiting time
    Time maxWaitTime = NanoSeconds(0);
    auto selectedQueueIt = m_destQueues.end();
    Time currentTime = Simulator::Now();
    
    for (auto it = m_destQueues.begin(); it != m_destQueues.end(); ++it) {
        if (!it->second.queue.empty()) {
            Time enqueueTime = it->second.queue.front().first;
            Time waitTime = currentTime - enqueueTime;
            
            if (waitTime > maxWaitTime) {
                maxWaitTime = waitTime;
                selectedQueueIt = it;
            }
        }
    }
    
    // If no non-empty queue found, return
    if (selectedQueueIt == m_destQueues.end()) {
        return;
    }
    
    // Record scheduling decision
    NS_LOG_DEBUG(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId 
                << "] Selected queue with max wait time: " 
                << maxWaitTime.GetNanoSeconds() << "ns");
    
    Destination dest = selectedQueueIt->first;
    auto& queueInfo = selectedQueueIt->second;

    // RateControl();

    if (!queueInfo.queue.empty()) {

        Ptr<PointToPointSueNetDevice> selectedDevice = nullptr;

        // 1. First perform packing, then select device based on packing result
        std::vector<Ptr<Packet>> packedPackets = Packing(dest);
        
        if (packedPackets.empty()) {
            // No packed packets available to send, continue waiting
            return;
        }
        
        // Note: Device selection will be done separately for each packet in the send loop
        // If waiting before, calculate and print waiting time
        if (!m_waitingStartTime.IsZero()) {
            Time waitingDuration = Simulator::Now() - m_waitingStartTime;
            NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << "] Resumed sending after waiting for " 
                        << waitingDuration.GetMicroSeconds() << " us.");
            m_waitingStartTime = Time(0); // Reset wait timer
        }

        // Select device and send for each packed packet individually
        Time processingDelay = m_packingDelayPerPacket;
        uint32_t targetXpu = dest.destXpuId;

        for (size_t i = 0; i < packedPackets.size(); ++i) {
            Ptr<Packet> packedPacket = packedPackets[i];
            uint32_t packetSize = packedPacket->GetSize();
            
            // Extract VC ID from packed packet
            SueHeader sueHeader;
            packedPacket->PeekHeader(sueHeader);
            uint8_t vcId = sueHeader.GetVc();
            
            // Select device for current packet
            Ptr<PointToPointSueNetDevice> currentDevice = nullptr;
            if (!m_managedDevices.empty()) {
                currentDevice = SelectDeviceByVcCapacity(packetSize, vcId);
                if (!currentDevice) {
                    // Current packet has no device with sufficient capacity, skip this packet
                    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId 
                                << "] No device has sufficient VC capacity for packet " << (i+1) 
                                << ", skipping...");
                    continue;
                }
            }

            // Generate remote address based on selected device and target XPU
            uint32_t selectedPort = currentDevice->GetIfIndex();
            std::ostringstream ipStream;
            ipStream << "10." << (targetXpu + 1) << "." << selectedPort << ".1";
            Ipv4Address remoteIp(ipStream.str().c_str());
            uint16_t remotePort = 8080 + (selectedPort-1);
            
            InetSocketAddress remoteAddr(remoteIp, remotePort);
            auto socketIt = m_deviceSockets.find(currentDevice);
            if (socketIt == m_deviceSockets.end()) {
                NS_LOG_WARN("No socket found for selected device");
                // Release reserved capacity
                auto queueManager = currentDevice->GetQueueManager();
                if (queueManager) {
                    queueManager->ReleaseVcCapacity(vcId, packetSize);
                }
                continue;
            }
            Ptr<Socket> sendingSocket = socketIt->second;

            uint32_t packSizeTemp = packedPacket->GetSize() - 8; // SUE 8 Bytes
            m_totalBytesSent += packSizeTemp;

            // Send log
            NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId
                        << " Device "<< currentDevice->GetIfIndex()
                        <<"] Sent packed packet " << (i+1) << "/" << packedPackets.size()
                        << " (" << packSizeTemp << " bytes) to [Xpu" << targetXpu
                        << "-Port" << selectedPort << "]");

            // Schedule send event with packing delay
            Time sendDelay = processingDelay * (i + 1);
            Simulator::Schedule(sendDelay, &SueClient::DoSendBurst, this,
                               packedPacket, sendingSocket, remoteAddr, dest, currentDevice);
        }
        // Note: Credit checking is now handled by LoadBalancer
        // SueClient no longer needs to check device credits
        // The LoadBalancer ensures that only SUEs with available credits receive transactions
    }

    // Switched from polling to wait-time priority scheduling
    // Note: m_currentQueueIt is no longer used for polling
}

void SueClient::DoSendBurst(Ptr<Packet> burstPacket, Ptr<Socket> sendingSocket, InetSocketAddress remoteAddr, const Destination& dest, Ptr<PointToPointSueNetDevice> device){
    // EthernetHeader will be added in link layer Send method
    // Link layer extracts destination IP from IPv4 header and queries corresponding MAC address

    // Validate socket state
    if (!ValidateSocketState(sendingSocket, "Before sending")) {
        NS_LOG_ERROR("Socket validation failed, aborting send");
        return;
    }

    // Detailed pre-send logging
    NS_LOG_DEBUG(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                 << "] Attempting to send " << burstPacket->GetSize()
                 << " bytes to XPU" << dest.destXpuId);

    // Validate target XPU is valid
    if (dest.destXpuId == 0xFFFFFFFF) { // Use invalid value to indicate unset
        NS_LOG_ERROR("Invalid destination XPU ID: " << dest.destXpuId);
        return;
    }

    // Calculate transaction count in packet and use directly for credit return
    uint32_t transactionCount = AnalyzeTransactionCount(burstPacket);
    NS_LOG_DEBUG("Packet contains " << transactionCount << " transactions, restoring " << transactionCount << " credits");

    // Extract VC ID for capacity management
    SueHeader sueHeader;
    burstPacket->PeekHeader(sueHeader);
    uint8_t vcId = sueHeader.GetVc();
    uint32_t packetSize = burstPacket->GetSize();

    int32_t result = sendingSocket->SendTo(burstPacket, 0, remoteAddr);

    if (result >= 0) {  // Send successful
        m_psn++;
        m_packetsSent++;
        NS_LOG_DEBUG(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                     << "] Successfully sent " << result << " bytes to XPU"
                     << dest.destXpuId << " (PSN: " << (m_psn-1) << ")");

        // Remove sent transactions from destination queue after successful send
        PopTransactionsFromQueue(dest, transactionCount);

        // Release reserved VC capacity
        if (device) {
            auto queueManager = device->GetQueueManager();
            if (queueManager) {
                queueManager->ReleaseVcCapacity(vcId, packetSize);
            }
        }

        NS_LOG_DEBUG("Successfully sent " << transactionCount << " transactions for SUE " << m_sueId);
    } else {
        // Also release reserved VC capacity on send failure
        if (device) {
            auto queueManager = device->GetQueueManager();
            if (queueManager) {
                queueManager->ReleaseVcCapacity(vcId, packetSize);
            }
        }
        // Detailed error handling
        std::string errorMsg;
        switch (result) {
            case -1:
                errorMsg = "Socket error (possibly not connected or invalid address)";
                break;
            default:
                errorMsg = "Unknown error code: " + std::to_string(result);
                break;
        }

        NS_LOG_ERROR(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                    << "] Send FAILED to XPU" << dest.destXpuId
                    << " - Error: " << errorMsg
                    << " (Packet size: " << burstPacket->GetSize() << " bytes)");

        // Log detailed network status information
        LogNetworkState(dest);

        // Improved error recovery: retry instead of direct exit
        NS_LOG_WARN("Send failed, will retry later instead of exiting");

        // TODO: Implement better error recovery mechanism
        // 1. Re-queue packets
        // 2. Adjust send rate
        // 3. Try alternative routes
    }
}

void SueClient::StopApplication(void) {
    // Cancel scheduler events
    if (m_schedulerEvent.IsPending()) {
        Simulator::Cancel(m_schedulerEvent);
    }

    
    
    // // Cancel all retransmission events
    // for (auto& event : m_retransmitEvents) {
    //     Simulator::Cancel(event.second);
    // }
    // m_retransmitEvents.clear();
    
    for (auto const& pair : m_deviceSockets) {
        Ptr<Socket> socket = pair.second;
        if (socket) {
            socket->Close();
        }
    }
    m_deviceSockets.clear();
    
    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << "-SUE" << m_sueId << "] Summary: Sent "
               << m_packetsSent << " packets");

    std::cout << Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << "-SUE" << m_sueId << "] Summary: Sent "
              << m_packetsSent << " packets" << std::endl;

    // Note: Packing log files are now managed by PerformanceLogger, no need to manually close
}

void SueClient::HandleRead(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() >= 8) {
            SueHeader responseHeader;
            packet->RemoveHeader(responseHeader);
            uint16_t ackedPsn = responseHeader.GetRpsn();
            NS_LOG_INFO("[Xpu" << m_xpuId << "] At " 
                      << Simulator::Now().GetSeconds() 
                      << "s received ACK for PSN " << ackedPsn);
            m_acksReceived++;          
            // // If this ACK is for the packet we're waiting for, cancel the corresponding retransmission timer
            // if (m_retransmitEvents.count(ackedPsn)) {
            //     Simulator::Cancel(m_retransmitEvents[ackedPsn]);
            //     m_retransmitEvents.erase(ackedPsn);
            //     m_inflightPackets.erase(ackedPsn);
            //     m_acksReceived++;
            // }
        }
    }
}

// void SueClient::Retransmit(uint16_t psnToRetransmit) {
//     // Check if this packet is still in flight
//     if (m_inflightPackets.count(psnToRetransmit)) {
//         Ptr<Packet> packetToResend = m_inflightPackets[psnToRetransmit]->Copy();
//         m_socket->Send(packetToResend);
//         NS_LOG_WARN("[Client " << m_nodeId << "] At " 
//                    << Simulator::Now().GetSeconds() 
//                    << "s RTO expired! Retransmitting PSN " << psnToRetransmit);
        
//         // Reset the packet's retransmission timer
//         EventId rtxEvent = Simulator::Schedule(m_rto, &SueClient::Retransmit, this, psnToRetransmit);
//         m_retransmitEvents[psnToRetransmit] = rtxEvent;
//     }
// }

// Round-robin device selection
Ptr<PointToPointSueNetDevice> SueClient::SelectDeviceByRoundRobin() {
    if (m_managedDevices.empty()) {
        NS_LOG_WARN(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                    << "] No managed devices available");
        return nullptr;
    }

    // Round-robin device selection: start from last used index
    uint32_t deviceIndex = m_lastUsedDeviceIndex % m_portsPerSue;
    Ptr<PointToPointSueNetDevice> selectedDevice = m_managedDevices[deviceIndex];

    // Update index for next use
    m_lastUsedDeviceIndex = (deviceIndex + 1) % m_portsPerSue;

    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                << "] Selected device " << selectedDevice->GetIfIndex()
                << " by round-robin (index " << deviceIndex << ", next will be "
                << m_lastUsedDeviceIndex << ")");

    return selectedDevice;
}

Ptr<PointToPointSueNetDevice> SueClient::SelectDeviceByVcCapacity(uint32_t packetSize, uint8_t vcId) {
    if (m_managedDevices.empty()) {
        NS_LOG_WARN(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                    << "] No managed devices available");
        return nullptr;
    }

    // Start from last used device index, check each device's VC capacity in round-robin order
    for (uint32_t i = 0; i < m_portsPerSue; ++i) {
        uint32_t deviceIndex = (m_lastUsedDeviceIndex + i) % m_portsPerSue;
        Ptr<PointToPointSueNetDevice> device = m_managedDevices[deviceIndex];
        
        if (device) {
            // Try to reserve VC capacity
            auto queueManager = device->GetQueueManager();
            if (queueManager && queueManager->ReserveVcCapacity(vcId, packetSize)) {
                // Successfully reserved capacity, select this device
                m_lastUsedDeviceIndex = (deviceIndex + 1) % m_portsPerSue;
                
                NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                            << "] Selected and reserved device " << device->GetIfIndex()
                            << " with VC" << static_cast<uint32_t>(vcId) << " capacity " 
                            << packetSize << " bytes");
                
                return device;
            } else {
                NS_LOG_DEBUG(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                             << "] Device " << device->GetIfIndex() << " VC" << static_cast<uint32_t>(vcId)
                             << " failed to reserve " << packetSize << " bytes");
            }
        }
    }
    
    // All devices have insufficient VC capacity
    NS_LOG_WARN(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                << "] No device has sufficient VC" << static_cast<uint32_t>(vcId) 
                << " capacity for packet size " << packetSize);
    
    return nullptr;
}

void SueClient::PopTransactionsFromQueue(const Destination& dest, uint32_t count) {
    NS_LOG_FUNCTION(this << dest.destXpuId << static_cast<uint32_t>(dest.vcId) << count);
    
    auto it = m_destQueues.find(dest);
    if (it == m_destQueues.end()) {
        NS_LOG_WARN("Destination queue not found for XPU" << dest.destXpuId 
                    << "-VC" << static_cast<uint32_t>(dest.vcId));
        return;
    }
    
    auto& queueInfo = it->second;
    uint32_t removedCount = 0;
    
    // Remove specified number of transactions from the front of the queue
    while (removedCount < count && !queueInfo.queue.empty()) {
        auto frontItem = queueInfo.queue.front();
        Ptr<Packet> transaction = frontItem.second;
        
        // Reduce current burst size
        queueInfo.currentBurstSize -= transaction->GetSize();
        NS_ASSERT(queueInfo.currentBurstSize >= 0 && "currentBurstSize should not be negative");
        
        queueInfo.queue.pop(); // Remove from queue
        removedCount++;

        // Notify LoadBalancer that destination queue space is available
        if (m_destQueueSpaceCallback)
        {
            m_destQueueSpaceCallback (m_sueId, dest.destXpuId, dest.vcId);
        }
    }
    
    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << " SUE" << m_sueId
                << "] Removed " << removedCount << " transactions from queue XPU" 
                << dest.destXpuId << "-VC" << static_cast<uint32_t>(dest.vcId)
                << " (remaining: " << queueInfo.queue.size() << ")");
}

// Socket state validation
bool SueClient::ValidateSocketState(Ptr<Socket> socket, const std::string& context) {
    if (!socket) {
        NS_LOG_WARN(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << "] " << context << ": Socket is null");
        return false;
    }

    // Try to get socket's local address to verify binding status
    Address localAddr;
    if (socket->GetSockName(localAddr) == 0) {
        InetSocketAddress inetLocalAddr = InetSocketAddress::ConvertFrom(localAddr);
        NS_LOG_DEBUG(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << "] " << context
                     << ": Socket bound to " << inetLocalAddr.GetIpv4() << ":" << inetLocalAddr.GetPort());
        return true;
    } else {
        NS_LOG_WARN(Simulator::Now().GetSeconds() << "s [XPU" << m_xpuId << "] " << context << ": Socket is not properly bound");
        return false;
    }
}

// Network state logging
void SueClient::LogNetworkState(const Destination& dest) {
    Ptr<Node> node = GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();

    if (!ipv4) {
        NS_LOG_WARN("Node has no IPv4 stack");
        return;
    }

    NS_LOG_INFO("=== Network State for XPU" << m_xpuId << " SUE" << m_sueId << " ===");
    NS_LOG_INFO("Destination XPU: " << dest.destXpuId);

    // Display all interface information
    NS_LOG_INFO("  Node has " << ipv4->GetNInterfaces() << " interfaces:");
    for (uint32_t i = 0; i < ipv4->GetNInterfaces(); ++i) {
        if (ipv4->IsUp(i)) {
            Ipv4Address addr = ipv4->GetAddress(i, 0).GetLocal();
            Ipv4Mask mask = ipv4->GetAddress(i, 0).GetMask();
            NS_LOG_INFO("    Interface " << i << ": " << addr << "/" << mask << " (UP)");
        } else {
            NS_LOG_INFO("    Interface " << i << ": DOWN");
        }
    }

    // Check MAC address corresponding to target XPU
    // Calculate IP address based on target XPU
    std::ostringstream ipStream;
    ipStream << "10." << (dest.destXpuId + 1) << ".1.1";
    Ipv4Address destIp(ipStream.str().c_str());
    Mac48Address destMac = GetMacForIp(destIp);
    NS_LOG_INFO("Destination MAC for XPU" << dest.destXpuId << ": " << destMac);

    // Check SUE managed device status
    NS_LOG_INFO("SUE Managed Devices: " << m_managedDevices.size());
    for (size_t i = 0; i < m_managedDevices.size(); ++i) {
        Ptr<PointToPointSueNetDevice> dev = m_managedDevices[i];
        if (dev) {
            NS_LOG_INFO("  Device " << dev->GetIfIndex() << ": " << dev->GetAddress());
        }
    }
}

Mac48Address
SueClient::GetMacForIp(Ipv4Address ip){
    auto it = s_ipToMacMap.find(ip);
    if (it != s_ipToMacMap.end()) {
        return it->second;
    }
    return Mac48Address::GetBroadcast(); // Use broadcast address if not found
}

// Packing function: pack transactions for specified destination queue, return multiple packed packets
// Note: Since destination queues are already keyed by {destXpuId, vcId}, this function no longer needs to group by VC
std::vector<Ptr<Packet>>
SueClient::Packing(const Destination& dest) {
    std::vector<Ptr<Packet>> packedPackets;

    // Get destination queue (already keyed by {destXpuId, vcId})
    auto it = m_destQueues.find(dest);
    if (it == m_destQueues.end()) {
        return packedPackets; // Return empty list
    }

    auto& queueInfo = it->second;

    if (queueInfo.queue.empty()) {
        return packedPackets; // Return empty list if queue is empty
    }

    // Record pending transaction info and timestamps, but don't remove from queue
    std::vector<std::pair<Time, Ptr<Packet>>> transactionsToProcess;

    // 1. Iterate through queue, extract all pending transactions (Peek not Pop)
    // Limit number of transactions to process to avoid excessive processing
    size_t maxTransactionsToProcess = std::min(static_cast<size_t>(50), queueInfo.queue.size());
    
    // Use temporary queue to iterate without modifying original queue
    std::queue<std::pair<Time, Ptr<Packet>>> tempQueue = queueInfo.queue;
    
    for (size_t i = 0; i < maxTransactionsToProcess && !tempQueue.empty(); ++i) {
        Time enqueueTime = tempQueue.front().first;
        Ptr<Packet> transaction = tempQueue.front().second;

        transactionsToProcess.emplace_back(enqueueTime, transaction);
        tempQueue.pop(); // Remove from temporary queue
        // Note: Don't pop original queue, don't modify currentBurstSize, don't modify queue
    }

    // 2. Create packed packets directly for current queue (already separated by VC)
    if (!transactionsToProcess.empty()) {
        // Store current batch transactions
        std::vector<Ptr<Packet>> currentBatch;
        uint32_t currentBatchSize = 0;
        uint32_t targetXpuId = dest.destXpuId; // Get directly from dest key
        uint8_t vcId = dest.vcId; // Get directly from dest key

        // 3. Process all transactions, batch by maximum burst size
        for (auto const& transItem : transactionsToProcess) {
            Time enqueueTime = transItem.first;
            Ptr<Packet> transaction = transItem.second;

            // Extract transaction information
            SueHeader sueHeader;
            transaction->PeekHeader(sueHeader);
            uint32_t transactionSize = transaction->GetSize();

            // Check if need to start new batch (exceeds max burst size or batch is not empty)
            if (currentBatchSize + transactionSize > m_maxBurstSize && !currentBatch.empty()) {
                // Create packed packet
                Ptr<Packet> packedPacket = CreateCombinedPacket(currentBatch, vcId, targetXpuId);
                if (packedPacket) {
                    packedPackets.push_back(packedPacket);
                }

                // Reset batch
                currentBatch.clear();
                currentBatchSize = 0;
            }

            // Remove SUE header and add payload to current batch
            Ptr<Packet> payloadPacket = transaction->Copy();
            payloadPacket->RemoveHeader(sueHeader);
            currentBatch.push_back(payloadPacket);
            currentBatchSize += transactionSize;
        }

        // 4. Process the last batch
        if (!currentBatch.empty()) {
            Ptr<Packet> packedPacket = CreateCombinedPacket(currentBatch, vcId, targetXpuId);
            if (packedPacket) {
                packedPackets.push_back(packedPacket);
            }
        }
    }

    NS_LOG_INFO("Packing: destination XPU" << dest.destXpuId << "-VC" << (uint32_t)dest.vcId
               << " processed " << transactionsToProcess.size()
               << " transactions into " << packedPackets.size() << " packed packets");

    // Store packing statistics for unified log writing
    if (!transactionsToProcess.empty()) {
        Time now = Simulator::Now();
        Time firstEnqueueTime = transactionsToProcess[0].first;
        int64_t waitTimeNs = (now - firstEnqueueTime).GetNanoSeconds();

        // Store packing delay data (nanoseconds)
        m_packDelays.push_back(waitTimeNs);

        // Store packing count data
        m_packNumbers.push_back(packedPackets.size());

        NS_LOG_DEBUG("Pack statistics stored for XPU" << m_xpuId
                    << " - Delay: " << waitTimeNs << "ns, Packets: " << packedPackets.size()
                    << " (waiting for LogClientStatistics to write)");
    }

    return packedPackets;
}

// Helper function: create combined data packet
Ptr<Packet>
SueClient::CreateCombinedPacket(const std::vector<Ptr<Packet>>& payloads,
                                uint8_t vcId, uint32_t targetXpuId) {
    if (payloads.empty()) {
        return nullptr;
    }

    // Create empty combined packet
    Ptr<Packet> combinedPacket = Create<Packet>();

    // Concatenate all payload packets
    for (const auto& payload : payloads) {
        combinedPacket->AddAtEnd(payload);
    }

    // Create new SUE header
    SueHeader newHeader;
    newHeader.SetPsn(m_psn);
    newHeader.SetXpuId(targetXpuId);  // Set target XPU ID
    newHeader.SetVc(vcId);           // Set VC ID
    newHeader.SetOp(0);              // Data packet operation

    // Add SUE header
    combinedPacket->AddHeader(newHeader);

    return combinedPacket;
}

// Credit Management Implementation with HPC Delay and Batch Processing



// Analyze transaction count in packet
uint32_t
SueClient::AnalyzeTransactionCount(Ptr<Packet> packet) const {
    if (!packet) {
        return 0;
    }

    // Remove SUE header to analyze payload
    Ptr<Packet> packetCopy = packet->Copy();
    SueHeader sueHeader;

    try {
        packetCopy->RemoveHeader(sueHeader);
    } catch (...) {
        NS_LOG_WARN("Failed to extract SUE header for transaction analysis");
        return 1; // Return 1 by default, at least contains one transaction
    }

    uint32_t payloadSize = packetCopy->GetSize();

    // If no payload size information, return 1 transaction by default
    if (payloadSize == 0) {
        return 1;
    }

    // Calculate transaction count: estimate based on average transaction size
    uint32_t estimatedTransactionCount = (payloadSize + m_transactionSize - 1) / m_transactionSize;

    // Ensure at least 1 transaction is returned
    estimatedTransactionCount = std::max(estimatedTransactionCount, uint32_t(1));

    NS_LOG_DEBUG("Analyzed packet: payloadSize=" << payloadSize
                << " bytes, transactionSize=" << m_transactionSize
                << " bytes, estimatedTransactions=" << estimatedTransactionCount);

    return estimatedTransactionCount;
}


// Check if there are pending transactions
bool
SueClient::HasPendingTransactions() const {
    // Check if there are pending transaction queues
    for (const auto& pair : m_destQueues) {
        if (!pair.second.queue.empty()) {
            return true;
        }
    }
    return false;
}

uint32_t
SueClient::GetPendingTransactionCount() const {
    uint32_t totalCount = 0;

    // Calculate total transaction count in all destination queues
    for (const auto& pair : m_destQueues) {
        totalCount += pair.second.queue.size();
    }

    return totalCount;
}

// Log destination queue usage
void
SueClient::LogDestinationQueueUsage() {
    if (!m_loggingEnabled) {
        return;
    }

    uint64_t timeNs = Simulator::Now().GetNanoSeconds();

    // Iterate through all destination queues, log byte-level usage
    for (const auto& pair : m_destQueues) {
        uint32_t destXpuId = pair.first.destXpuId;
        uint8_t vcId = pair.first.vcId;
        const auto& queueInfo = pair.second;

        uint32_t currentBytes = queueInfo.currentBurstSize;
        uint32_t maxBytes = m_destQueueMaxBytes; // Configurable destination queue maximum capacity

        // Log byte-level usage to PerformanceLogger, add VC ID information
        PerformanceLogger::GetInstance().LogDestinationQueueUsage(
            timeNs, m_xpuId, m_sueId, destXpuId, vcId, currentBytes, maxBytes);
    }
}

// Credit Management Implementation

uint32_t
SueClient::GetManagedDeviceCount () const
{
  NS_LOG_FUNCTION (this);
  return m_managedDevices.size ();
}

uint32_t
SueClient::GetDestQueueMaxBytes () const
{
  NS_LOG_FUNCTION (this);
  return m_destQueueMaxBytes;
}

uint32_t
SueClient::GetDestQueueCurrentUsage (uint32_t destXpuId, uint8_t vcId) const
{
  NS_LOG_FUNCTION (this << destXpuId << static_cast<uint32_t> (vcId));

  Destination dest;
  dest.destXpuId = destXpuId;
  dest.vcId = vcId;

  auto it = m_destQueues.find (dest);
  if (it == m_destQueues.end ())
    {
      return 0; // Queue does not exist, usage is 0
    }

  return it->second.currentBurstSize;
}



} // namespace ns3