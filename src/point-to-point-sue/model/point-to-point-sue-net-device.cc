/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2008 University of Washington
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

#include "ns3/log.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"
#include <sstream>
#include "ns3/mac48-address.h"
#include "ns3/llc-snap-header.h"
#include "ns3/error-model.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"
#include "ns3/pointer.h"
#include "ns3/string.h"
#include "point-to-point-sue-net-device.h"
#include "point-to-point-sue-channel.h"
#include "sue-cbfc-header.h"
#include "ns3/drop-tail-queue.h"
#include "performance-logger.h"
#include "sue-header.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/udp-header.h"
#include "ns3/ethernet-header.h"
#include "ns3/ppp-header.h"
#include "ns3/performance-logger.h"
#include "xpu-delay-tag.h"

namespace ns3
{

    NS_LOG_COMPONENT_DEFINE("PointToPointSueNetDevice");

    std::map<Ipv4Address, Mac48Address> PointToPointSueNetDevice::s_ipToMacMap;

    NS_OBJECT_ENSURE_REGISTERED(PointToPointSueNetDevice);

    TypeId
    PointToPointSueNetDevice::GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::PointToPointSueNetDevice")
                                .SetParent<NetDevice>()
                                .SetGroupName("PointToPointSue")
                                .AddConstructor<PointToPointSueNetDevice>()
                                .AddAttribute("Mtu", "The MAC-level Maximum Transmission Unit",
                                              UintegerValue(DEFAULT_MTU),
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::SetMtu,
                                                                   &PointToPointSueNetDevice::GetMtu),
                                              MakeUintegerChecker<uint16_t>())
                                .AddAttribute("Address",
                                              "The MAC address of this device.",
                                              Mac48AddressValue(Mac48Address("ff:ff:ff:ff:ff:ff")),
                                              MakeMac48AddressAccessor(&PointToPointSueNetDevice::m_address),
                                              MakeMac48AddressChecker())
                                .AddAttribute("DataRate",
                                              "The default data rate for point to point links",
                                              DataRateValue(DataRate("32768b/s")),
                                              MakeDataRateAccessor(&PointToPointSueNetDevice::m_bps),
                                              MakeDataRateChecker())
                                .AddAttribute("ReceiveErrorModel",
                                              "The receiver error model used to simulate packet loss",
                                              PointerValue(),
                                              MakePointerAccessor(&PointToPointSueNetDevice::m_receiveErrorModel),
                                              MakePointerChecker<ErrorModel>())
                                .AddAttribute("InterframeGap",
                                              "The time to wait between packet (frame) transmissions",
                                              TimeValue(Seconds(0.0)),
                                              MakeTimeAccessor(&PointToPointSueNetDevice::m_tInterframeGap),
                                              MakeTimeChecker())
                                // CBFC
                                .AddAttribute("EnableLinkCBFC",
                                              "If enable LINK CBFC.",
                                              BooleanValue(false),
                                              MakeBooleanAccessor(&PointToPointSueNetDevice::m_enableLinkCBFC),
                                              MakeBooleanChecker())
                                .AddAttribute("InitialCredits", "The initial credits for each VC.",
                                              UintegerValue(20),
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::m_initialCredits),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("NumVcs", "The number of Virtual Channels.",
                                              UintegerValue(4),
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::m_numVcs),
                                              MakeUintegerChecker<uint8_t>())
                                .AddAttribute("VcQueueMaxBytes", "The maximum size of VC queues in bytes.",
                                              UintegerValue(2 * 1024 * 1024),
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::m_vcQueueMaxBytes),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("ProcessingQueueMaxBytes",
                                              "The maximum size of processing queue in bytes (default 2MB)",
                                              UintegerValue(2 * 1024 * 1024),
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::m_processingQueueMaxBytes),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("ProcessingDelayPerPacket",
                                              "Processing delay time for each package",
                                              TimeValue(NanoSeconds(10)),
                                              MakeTimeAccessor(&PointToPointSueNetDevice::m_processingDelay),
                                              MakeTimeChecker())
                                .AddAttribute("CreditBatchSize",
                                              "Number of packets to receive before sending a credit update",
                                              UintegerValue(10), // Default value: 10 packets
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::m_creditBatchSize),
                                              MakeUintegerChecker<uint32_t>(1, 1000))
                                .AddAttribute("AdditionalHeaderSize",
                                              "Additional header size for capacity reservation (default 46 bytes)",
                                              UintegerValue(46), // Default value: 46 bytes
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::m_additionalHeaderSize),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("LinkStatInterval",
                                              "Link Statistic Interval",
                                              StringValue("10us"), // Default: 10 microseconds
                                              MakeStringAccessor(&PointToPointSueNetDevice::m_linkStatIntervalString),
                                              MakeStringChecker())
                                .AddAttribute("CreUpdateAddHeadDelay",
                                              "Credit Update packet Add Head Delay",
                                              TimeValue(NanoSeconds(3)),
                                              MakeTimeAccessor(&PointToPointSueNetDevice::m_creUpdateAddHeadDelay),
                                              MakeTimeChecker())
                                .AddAttribute("DataAddHeadDelay",
                                              "Data packet Add Head Delay",
                                              TimeValue(NanoSeconds(5)),
                                              MakeTimeAccessor(&PointToPointSueNetDevice::m_dataAddHeadDelay),
                                              MakeTimeChecker())
                                .AddAttribute("StatLoggingEnabled",
                                              "Stat Logging Enabled Switch",
                                              BooleanValue(true),
                                              MakeBooleanAccessor(&PointToPointSueNetDevice::m_loggingEnabled),
                                              MakeBooleanChecker())
                                .AddAttribute("ProcessingRate",
                                              "The data rate at which this device can process received packets",
                                              StringValue("200Gbps"), // Default: 200Gbps for compatibility
                                              MakeStringAccessor(&PointToPointSueNetDevice::m_processingRateString),
                                              MakeStringChecker())
                                .AddAttribute("CreditGenerateDelay",
                                              "The delay before sending a credit update after a batch is ready",
                                              TimeValue(NanoSeconds(10)), // Default: 10 nanoseconds
                                              MakeTimeAccessor(&PointToPointSueNetDevice::m_creditGenerateDelay),
                                              MakeTimeChecker())
                                .AddAttribute("SwitchForwardDelay",
                                              "Delay before forwarding packets in switch",
                                              TimeValue(NanoSeconds(150)),
                                              MakeTimeAccessor(&PointToPointSueNetDevice::m_switchForwardDelay),
                                              MakeTimeChecker())
                                .AddAttribute("VcSchedulingDelay",
                                              "VC queue scheduling delay",
                                              TimeValue(NanoSeconds(8)),   // Modern NIC scheduling time is approximately 5-10ns
                                              MakeTimeAccessor(&PointToPointSueNetDevice::m_vcSchedulingDelay),
                                              MakeTimeChecker())
                                //
                                // Transmit queueing discipline for the device which includes its own set
                                // of trace hooks.
                                //
                                .AddAttribute("TxQueue",
                                              "A queue to use as the transmit queue in the device.",
                                              PointerValue(),
                                              MakePointerAccessor(&PointToPointSueNetDevice::m_queue),
                                              MakePointerChecker<Queue<Packet>>())

                                //
                                // Trace sources at the "top" of the net device, where packets transition
                                // to/from higher layers.
                                //
                                .AddTraceSource("MacTx",
                                                "Trace source indicating a packet has arrived "
                                                "for transmission by this device",
                                                MakeTraceSourceAccessor(&PointToPointSueNetDevice::m_macTxTrace),
                                                "ns3::Packet::TracedCallback")
                                .AddTraceSource("MacTxDrop",
                                                "Trace source indicating a packet has been dropped "
                                                "by the device before transmission",
                                                MakeTraceSourceAccessor(&PointToPointSueNetDevice::m_macTxDropTrace),
                                                "ns3::Packet::TracedCallback")
                                .AddTraceSource("MacPromiscRx",
                                                "A packet has been received by this device, "
                                                "has been passed up from the physical layer "
                                                "and is being forwarded up the local protocol stack.  "
                                                "This is a promiscuous trace,",
                                                MakeTraceSourceAccessor(&PointToPointSueNetDevice::m_macPromiscRxTrace),
                                                "ns3::Packet::TracedCallback")
                                .AddTraceSource("MacRx",
                                                "A packet has been received by this device, "
                                                "has been passed up from the physical layer "
                                                "and is being forwarded up the local protocol stack.  "
                                                "This is a non-promiscuous trace,",
                                                MakeTraceSourceAccessor(&PointToPointSueNetDevice::m_macRxTrace),
                                                "ns3::Packet::TracedCallback")
#if 0
    // Not currently implemented for this device
    .AddTraceSource ("MacRxDrop",
                     "Trace source indicating a packet was dropped "
                     "before being forwarded up the stack",
                     MakeTraceSourceAccessor (&PointToPointSueNetDevice::m_macRxDropTrace),
                     "ns3::Packet::TracedCallback")
#endif
                                //
                                // Trace sources at the "bottom" of the net device, where packets transition
                                // to/from the channel.
                                //
                                .AddTraceSource("PhyTxBegin",
                                                "Trace source indicating a packet has begun "
                                                "transmitting over the channel",
                                                MakeTraceSourceAccessor(&PointToPointSueNetDevice::m_phyTxBeginTrace),
                                                "ns3::Packet::TracedCallback")
                                .AddTraceSource("PhyTxEnd",
                                                "Trace source indicating a packet has been "
                                                "completely transmitted over the channel",
                                                MakeTraceSourceAccessor(&PointToPointSueNetDevice::m_phyTxEndTrace),
                                                "ns3::Packet::TracedCallback")
                                .AddTraceSource("PhyTxDrop",
                                                "Trace source indicating a packet has been "
                                                "dropped by the device during transmission",
                                                MakeTraceSourceAccessor(&PointToPointSueNetDevice::m_phyTxDropTrace),
                                                "ns3::Packet::TracedCallback")
#if 0
    // Not currently implemented for this device
    .AddTraceSource ("PhyRxBegin",
                     "Trace source indicating a packet has begun "
                     "being received by the device",
                     MakeTraceSourceAccessor (&PointToPointSueNetDevice::m_phyRxBeginTrace),
                     "ns3::Packet::TracedCallback")
#endif
                                .AddTraceSource("PhyRxEnd",
                                                "Trace source indicating a packet has been "
                                                "completely received by the device",
                                                MakeTraceSourceAccessor(&PointToPointSueNetDevice::m_phyRxEndTrace),
                                                "ns3::Packet::TracedCallback")
                                .AddTraceSource("PhyRxDrop",
                                                "Trace source indicating a packet has been "
                                                "dropped by the device during reception",
                                                MakeTraceSourceAccessor(&PointToPointSueNetDevice::m_phyRxDropTrace),
                                                "ns3::Packet::TracedCallback")

                                //
                                // Trace sources designed to simulate a packet sniffer facility (tcpdump).
                                // Note that there is really no difference between promiscuous and
                                // non-promiscuous traces in a point-to-point link.
                                //
                                .AddTraceSource("Sniffer",
                                                "Trace source simulating a non-promiscuous packet sniffer "
                                                "attached to the device",
                                                MakeTraceSourceAccessor(&PointToPointSueNetDevice::m_snifferTrace),
                                                "ns3::Packet::TracedCallback")
                                .AddTraceSource("PromiscSniffer",
                                                "Trace source simulating a promiscuous packet sniffer "
                                                "attached to the device",
                                                MakeTraceSourceAccessor(&PointToPointSueNetDevice::m_promiscSnifferTrace),
                                                "ns3::Packet::TracedCallback");
        return tid;
    }

    // Define declared static constants
    const uint16_t PointToPointSueNetDevice::PROT_CBFC_UPDATE;

    PointToPointSueNetDevice::PointToPointSueNetDevice()
        : m_txMachineState(READY),
          m_channel(0),
          m_linkUp(false),
          m_currentPkt(0),
          // CBFC
          m_cbfcInitialized(false),
          m_initialCredits(0),
          m_numVcs(0),
          m_creditBatchSize(10),
          m_vcQueueMaxBytes(2 * 1024 * 1024), // Default VC queue max capacity 2MB (2*1024*1024 bytes)
          m_additionalHeaderSize(46), // Default 46 bytes
          m_currentProcessingQueueSize(0),
          m_currentProcessingQueueBytes(0),
          m_isProcessing(false),
          m_processingDelay(NanoSeconds(10)),
          m_processingQueueMaxBytes(2 * 1024 * 1024), // Default processing queue max capacity 2MB
          m_linkStatInterval(MilliSeconds(10)),
          m_enableLinkCBFC(false),
          m_totalPacketDropNum(0),
          m_creUpdateAddHeadDelay(NanoSeconds(3)),
          m_dataAddHeadDelay(NanoSeconds(5)),
          m_creditGenerateDelay(NanoSeconds(10)),
          m_switchForwardDelay(NanoSeconds(150)),
          m_vcSchedulingDelay(NanoSeconds(8)),
          m_loggingEnabled(true), // Enable logging by default
          m_processingRate(m_bps), // Default same as transmission rate
          m_processingRateString("200Gbps"), // Default processing rate string
          m_linkStatIntervalString("10us") // Default link stat interval string
    {
        NS_LOG_FUNCTION(this);
    }

    PointToPointSueNetDevice::~PointToPointSueNetDevice()
    {
        NS_LOG_FUNCTION(this);
    }

    // Initialize CBFC functionality
    void
    PointToPointSueNetDevice::InitializeCbfc()
    {
        if (m_cbfcInitialized)
            return;

        // Convert processing rate string to DataRate for compatibility
        if (!m_processingRateString.empty())
        {
            try
            {
                // Convert processing rate string to DataRate - handle Gbps format manually
                std::string rateStr = m_processingRateString;
                uint64_t bps = 0;

                if (rateStr.find("Gbps") != std::string::npos)
                {
                    size_t pos = rateStr.find("Gbps");
                    std::string number = rateStr.substr(0, pos);
                    double value = std::stod(number);
                    bps = static_cast<uint64_t>(value * 1000000000); // Convert Gbps to bps
                }
                else if (rateStr.find("Mbps") != std::string::npos)
                {
                    size_t pos = rateStr.find("Mbps");
                    std::string number = rateStr.substr(0, pos);
                    double value = std::stod(number);
                    bps = static_cast<uint64_t>(value * 1000000); // Convert Mbps to bps
                }
                else if (rateStr.find("Kbps") != std::string::npos)
                {
                    size_t pos = rateStr.find("Kbps");
                    std::string number = rateStr.substr(0, pos);
                    double value = std::stod(number);
                    bps = static_cast<uint64_t>(value * 1000); // Convert Kbps to bps
                }
                else if (rateStr.find("bps") != std::string::npos)
                {
                    size_t pos = rateStr.find("bps");
                    std::string number = rateStr.substr(0, pos);
                    bps = static_cast<uint64_t>(std::stod(number));
                }

                if (bps > 0)
                {
                    m_processingRate = DataRate(bps);
                }
                else
                {
                    throw std::invalid_argument("Unknown rate format");
                }
                NS_LOG_INFO("Processing rate set to: " << m_processingRateString << " (" << m_processingRate.GetBitRate() << " bps)");
            }
            catch (const std::exception& e)
            {
                NS_LOG_WARN("Invalid processing rate format: " << m_processingRateString << ", using default value");
                m_processingRate = DataRate("200Gb/s");
            }
        }

        // Convert link stat interval string to Time for compatibility
        if (!m_linkStatIntervalString.empty())
        {
            try
            {
                // Convert "us" to ns3 compatible format
                std::string timeStr = m_linkStatIntervalString;
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
                m_linkStatInterval = Time(timeStr);
                NS_LOG_INFO("Link stat interval set to: " << m_linkStatIntervalString << " (" << m_linkStatInterval.GetNanoSeconds() << " ns)");
            }
            catch (const std::exception& e)
            {
                NS_LOG_WARN("Invalid link stat interval format: " << m_linkStatIntervalString << ", using default value");
                m_linkStatInterval = MilliSeconds(10);
            }
        }

        // Initialize peer device credits regardless of whether this is a switch device
        Mac48Address peerMac = GetRemoteMac();
        for (uint8_t vc = 0; vc < m_numVcs; vc++)
        {
            m_txCreditsMap[peerMac][vc] = m_initialCredits;
            m_rxCreditsToReturnMap[peerMac][vc] = 0;
        }

        // If switch device, initialize credit allocation for other devices on the switch
        if (IsSwitchDevice())
        {
            // Switch device: initialize credits for all peer devices on all ports
            Ptr<Node> node = GetNode();
            for (uint32_t i = 0; i < node->GetNDevices(); i++)
            {
                Ptr<NetDevice> dev = node->GetDevice(i);
                Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice>(dev);
                // Check if conversion is successful
                if (p2pDev && p2pDev != this)
                {
                    Mac48Address mac = Mac48Address::ConvertFrom(dev->GetAddress());
                    for (uint8_t vc = 0; vc < m_numVcs; vc++)
                    {
                        m_txCreditsMap[mac][vc] = 85;
                        m_rxCreditsToReturnMap[mac][vc] = 0;
                    }
                }
            }
        }

        for (uint8_t i = 0; i < m_numVcs; ++i)
        {
            m_vcQueues[i] = CreateObject<DropTailQueue<Packet>>();
            std::stringstream vcMaxSize;
            vcMaxSize << m_vcQueueMaxBytes << "B";
            m_vcQueues[i]->SetAttribute("MaxSize", QueueSizeValue(QueueSize(vcMaxSize.str()))); // Use parameterized VC queue size (bytes)

            // Initialize reserved capacity
            m_vcReservedCapacity[i] = 0;

            // Initialize drop statistics
            m_vcDropCounts[i] = 0;
        }
        // Handle link layer sender queue packet drops
        for (auto &queuePair : m_vcQueues)
        {
            ns3::Ptr<ns3::Queue<ns3::Packet>> queue = queuePair.second;

            // Connect Drop trace source to custom handler HandlePacketDrop
            queue->TraceConnectWithoutContext("Drop", ns3::MakeCallback(&PointToPointSueNetDevice::HandlePacketDrop, this));
        }

        m_cbfcInitialized = true;
        if (!IsSwitchDevice())
        {
            NS_LOG_INFO("Link: Initialized on Node " << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                                     << " with " << (uint32_t)m_numVcs << " VCs and " << m_initialCredits << " initial credits.");
        }
        // Start statistics after initialization completes
        m_lastStatTime = Simulator::Now();
        m_logStatisticsEvent = Simulator::Schedule(m_linkStatInterval, &PointToPointSueNetDevice::LogStatistics, this);
    }

    Mac48Address PointToPointSueNetDevice::GetRemoteMac()
    {
        NS_LOG_FUNCTION(this);
        Address remoteAddress = GetRemote();
        return Mac48Address::ConvertFrom(remoteAddress);
    }

    Mac48Address PointToPointSueNetDevice::GetLocalMac()
    {
        NS_LOG_FUNCTION(this);
        return Mac48Address::ConvertFrom(GetAddress());
    }

    // Custom packet drop handler (member function)
    void PointToPointSueNetDevice::HandlePacketDrop(ns3::Ptr<const ns3::Packet> droppedPacket)
    {
        // Extract VC ID from packet
        uint8_t vcId = ExtractVcIdFromPacket(droppedPacket);
        m_vcDropCounts_SendQ[vcId + 1]++;
        m_totalPacketDropNum += 1;
    }

    uint32_t
    PointToPointSueNetDevice::GetTotalPacketDropNum()
    {
        return m_totalPacketDropNum;
    }

    void PointToPointSueNetDevice::SetLoggingEnabled(bool enabled)
    {
        NS_LOG_FUNCTION(this << enabled);
        m_loggingEnabled = enabled;
    }

    void PointToPointSueNetDevice::SetVcQueueMaxBytes(uint32_t maxBytes)
    {
        NS_LOG_FUNCTION(this << maxBytes);
        m_vcQueueMaxBytes = maxBytes;

        // If VC queues already created, update their maximum capacity
        for (uint8_t i = 0; i < m_numVcs; ++i)
        {
            if (m_vcQueues[i])
            {
                std::stringstream vcMaxSize;
                vcMaxSize << m_vcQueueMaxBytes << "B";
                m_vcQueues[i]->SetAttribute("MaxSize", QueueSizeValue(QueueSize(vcMaxSize.str())));
            }
        }
    }

    uint32_t PointToPointSueNetDevice::GetVcQueueMaxBytes(void) const
    {
        return m_vcQueueMaxBytes;
    }

    void PointToPointSueNetDevice::LogStatistics()
    {
        // Check if logging is enabled
        if (!m_loggingEnabled)
        {
            NS_LOG_INFO("Logging disabled on device " << GetIfIndex());
            return;
        }

        Time currentTime = Simulator::Now();
        int64_t nanoseconds = currentTime.GetNanoSeconds();
        for (auto &entry : m_vcBytesSent)
        {
            double rate = (entry.second * 8) / m_linkStatInterval.GetSeconds() / 1e9; // Gbps
            PerformanceLogger::GetInstance().LogDeviceStat(
                nanoseconds, GetNode()->GetId() + 1, GetIfIndex(), entry.first, "Tx", rate);
            entry.second = 0;
        }
        for (auto &entry : m_vcBytesReceived)
        {
            double rate = (entry.second * 8) / m_linkStatInterval.GetSeconds() / 1e9; // Gbps
            PerformanceLogger::GetInstance().LogDeviceStat(
                nanoseconds, GetNode()->GetId() + 1, GetIfIndex(), entry.first, "Rx", rate);
            entry.second = 0;
        }
        // Log packet drop statistics
        // Receiver queue drops
        for (auto &entry : m_vcDropCounts)
        {
            if (entry.second > 0)
            {
                PerformanceLogger::GetInstance().LogDropStat(
                    nanoseconds, GetNode()->GetId() + 1, GetIfIndex(),
                    entry.first, "LinkReceiveDrop", entry.second);
                entry.second = 0; // Reset counter
            }
        }
        // Sender queue drops
        for (auto &entry : m_vcDropCounts_SendQ)
        {
            if (entry.second > 0)
            {
                PerformanceLogger::GetInstance().LogDropStat(
                    nanoseconds, GetNode()->GetId() + 1, GetIfIndex(),
                    entry.first, "LinkSendDrop", entry.second);
                entry.second = 0; // Reset counter
            }
        }

        // Log device credit changes
        // Iterate through all target device credit mappings
        for (const auto &macEntry : m_txCreditsMap)
        {
            Mac48Address targetMac = macEntry.first;
            for (const auto &vcEntry : macEntry.second)
            {
                uint8_t vcId = vcEntry.first;
                uint32_t credits = vcEntry.second;

                // Convert Mac48Address to string format
                std::ostringstream macStream;
                macStream << targetMac;
                std::string macStr = macStream.str();
                if(IsSwitchDevice()){
                    // Log credit status
                    PerformanceLogger::GetInstance().LogCreditStat(
                        nanoseconds, GetNode()->GetId() + 1, GetIfIndex(),
                        vcId, "SwitchCredits", credits, macStr);
                }
                else{
                    // Log credit status
                    PerformanceLogger::GetInstance().LogCreditStat(
                        nanoseconds, GetNode()->GetId() + 1, GetIfIndex(),
                        vcId, "XPUCredits", credits, macStr);
                }

            }
        }

        // Log queue utilization
        LogDeviceQueueUsage();

        // Only reschedule when logging is enabled
        if (m_loggingEnabled)
        {
            m_logStatisticsEvent = Simulator::Schedule(m_linkStatInterval,
                                                       &PointToPointSueNetDevice::LogStatistics, this);
        }
    }

    void
    PointToPointSueNetDevice::AddHeader(Ptr<Packet> p, uint16_t protocolNumber)
    {
        NS_LOG_FUNCTION(this << p << protocolNumber);
        PppHeader ppp;
        ppp.SetProtocol(EtherToPpp(protocolNumber));
        p->AddHeader(ppp);
    }

    bool
    PointToPointSueNetDevice::ProcessHeader(Ptr<Packet> p, uint16_t &param)
    {
        NS_LOG_FUNCTION(this << p << param);
        PppHeader ppp;
        p->RemoveHeader(ppp);
        param = PppToEther(ppp.GetProtocol());
        return true;
    }

    void
    PointToPointSueNetDevice::DoDispose()
    {
        NS_LOG_FUNCTION(this);
        m_node = 0;
        m_channel = 0;
        m_receiveErrorModel = 0;
        m_currentPkt = nullptr;
        m_queue = 0;
        NetDevice::DoDispose();
    }

    void
    PointToPointSueNetDevice::SetDataRate(DataRate bps)
    {
        NS_LOG_FUNCTION(this);
        m_bps = bps;
    }

    bool PointToPointSueNetDevice::IsSwitchDevice() const
    {
        Mac48Address addr = m_address;
        uint8_t buffer[6];
        addr.CopyTo(buffer);
        uint8_t lastByte = buffer[5]; // Last byte of MAC address
        // TODO: Simplistic logic; needs modification for proper XPU/switch identification
        return (lastByte % 2 == 0); // Even numbers are switch devices
    }

    bool PointToPointSueNetDevice::IsMacSwitchDevice(Mac48Address mac) const
    {
        uint8_t buffer[6];
        mac.CopyTo(buffer);
        uint8_t lastByte = buffer[5]; // Last byte of MAC address
        // TODO: Simplistic logic; needs modification for proper XPU/switch identification
        return (lastByte % 2 == 0); // Even numbers are switch devices
    }

    void
    PointToPointSueNetDevice::SetInterframeGap(Time t)
    {
        NS_LOG_FUNCTION(this << t.As(Time::S));
        m_tInterframeGap = t;
    }

    bool
    PointToPointSueNetDevice::TransmitStart(Ptr<Packet> p)
    {
        NS_LOG_FUNCTION(this << p);
        NS_LOG_LOGIC("UID is " << p->GetUid() << ")");

        //
        // This function is called to start the process of transmitting a packet.
        // We need to tell the channel that we've started wiggling the wire and
        // schedule an event that will be executed when the transmission is complete.
        //
        NS_ASSERT_MSG(m_txMachineState == READY, "Must be READY to transmit");
        m_txMachineState = BUSY;
        m_currentPkt = p;
        m_phyTxBeginTrace(m_currentPkt);

        // Add timestamp tag to packets sent by XPU devices
        if (!IsSwitchDevice())
        {
            XpuDelayTag timestampTag(Simulator::Now());
            p->AddPacketTag(timestampTag);
            NS_LOG_DEBUG("Added XPU timestamp tag to packet UID " << p->GetUid()
                        << " at time " << Simulator::Now().GetNanoSeconds() << "ns");
        }

        Time txTime = m_bps.CalculateBytesTxTime(p->GetSize());
        Time txCompleteTime = txTime + m_tInterframeGap;

        NS_LOG_LOGIC("Schedule TransmitCompleteEvent in " << txCompleteTime.As(Time::S));
        Simulator::Schedule(txCompleteTime, &PointToPointSueNetDevice::TransmitComplete, this);

        Simulator::Schedule(txCompleteTime, &PointToPointSueNetDevice::SendPacketStatistic, this, p);

        // Switch egress port: credit return only after packet transmission
        PppHeader ppp;
        p->PeekHeader(ppp);

        if (IsSwitchDevice() && ppp.GetProtocol() != EtherToPpp(PROT_CBFC_UPDATE))
        {
            // Extract VC ID from packet
            uint8_t vcId = ExtractVcIdFromPacket(p);

            // Switch egress port: replace Source Destination MAC with current device MAC
            // to enable universal credit calculation based on SourceMac.
            Mac48Address targetMac = GetSourceMac(p, true);

            Simulator::Schedule(txCompleteTime, &PointToPointSueNetDevice::CreditReturn, this, targetMac, vcId);
        }

        bool result = m_channel->TransmitStart(p, this, txTime);
        if (result == false)
        {
            m_phyTxDropTrace(p);
            // TODO: Link-level retransmission
        }
        return result;
    }

    void PointToPointSueNetDevice::SendPacketStatistic(Ptr<Packet> packet)
    {
        PppHeader ppp;
        packet->PeekHeader(ppp);

        // Extract VC ID from packet
        uint8_t vcId = ExtractVcIdFromPacket(packet);

        if (ppp.GetProtocol() == EtherToPpp(PROT_CBFC_UPDATE))
        { // If it's an update packet
            // Temporarily do not count credit packets
            // m_vcBytesSent[0] += packet->GetSize(); // VC0 is used for credit packets
        }
        else
        {
            m_vcBytesSent[vcId + 1] += packet->GetSize(); // Data packet statistics
        }
    }

    void PointToPointSueNetDevice::ReceivePacketStatistic(Ptr<Packet> packet)
    {
        PppHeader ppp;
        packet->PeekHeader(ppp);

        // Extract VC ID from packet
        uint8_t vcId = ExtractVcIdFromPacket(packet);

        if (ppp.GetProtocol() == EtherToPpp(PROT_CBFC_UPDATE))
        { // If it's an update packet
            // m_vcBytesReceived[0] += packet->GetSize(); // VC0 is used for credit packets
        }
        else
        {
            m_vcBytesReceived[vcId + 1] += packet->GetSize(); // Data packet statistics
        }
    }

    Mac48Address
    PointToPointSueNetDevice::GetSourceMac(Ptr<Packet> p, bool ChangeHead)
    {
        PppHeader ppp;
        SueCbfcHeader dataHeader;
        EthernetHeader ethHeader;
        p->RemoveHeader(ppp);
        p->RemoveHeader(dataHeader);
        p->RemoveHeader(ethHeader);
        Mac48Address sourceMac = ethHeader.GetSource();

        if (ChangeHead)
        {
            ethHeader.SetSource(GetLocalMac());
        }

        p->AddHeader(ethHeader);
        p->AddHeader(dataHeader);
        p->AddHeader(ppp);

        return sourceMac;
    }

    // Core function to check all queues and trigger transmission
    void
    PointToPointSueNetDevice::TryTransmit()
    {
        if (m_txMachineState != READY)
        {
            return;
        }

        // 1. Prioritize checking high-priority main queue (for credit packets)
        if (!m_queue->IsEmpty())
        {
            Ptr<Packet> packet = m_queue->Dequeue();

            if (!IsSwitchDevice())
            {
                NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                          << "] sending credit packet from main queue"
                                          << " (main queue size now: " << m_queue->GetNPackets() << " packets)");
            }

            m_snifferTrace(packet);
            m_promiscSnifferTrace(packet);
            TransmitStart(packet);
            return;
        }
        else
        {
            // 2. Poll all VC queues (weighted round robin)
            static uint8_t lastVC = 0;
            // 2. If main queue is empty, poll all VC queues
            // TODO link layer
            for (uint8_t i = 0; i < m_numVcs; ++i)
            {
                uint8_t currentVC = (lastVC + i) % m_numVcs;
                if (!m_vcQueues[currentVC]->IsEmpty() && m_txCreditsMap[GetRemoteMac()][currentVC] > 0)
                {
                    if (m_enableLinkCBFC)
                    {
                        m_txCreditsMap[GetRemoteMac()][currentVC]--;
                    }
                    Ptr<Packet> packet = m_vcQueues[currentVC]->Dequeue();

                    if (!IsSwitchDevice())
                    {
                        NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex() << "] sending packet for VC " << (uint32_t)currentVC
                                                  << ". Credits left: " << m_txCreditsMap[GetRemoteMac()][currentVC]
                                                  << " (VC queue size now: " << m_vcQueues[currentVC]->GetNPackets() << " packets)");
                    }

                    m_snifferTrace(packet);
                    m_promiscSnifferTrace(packet);
                    TransmitStart(packet);
                    lastVC = (currentVC + 1) % m_numVcs; // Update last serviced VC
                    return;
                }
            }
        }
    }

    void
    PointToPointSueNetDevice::TransmitComplete(void)
    {
        NS_LOG_FUNCTION(this);

        //
        // This function is called to when we're all done transmitting a packet.
        // We try and pull another packet off of the transmit queue.  If the queue
        // is empty, we are done, otherwise we need to start transmitting the
        // next packet.
        //
        NS_ASSERT_MSG(m_txMachineState == BUSY, "Must be BUSY if transmitting");
        m_txMachineState = READY;

        NS_ASSERT_MSG(m_currentPkt != nullptr, "PointToPointSueNetDevice::TransmitComplete(): m_currentPkt zero");

        m_phyTxEndTrace(m_currentPkt);
        m_currentPkt = nullptr;

        // Add VC queue scheduling delay, then try to transmit next packet
        if (m_vcSchedulingDelay > NanoSeconds(0))
        {
            NS_LOG_DEBUG("Scheduling VC transmission with " << m_vcSchedulingDelay.GetNanoSeconds() << "ns delay");
            Simulator::Schedule(m_vcSchedulingDelay, &PointToPointSueNetDevice::TryTransmit, this);
        }
        else
        {
            TryTransmit();
        }
    }

    bool
    PointToPointSueNetDevice::Attach(Ptr<PointToPointSueChannel> ch)
    {
        NS_LOG_FUNCTION(this << &ch);

        m_channel = ch;

        m_channel->Attach(this);

        //
        // This device is up whenever it is attached to a channel.  A better plan
        // would be to have the link come up when both devices are attached, but this
        // is not done for now.
        //
        NotifyLinkUp();
        return true;
    }

    void
    PointToPointSueNetDevice::SetQueue(Ptr<Queue<Packet>> q)
    {
        NS_LOG_FUNCTION(this << q);
        m_queue = q;
    }

    void
    PointToPointSueNetDevice::SetReceiveErrorModel(Ptr<ErrorModel> em)
    {
        NS_LOG_FUNCTION(this << em);
        m_receiveErrorModel = em;
    }

    void
    PointToPointSueNetDevice::CreditReturn(Mac48Address targetMac, uint8_t vcId)
    {
        if (!m_enableLinkCBFC)
        {
            return;
        }
        // Check if credit records exist for the specified target MAC and VC
        auto macIt = m_rxCreditsToReturnMap.find(targetMac);
        if (macIt == m_rxCreditsToReturnMap.end())
        {
            NS_LOG_LOGIC("No credit records for target MAC: " << targetMac);
            return;
        }

        auto &vcMap = macIt->second;
        auto vcIt = vcMap.find(vcId);
        if (vcIt == vcMap.end())
        {
            NS_LOG_LOGIC("No credit records for VC " << static_cast<uint32_t>(vcId)
                                                     << " on target MAC: " << targetMac);
            return;
        }

        uint32_t creditsToSend = vcIt->second;

        // Check if batch sending conditions are met
        if (creditsToSend < m_creditBatchSize)
        {
            NS_LOG_LOGIC("Credits for VC " << static_cast<uint32_t>(vcId)
                                           << " are less than batch size (" << m_creditBatchSize << ")");
            return;
        }

        // Create credit packet
        EthernetHeader ethHeader;
        ethHeader.SetSource(GetLocalMac());
        ethHeader.SetDestination(targetMac);
        ethHeader.SetLengthType(0x0800);

        SueCbfcHeader creditHeader;
        creditHeader.SetVcId(vcId);
        creditHeader.SetCredits(creditsToSend);
        Ptr<Packet> creditPacket = Create<Packet>();

        creditPacket->AddHeader(ethHeader);
        creditPacket->AddHeader(creditHeader);

        NS_LOG_INFO("Node " << GetNode()->GetId() << " sending "
                            << creditsToSend << " credits to " << targetMac
                            << " for VC " << static_cast<uint32_t>(vcId));

        Simulator::Schedule(m_creditGenerateDelay, &PointToPointSueNetDevice::FindDeviceAndSend, this, creditPacket, targetMac, PROT_CBFC_UPDATE);

        vcIt->second = 0; // Reset credit count after successful transmission
    }

    void PointToPointSueNetDevice::FindDeviceAndSend(Ptr<Packet> packet, Mac48Address targetMac, uint16_t protocolNum)
    {
        // First check if it's credit to be returned to the peer device
        if (targetMac == GetRemoteMac())
        {
            Send(packet->Copy(), GetRemote(), protocolNum);
            return;
        }
        for (uint32_t i = 0; i < GetNode()->GetNDevices(); i++)
        {
            Ptr<NetDevice> dev = GetNode()->GetDevice(i);
            Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice>(dev);
            if (!p2pDev)
                continue;
            Mac48Address mac = Mac48Address::ConvertFrom(p2pDev->GetAddress());

            if (mac == targetMac)
            {
                // Send to target port
                // Add PPP header
                AddHeader(packet, PROT_CBFC_UPDATE);
                p2pDev->Receive(packet->Copy());
            }
        }
    }

    void PointToPointSueNetDevice::Receive(Ptr<Packet> packet)
    {
        if (!m_cbfcInitialized)
        {
            InitializeCbfc();
        }
        if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt(packet))
        {
            m_phyRxDropTrace(packet);
            return;
        }

        m_snifferTrace(packet);
        m_promiscSnifferTrace(packet);
        m_phyRxEndTrace(packet);
        Ptr<Packet> originalPacket = packet->Copy();

        PppHeader ppp;
        packet->PeekHeader(ppp);

        if (ppp.GetProtocol() == EtherToPpp(PROT_CBFC_UPDATE))
        { // If it's an update packet

            packet->RemoveHeader(ppp);
            SueCbfcHeader creditHeader;
            packet->RemoveHeader(creditHeader);
            EthernetHeader ethHeader;
            packet->RemoveHeader(ethHeader);

            uint8_t vcId = creditHeader.GetVcId();
            uint32_t credits = creditHeader.GetCredits();
            Mac48Address sourceMac = ethHeader.GetSource();

            // Do not count internal switch credit reception
            if (!IsMacSwitchDevice(GetLocalMac()) || !IsMacSwitchDevice(sourceMac))
            { // XPU or switch egress port
                Time processingTime = m_processingRate.CalculateBytesTxTime(originalPacket->GetSize());
                // Schedule processing completion event
                Simulator::Schedule(processingTime, &PointToPointSueNetDevice::ReceivePacketStatistic, this, originalPacket);
            }

            if (credits > 0)
            {
                m_txCreditsMap[sourceMac][vcId] += credits;
                if (!IsSwitchDevice())
                {
                    NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex() << "] received " << credits
                                              << " credits for VC " << (uint32_t)vcId
                                              << ". Total now: " << m_txCreditsMap[sourceMac][vcId]);
                }
            }
            return;
        }
        else
        { // If it's a data packet
            packet->RemoveHeader(ppp);
            SueCbfcHeader dataHeader;
            packet->RemoveHeader(dataHeader);
            uint8_t vcId = dataHeader.GetVcId();
            uint16_t protocol = PppToEther(ppp.GetProtocol());

            // Put into processing queue (check byte-level capacity)
            uint32_t packetSize = packet->GetSize();
            ProcessItem item = {originalPacket, packet, vcId, protocol};

            // Check byte-level capacity limit
            if (m_currentProcessingQueueBytes + packetSize <= m_processingQueueMaxBytes)
            {
                m_processingQueue.push(item);
                m_currentProcessingQueueSize++;
                m_currentProcessingQueueBytes += packetSize;
            }
            else
            {
                // Queue is full, drop packet
                m_vcDropCounts[vcId + 1]++; // Increase drop statistics

                if (!IsSwitchDevice())
                {
                    NS_LOG_INFO("Receive processing queue full! DROPPED packet on VC " << (uint32_t)vcId);
                }
                // TODO: Link-level retransmission
                m_phyRxDropTrace(packet);
                return;
            }

            if (!m_isProcessing)
            {
                m_isProcessing = true;
                StartProcessing();
            }
        }
    }

    // Set complete forwarding table
    void PointToPointSueNetDevice::SetForwardingTable(const std::map<Mac48Address, uint32_t> &table)
    {
        m_forwardingTable = table;
    }

    // Clear existing forwarding table
    void PointToPointSueNetDevice::ClearForwardingTable()
    {
        m_forwardingTable.clear();
    }

    void PointToPointSueNetDevice::StartProcessing()
    {
        if (m_processingQueue.empty())
        {
            m_isProcessing = false;
            return;
        }

        ProcessItem item = m_processingQueue.front();
        m_processingQueue.pop();
        m_currentProcessingQueueSize--;
        m_currentProcessingQueueBytes -= item.packet->GetSize();

        Time processingTime = m_processingRate.CalculateBytesTxTime(item.packet->GetSize());

        // Schedule processing completion event
        Simulator::Schedule(processingTime, &PointToPointSueNetDevice::CompleteProcessing, this, item);

        // Receive queue statistics
        Simulator::Schedule(processingTime, &PointToPointSueNetDevice::ReceivePacketStatistic, this, item.originalPacket);
    }

    void PointToPointSueNetDevice::CompleteProcessing(ProcessItem item)
    {
        // Actually process packet
        if (!m_promiscCallback.IsNull())
        {
            m_macPromiscRxTrace(item.originalPacket);
            m_promiscCallback(this, item.packet, item.protocol, GetRemote(), GetAddress(), NetDevice::PACKET_HOST);
        }

        // TODO Key part of switch two-port data transmission
        // Layer 2 forwarding logic
        EthernetHeader ethHeader;

        item.packet->PeekHeader(ethHeader);

        if (IsSwitchDevice() && !m_forwardingTable.empty())
        {
            // Extract destination MAC address from packet
            if (item.packet->PeekHeader(ethHeader))
            {
                Mac48Address destination = ethHeader.GetDestination();

                // Lookup in global forwarding table
                auto it = m_forwardingTable.find(destination);
                if (it != m_forwardingTable.end())
                {
                    uint32_t outPortIndex = it->second;
                    Ptr<Node> node = GetNode();

                    // Find device corresponding to the port on the node
                    for (uint32_t i = 0; i < node->GetNDevices(); i++)
                    {
                        Ptr<NetDevice> dev = node->GetDevice(i);
                        Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice>(dev);
                        // Check if conversion is successful
                        if (p2pDev && p2pDev->GetIfIndex() == outPortIndex)
                        {
                            // If current port is the egress port, directly enter VC queue
                            // Switch egress port: replace SourceDestination MAC with current device MAC only during TransmitStart
                            // If replaced with local MAC first, it's difficult to find the previous device's MAC later
                            if (GetIfIndex() == outPortIndex)
                            {
                                // This won't actually execute here, because ingress port directly puts data into egress port's VC queue
                                Send(item.packet->Copy(), destination, item.protocol);
                                // Queue operations have been completed in StartProcessing
                                HandleCreditReturn(ethHeader, item); // Only do CreditReturn when actually sent out
                            }
                            else
                            { // If current port is ingress port, hand over to egress port's receive queue
                                // When switch ingress port forwards, replace SourceDestination MAC with current device MAC
                                // to enable universal credit calculation based on SourceMac.
                                EthernetHeader ethTemp;
                                item.packet->RemoveHeader(ethTemp);
                                ethTemp.SetSource(GetLocalMac());
                                item.packet->AddHeader(ethTemp);

                                // Extract VC ID from packet
                                uint8_t vcId = ExtractVcIdFromPacket(item.packet);

                                Mac48Address mac = Mac48Address::ConvertFrom(p2pDev->GetAddress());
                                if (m_txCreditsMap[mac][vcId] > 0)
                                {
                                    if (m_enableLinkCBFC)
                                    {
                                        m_txCreditsMap[mac][vcId]--;
                                    }
                                    // Switch forwarding delay
                                    Simulator::Schedule(m_switchForwardDelay, &PointToPointSueNetDevice::SpecDevEnqueueToVcQueue, this, p2pDev, item.packet->Copy());
                                    HandleCreditReturn(ethHeader, item);
                                    // TODO delay to be set
                                    CreditReturn(ethHeader.GetSource(), item.vcId);
                                    // Queue operations have been completed in StartProcessing
                                }
                            }
                        }
                    }
                }
            }
        }
        else
        {
            // Non-switch device
            // Queue operations have been completed in StartProcessing
            m_macRxTrace(item.originalPacket);

            // Remove Ethernet header for easier reception
            EthernetHeader removeEthHeader;
            item.packet->RemoveHeader(removeEthHeader);

            m_rxCallback(this, item.packet, item.protocol, GetRemote());
            HandleCreditReturn(ethHeader, item);
            // TODO delay to be set currently: receiver is XPU and directly returns credits upon reception
            CreditReturn(ethHeader.GetSource(), item.vcId);
        }

        if (!IsSwitchDevice())
        {
            NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex() << "] processed data packet for VC " << (uint32_t)item.vcId
                                      << ", queuing " << m_rxCreditsToReturnMap[ethHeader.GetSource()][item.vcId] << " credit return");
        }

        // Immediately start processing next packet
        if (!m_processingQueue.empty())
        {
            StartProcessing();
        }
        else
        {
            m_isProcessing = false;
        }
    }

    void
    PointToPointSueNetDevice::SpecDevEnqueueToVcQueue(Ptr<PointToPointSueNetDevice> p2pDev, Ptr<Packet> packet)
    {
        p2pDev->EnqueueToVcQueue(packet);
    }

    bool
    PointToPointSueNetDevice::EnqueueToVcQueue(Ptr<Packet> packet)
    {
        if (!m_cbfcInitialized)
        {
            InitializeCbfc();
        }
        NS_LOG_FUNCTION(this << packet);

        // Extract VC ID from packet header
        uint8_t vcId = ExtractVcIdFromPacket(packet);
        NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                  << "] EnqueueToVcQueue extracted VC ID: " << (uint32_t)vcId);

        // CBFC Header
        SueCbfcHeader dataHeader;
        dataHeader.SetVcId(vcId);
        dataHeader.SetCredits(0);
        packet->AddHeader(dataHeader);
        // PPP Header
        AddHeader(packet, 0x0800);
        // Get source MAC to check if it's a forwarded packet
        Mac48Address sourceMac = GetSourceMac(packet);
        if (IsMacSwitchDevice(sourceMac))
        {
            m_rxCreditsToReturnMap[sourceMac][vcId]++;
        }

        m_macTxTrace(packet);

        // Check if queue is full
        if (!m_vcQueues[vcId]->Enqueue(packet))
        {
            NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                      << "] packet DROPPED (VC " << static_cast<uint32_t>(vcId) << " queue full: "
                                      << m_vcQueues[vcId]->GetNPackets() << "/"
                                      << m_vcQueues[vcId]->GetMaxSize().GetValue() << " packets)");
            m_macTxDropTrace(packet);
            return false;
        }

        NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                  << "] packet enqueued to VC " << static_cast<uint32_t>(vcId)
                                  << " (queue size now: " << m_vcQueues[vcId]->GetNPackets() << " packets)");

        // Schedule transmission attempt
        Simulator::Schedule(m_dataAddHeadDelay, &PointToPointSueNetDevice::TryTransmit, this);

        return true;
    }

    void
    PointToPointSueNetDevice::HandleCreditReturn(const EthernetHeader &ethHeader, const ProcessItem &item)
    {
        if (m_enableLinkCBFC)
        {
            // Increase credit count for corresponding source address and VC
            Mac48Address source = ethHeader.GetSource();
            uint16_t vcId = item.vcId;

            m_rxCreditsToReturnMap[source][vcId]++;
        }
    }

    Ptr<Queue<Packet>>
    PointToPointSueNetDevice::GetQueue(void) const
    {
        NS_LOG_FUNCTION(this);
        return m_queue;
    }

    uint32_t
    PointToPointSueNetDevice::GetVcQueueAvailableCapacity(uint8_t vcId)
    {
        NS_LOG_FUNCTION(this << static_cast<uint32_t>(vcId));
        
        if (vcId >= m_numVcs)
        {
            NS_LOG_WARN("Invalid VC ID: " << static_cast<uint32_t>(vcId));
            return 0;
        }
        
        // Check if VC queue pointer is valid, if invalid then not initialized yet
        if (!m_vcQueues[vcId])
        {
            return m_vcQueueMaxBytes;
        }
        
        // Get current VC queue used bytes
        uint32_t currentBytes = m_vcQueues[vcId]->GetNBytes();
        // Get VC queue maximum capacity
        uint32_t maxBytes = m_vcQueueMaxBytes;
        // Get reserved capacity
        uint32_t reservedBytes = m_vcReservedCapacity[vcId];
        
        // Calculate actual available capacity (total capacity - used - reserved)
        uint32_t usedBytes = currentBytes + reservedBytes;
        
        // Return available capacity (bytes)
        if (usedBytes >= maxBytes)
        {
            return 0;
        }
        
        return maxBytes - usedBytes;
    }

    bool
    PointToPointSueNetDevice::ReserveVcCapacity(uint8_t vcId, uint32_t amount)
    {
        NS_LOG_FUNCTION(this << static_cast<uint32_t>(vcId) << amount);
        
        if (vcId >= m_numVcs)
        {
            NS_LOG_WARN("Invalid VC ID: " << static_cast<uint32_t>(vcId));
            return false;
        }
        
        // Check if there's enough available capacity for reservation
        // Need to reserve packet size plus additional header size
        uint32_t totalReservationSize = amount + m_additionalHeaderSize;
        uint32_t availableCapacity = GetVcQueueAvailableCapacity(vcId);
        
        if (availableCapacity >= totalReservationSize)
        {
            // Reserve capacity (including packet size and additional header size)
            m_vcReservedCapacity[vcId] += totalReservationSize;
            NS_LOG_DEBUG("Reserved " << totalReservationSize << " bytes for VC" 
                         << static_cast<uint32_t>(vcId) << " (packet: " << amount 
                         << ", headers: " << m_additionalHeaderSize 
                         << "), total reserved: " << m_vcReservedCapacity[vcId]);
            return true;
        }
        
        NS_LOG_DEBUG("Failed to reserve " << totalReservationSize << " bytes for VC" 
                     << static_cast<uint32_t>(vcId) << " (packet: " << amount 
                     << ", headers: " << m_additionalHeaderSize 
                     << "), available: " << availableCapacity);
        return false;
    }

    void
    PointToPointSueNetDevice::ReleaseVcCapacity(uint8_t vcId, uint32_t amount)
    {
        NS_LOG_FUNCTION(this << static_cast<uint32_t>(vcId) << amount);
        
        if (vcId >= m_numVcs)
        {
            NS_LOG_WARN("Invalid VC ID: " << static_cast<uint32_t>(vcId));
            return;
        }
        
        // Releasing capacity also needs to include additional header size
        uint32_t totalReleaseSize = amount + m_additionalHeaderSize;
        
        // Prevent releasing more capacity than reserved
        if (m_vcReservedCapacity[vcId] >= totalReleaseSize)
        {
            m_vcReservedCapacity[vcId] -= totalReleaseSize;
        }
        else
        {
            NS_LOG_WARN("Attempting to release more capacity than reserved for VC" 
                        << static_cast<uint32_t>(vcId) << ", reserved: " 
                        << m_vcReservedCapacity[vcId] << ", attempting to release: " 
                        << totalReleaseSize);
            m_vcReservedCapacity[vcId] = 0;
        }
        
        NS_LOG_DEBUG("Released " << totalReleaseSize << " bytes for VC" 
                     << static_cast<uint32_t>(vcId) << " (packet: " << amount 
                     << ", headers: " << m_additionalHeaderSize 
                     << "), total reserved: " << m_vcReservedCapacity[vcId]);
    }

    void
    PointToPointSueNetDevice::NotifyLinkUp(void)
    {
        NS_LOG_FUNCTION(this);
        m_linkUp = true;
        m_linkChangeCallbacks();
    }

    void
    PointToPointSueNetDevice::SetIfIndex(const uint32_t index)
    {
        NS_LOG_FUNCTION(this);
        m_ifIndex = index;
    }

    uint32_t
    PointToPointSueNetDevice::GetIfIndex(void) const
    {
        return m_ifIndex;
    }

    Ptr<Channel>
    PointToPointSueNetDevice::GetChannel(void) const
    {
        return m_channel;
    }

    //
    // This is a point-to-point device, so we really don't need any kind of address
    // information.  However, the base class NetDevice wants us to define the
    // methods to get and set the address.  Rather than be rude and assert, we let
    // clients get and set the address, but simply ignore them.

    void
    PointToPointSueNetDevice::SetAddress(Address address)
    {
        NS_LOG_FUNCTION(this << address);
        m_address = Mac48Address::ConvertFrom(address);
    }

    Address
    PointToPointSueNetDevice::GetAddress(void) const
    {
        return m_address;
    }

    bool
    PointToPointSueNetDevice::IsLinkUp(void) const
    {
        NS_LOG_FUNCTION(this);
        return m_linkUp;
    }

    void
    PointToPointSueNetDevice::AddLinkChangeCallback(Callback<void> callback)
    {
        NS_LOG_FUNCTION(this);
        m_linkChangeCallbacks.ConnectWithoutContext(callback);
    }

    //
    // This is a point-to-point device, so every transmission is a broadcast to
    // all of the devices on the network.
    //
    bool
    PointToPointSueNetDevice::IsBroadcast(void) const
    {
        NS_LOG_FUNCTION(this);
        return true;
    }

    //
    // We don't really need any addressing information since this is a
    // point-to-point device.  The base class NetDevice wants us to return a
    // broadcast address, so we make up something reasonable.
    //
    Address
    PointToPointSueNetDevice::GetBroadcast(void) const
    {
        NS_LOG_FUNCTION(this);
        return Mac48Address("ff:ff:ff:ff:ff:ff");
    }

    bool
    PointToPointSueNetDevice::IsMulticast(void) const
    {
        NS_LOG_FUNCTION(this);
        return true;
    }

    Address
    PointToPointSueNetDevice::GetMulticast(Ipv4Address multicastGroup) const
    {
        NS_LOG_FUNCTION(this);
        return Mac48Address("01:00:5e:00:00:00");
    }

    Address
    PointToPointSueNetDevice::GetMulticast(Ipv6Address addr) const
    {
        NS_LOG_FUNCTION(this << addr);
        return Mac48Address("33:33:00:00:00:00");
    }

    bool
    PointToPointSueNetDevice::IsPointToPoint(void) const
    {
        NS_LOG_FUNCTION(this);
        return true;
    }

    bool
    PointToPointSueNetDevice::IsBridge(void) const
    {
        NS_LOG_FUNCTION(this);
        return false;
    }

    bool
    PointToPointSueNetDevice::Send(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
    {
        if (IsLinkUp() == false)
        {
            m_macTxDropTrace(packet);
            return false;
        }
        if (!m_cbfcInitialized)
        {
            InitializeCbfc();
        }

        // Credit update packets enter high-priority main queue
        if (protocolNumber == PROT_CBFC_UPDATE)
        {
            // Credit packet structure - only CBFC header, add PPP header below
            // PPP Header
            AddHeader(packet, protocolNumber);
            if (!m_queue->Enqueue(packet))
            {
                // Record VC0 packet drops when queue is full
                m_vcDropCounts_SendQ[0]++;
                m_totalPacketDropNum += 1;
                if (!IsSwitchDevice())
                {
                    NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                              << "] credit packet DROPPED (main queue full: "
                                              << m_queue->GetNPackets() << "/"
                                              << m_queue->GetMaxSize().GetValue() << " packets)");
                }

                m_macTxDropTrace(packet);
                return false;
            }
            if (!IsSwitchDevice())
            {
                NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                          << "] credit packet enqueued to main queue"
                                          << " (size now: " << m_queue->GetNPackets() << " packets)");
            }
            // Delay is between enqueue and transmission
            Simulator::Schedule(m_creUpdateAddHeadDelay, &PointToPointSueNetDevice::TryTransmit, this);
        }
        else
        {
            if (!IsSwitchDevice())
            { // Add EthernetHeader when XPU device sends
                // Header processing logic: get destination IP from IPv4 header, add EthernetHeader
                // Packet structure: SUEHeader | UDP | IPv4 | Ethernet | CBFC | PPP

                // Extract destination IP from packet
                Ipv4Address destIp = ExtractDestIpFromPacket(packet);

                // Query destination MAC address
                Mac48Address destMac = GetMacForIp(destIp);

                // Add Ethernet header
                AddEthernetHeader(packet, destMac);

                NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                          << "] added EthernetHeader for IP " << destIp << " -> MAC " << destMac);
            }

            // Data packets enter corresponding VC queues
            EnqueueToVcQueue(packet); // EnqueueToVcQueue will re-extract vcId internally
        }

        return true;
    }

    bool
    PointToPointSueNetDevice::SendFrom(Ptr<Packet> packet,
                                       const Address &source,
                                       const Address &dest,
                                       uint16_t protocolNumber)
    {
        NS_LOG_FUNCTION(this << packet << source << dest << protocolNumber);
        return false;
    }

    Ptr<Node>
    PointToPointSueNetDevice::GetNode(void) const
    {
        return m_node;
    }

    void
    PointToPointSueNetDevice::SetNode(Ptr<Node> node)
    {
        NS_LOG_FUNCTION(this);
        m_node = node;
    }

    bool
    PointToPointSueNetDevice::NeedsArp(void) const
    {
        NS_LOG_FUNCTION(this);
        return false;
    }

    void
    PointToPointSueNetDevice::SetReceiveCallback(NetDevice::ReceiveCallback cb)
    {
        m_rxCallback = cb;
    }

    void
    PointToPointSueNetDevice::SetPromiscReceiveCallback(NetDevice::PromiscReceiveCallback cb)
    {
        m_promiscCallback = cb;
    }

    bool
    PointToPointSueNetDevice::SupportsSendFrom(void) const
    {
        NS_LOG_FUNCTION(this);
        return false;
    }

    void
    PointToPointSueNetDevice::DoMpiReceive(Ptr<Packet> p)
    {
        NS_LOG_FUNCTION(this << p);
        Receive(p);
    }

    Address
    PointToPointSueNetDevice::GetRemote(void) const
    {
        NS_LOG_FUNCTION(this);
        NS_ASSERT(m_channel->GetNDevices() == 2);
        for (std::size_t i = 0; i < m_channel->GetNDevices(); ++i)
        {
            Ptr<NetDevice> tmp = m_channel->GetDevice(i);
            if (tmp != this)
            {
                return tmp->GetAddress();
            }
        }
        NS_ASSERT(false);
        // quiet compiler.
        return Address();
    }

    bool
    PointToPointSueNetDevice::SetMtu(uint16_t mtu)
    {
        NS_LOG_FUNCTION(this << mtu);
        m_mtu = mtu;
        return true;
    }

    uint16_t
    PointToPointSueNetDevice::GetMtu(void) const
    {
        NS_LOG_FUNCTION(this);
        return m_mtu;
    }

    uint16_t
    PointToPointSueNetDevice::PppToEther(uint16_t proto)
    {
        NS_LOG_FUNCTION_NOARGS();
        switch (proto)
        {
        case 0x0021:
            return 0x0800; // IPv4
        case 0x0057:
            return 0x86DD; // IPv6
        case 0x00FB:
            return PROT_CBFC_UPDATE; // CBFC Update
        default:
            NS_ASSERT_MSG(false, "PPP Protocol number not defined!");
        }
        return 0;
    }

    uint16_t
    PointToPointSueNetDevice::EtherToPpp(uint16_t proto)
    {
        NS_LOG_FUNCTION_NOARGS();
        switch (proto)
        {
        case 0x0800:
            return 0x0021; // IPv4
        case 0x86DD:
            return 0x0057; // IPv6
        case PROT_CBFC_UPDATE:
            return 0x00FB; // CBFC Update
        default:
            NS_ASSERT_MSG(false, "PPP Protocol number not defined!");
        }
        return 0;
    }

    // Packet header processing method implementation
    uint8_t PointToPointSueNetDevice::ExtractVcIdFromPacket(Ptr<const Packet> packet)
    {
        Packet p = *packet;

        try
        {
            // First check if there's a PPP header
            PppHeader ppp;
            bool hasPppHeader = true;

            // Try to Peek PPP header to check protocol number
            try
            {
                p.PeekHeader(ppp);
                uint16_t protocol = ppp.GetProtocol();

                // Check if protocol number is valid
                if (!protocol)
                {
                    // Protocol number is 0 or invalid, indicating it's not a PPP header
                    hasPppHeader = false;
                }
            }
            catch (...)
            {
                // Not enough data to form PPP header
                hasPppHeader = false;
            }

            if (hasPppHeader)
            {
                // Packet structure: PPP + CBFC + Ethernet + IPv4 + UDP + SueHeader(data packet)
                // Or: PPP + CBFC + Ethernet(update packet)
                p.RemoveHeader(ppp);

                SueCbfcHeader cbfcHeader;
                p.RemoveHeader(cbfcHeader);

                EthernetHeader eth;
                p.RemoveHeader(eth);

                // Check if it's a credit packet
                if (cbfcHeader.GetCredits() > 0)
                {
                    // Credit packet: vcId is in CBFC header
                    return cbfcHeader.GetVcId();
                }
                else
                {
                    // Data packet: continue parsing to get vcId from SUE header
                    Ipv4Header ipv4;
                    p.RemoveHeader(ipv4);

                    UdpHeader udp;
                    p.RemoveHeader(udp);

                    SueHeader sueHeader;
                    p.RemoveHeader(sueHeader);
                    return sueHeader.GetVc();
                }
            }
            else
            {
                // Packet structure: Ethernet + IPv4 + UDP + SueHeader
                EthernetHeader eth;
                p.RemoveHeader(eth);

                Ipv4Header ipv4;
                p.RemoveHeader(ipv4);

                UdpHeader udp;
                p.RemoveHeader(udp);

                SueHeader sueHeader;
                p.RemoveHeader(sueHeader);
                return sueHeader.GetVc();
            }
        }
        catch (...)
        {
            NS_LOG_WARN("Failed to extract VC ID from packet");
            return 0; // Default VC
        }
    }

    Ipv4Address PointToPointSueNetDevice::ExtractDestIpFromPacket(Ptr<Packet> packet)
    {
        Packet p = *packet;

        try
        {
            // 292(256 + SueHeader(RH) + UdpHeader + Ipv4Header)
            // Get destination address from IPv4 header
            Ipv4Header ipv4;
            p.RemoveHeader(ipv4);
            return ipv4.GetDestination();
        }
        catch (...)
        {
            NS_LOG_WARN("Failed to extract destination IP from packet");
            return Ipv4Address::GetAny();
        }
    }

    void PointToPointSueNetDevice::AddEthernetHeader(Ptr<Packet> packet, Mac48Address destMac)
    {
        EthernetHeader ethHeader;
        ethHeader.SetSource(m_address);
        ethHeader.SetDestination(destMac);
        ethHeader.SetLengthType(0x0800); // IPv4
        packet->AddHeader(ethHeader);
    }

    // Static method: Set global IP-MAC mapping table
    void PointToPointSueNetDevice::SetGlobalIpMacMap(const std::map<Ipv4Address, Mac48Address> &map)
    {
        s_ipToMacMap = map;
        NS_LOG_INFO("SetGlobalIpMacMap - added " << map.size() << " IP-MAC entries");
    }

    // Static method: Get MAC address for IP address (lookup table approach)
    Mac48Address PointToPointSueNetDevice::GetMacForIp(Ipv4Address ip)
    {
        // std::cout << ip << std::endl;
        auto it = s_ipToMacMap.find(ip);
        if (it != s_ipToMacMap.end())
        {
            return it->second;
        }

        NS_LOG_WARN("GetMacForIp - could not find MAC for IP: " << ip << ", returning broadcast");
        return Mac48Address::GetBroadcast();
    }

    // Queue monitoring method implementation
    void PointToPointSueNetDevice::LogDeviceQueueUsage() {
        if (!m_loggingEnabled) {
            return;
        }

        uint64_t timeNs = Simulator::Now().GetNanoSeconds();
        uint32_t xpuId = GetNode()->GetId() + 1;
        uint32_t deviceId = GetIfIndex();

        // Log main queue usage
        uint32_t mainQueueSize = 0;
        uint32_t mainQueueMaxSize = m_queue->GetMaxSize().GetValue(); // Default queue size limit (bytes)
        if (m_queue) {
            // Try to get queue bytes, if not supported then estimate with packet count
            mainQueueSize = m_queue->GetNBytes();
        }

        // Log VC queue usage
        std::map<uint8_t, uint32_t> vcQueueSizes;
        std::map<uint8_t, uint32_t> vcQueueMaxSizes;
        for (const auto& vcPair : m_vcQueues) {
            uint8_t vcId = vcPair.first;
            Ptr<Queue<Packet>> vcQueue = vcPair.second;
            if (vcQueue) {
                // Get VC queue bytes
                vcQueueSizes[vcId] = vcQueue->GetNBytes();
                // Use actual VC queue maximum capacity configuration (bytes)
                vcQueueMaxSizes[vcId] = m_vcQueueMaxBytes; // Use parameterized VC queue size limit (bytes)
            }
        }

        // Log to PerformanceLogger
        PerformanceLogger::GetInstance().LogDeviceQueueUsage(
            timeNs, xpuId, deviceId, mainQueueSize, mainQueueMaxSize,
            vcQueueSizes, vcQueueMaxSizes);

        // Log link layer processing queue usage
        uint32_t processingQueueSize = m_currentProcessingQueueBytes;
        uint32_t processingQueueMaxSize = m_processingQueueMaxBytes;
        PerformanceLogger::GetInstance().LogProcessingQueueUsage(
            timeNs, xpuId, deviceId, processingQueueSize, processingQueueMaxSize);
    }

} // namespace ns3
