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
#include "sue-cbfc.h"
#include "sue-queue-manager.h"
#include "ns3/network-module.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/udp-header.h"
#include "ns3/ethernet-header.h"
#include "sue-ppp-header.h"
#include "ns3/performance-logger.h"
#include "sue-tag.h"
#include "sue-utils.h"

namespace ns3 {

    NS_LOG_COMPONENT_DEFINE("PointToPointSueNetDevice");

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
                                //LLR
                                .AddAttribute("EnableLLR",
                                            "If enable LLR.",
                                            BooleanValue(false),
                                            MakeBooleanAccessor(&PointToPointSueNetDevice::m_llrEnabled),
                                            MakeBooleanChecker())
                                .AddAttribute("LlrTimeout",
                                            "LLR timeout value.",
                                            TimeValue(NanoSeconds(1000)),
                                            MakeTimeAccessor(&PointToPointSueNetDevice::m_llrTimeout),
                                            MakeTimeChecker())
                                .AddAttribute("LlrWindowSize",
                                            "LLR window size.",
                                            UintegerValue(10),
                                            MakeUintegerAccessor(&PointToPointSueNetDevice::m_llrWindowSize),
                                            MakeUintegerChecker<uint32_t>(1, 100))
                                .AddAttribute("AckAddHeaderDelay",
                                            "ACK/NACK header adding delay",
                                            TimeValue(NanoSeconds(10)),
                                            MakeTimeAccessor(&PointToPointSueNetDevice::m_AckAddHeaderDelay),
                                            MakeTimeChecker())
                                .AddAttribute("AckProcessDelay",
                                            "ACK/NACK processing delay",
                                            TimeValue(NanoSeconds(10)),
                                            MakeTimeAccessor(&PointToPointSueNetDevice::m_AckProcessDelay),
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

    
    PointToPointSueNetDevice::PointToPointSueNetDevice()
        : m_txMachineState(READY),
          m_channel(0),
          m_linkUp(false),
          m_currentPkt(0),
          // CBFC configuration values (in header file order)
          m_initialCredits(0),
          m_numVcs(0),
          m_creditBatchSize(10),
          m_vcQueueMaxBytes(2 * 1024 * 1024), // Default VC queue max capacity 2MB (2*1024*1024 bytes)
          m_additionalHeaderSize(46), // Default 46 bytes
          m_enableLinkCBFC(false),
          m_currentProcessingQueueSize(0),
          m_currentProcessingQueueBytes(0),
          m_isProcessing(false),
          m_processingDelay(NanoSeconds(10)),
          m_processingQueueMaxBytes(2 * 1024 * 1024), // Default processing queue max capacity 2MB
          m_linkStatInterval(MilliSeconds(10)),
          m_totalPacketDropNum(0),
          m_creUpdateAddHeadDelay(NanoSeconds(3)),
          m_dataAddHeadDelay(NanoSeconds(5)),
          m_creditGenerateDelay(NanoSeconds(10)),
          m_switchForwardDelay(NanoSeconds(150)),
          m_vcSchedulingDelay(NanoSeconds(8)),
          m_loggingEnabled(true), // Enable logging by default
          m_processingRate(m_bps), // Default same as transmission rate
          m_processingRateString("200Gbps"), // Default processing rate string
          m_linkStatIntervalString("10us"), // Default link stat interval string
          //LLR
          m_llrEnabled(false),
          m_llrWindowSize(10),
          m_llrTimeout(NanoSeconds(10000)),
          m_AckAddHeaderDelay(NanoSeconds(10)),
          m_AckProcessDelay(NanoSeconds(10))
    {
        NS_LOG_FUNCTION(this);

        // Initialize CBFC manager
        m_cbfcManager = CreateObject<CbfcManager> ();

        // Initialize queue manager
        m_queueManager = CreateObject<SueQueueManager> ();

        // Initialize switch module
        m_switch = CreateObject<SueSwitch> ();

        // Initialize LLR managers
        m_llrNodeManager = CreateObject<LlrNodeManager> ();
        m_llrSwitchPortManager = CreateObject<LlrSwitchPortManager> ();

        // Initialize TryTransmit event tracking
        m_tryTransmitEvent = EventId();
        m_tryTransmitScheduled = false;
    }

    PointToPointSueNetDevice::~PointToPointSueNetDevice()
    {
        NS_LOG_FUNCTION(this);
    }

    // Initialize CBFC functionality
    void
    PointToPointSueNetDevice::InitializeCbfc()
    {
        if (m_cbfcManager->IsInitialized())
            return;

        // Convert strings using SueStringUtils
        m_processingRate = SueStringUtils::ParseDataRateString(m_processingRateString);
        m_linkStatInterval = SueStringUtils::ParseTimeIntervalString(m_linkStatIntervalString);

        // Initialize CBFC with configuration, callbacks, and peer device credits
        m_cbfcManager->Initialize (
            m_numVcs, m_initialCredits, m_enableLinkCBFC,
            m_creditBatchSize,
            [this]() { return GetLocalMac(); },                    // GetLocalMac callback
            [this]() { return GetNode(); },                        // GetNode callback
            [this](Ptr<Packet> packet, Mac48Address targetMac, uint16_t protocolNum) {
                FindDeviceAndSend(packet, targetMac, protocolNum); // SendPacket callback
            },
            m_creditGenerateDelay,                                 // Credit generation delay
            SuePacketUtils::PROT_CBFC_UPDATE,                                      // Protocol number
            [this]() { return GetRemoteMac(); },           // GetRemoteMac callback
            [this]() { return IsSwitchDevice(m_address); },        // IsSwitchDevice callback
            85                                              // Switch default credits
        );

        // Initialize queue manager directly with drop callback
        m_queueManager->Initialize(m_numVcs, m_vcQueueMaxBytes, m_additionalHeaderSize,
                                 MakeCallback(&PointToPointSueNetDevice::HandlePacketDrop, this));

        if (!m_switch || !IsSwitchDevice(m_address))
        {
            NS_LOG_INFO("Link: Initialized on Node " << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                                     << " with " << (uint32_t)m_numVcs << " VCs and " << m_initialCredits << " initial credits.");
        }
        // Start statistics after initialization completes
        m_lastStatTime = Simulator::Now();
    }

    void
    PointToPointSueNetDevice::InitializeLlr()
    {
        if (!m_llrEnabled)
            return;

        // Check if this is a switch device
        bool isSwitchDevice = m_switch && IsSwitchDevice(m_address);

        if (isSwitchDevice)
        {
            // Initialize LLR switch port manager
            Mac48Address peerMac = GetRemoteMac();
            m_llrSwitchPortManager->Initialize(
                m_llrEnabled,
                m_llrWindowSize,
                m_llrTimeout,
                m_AckAddHeaderDelay,
                m_AckProcessDelay,
                SuePacketUtils::ACK_REV,
                m_numVcs,
                [this]() { return GetLocalMac(); },                    // GetLocalMac callback
                [this]() { return GetNode(); },                        // GetNode callback
                [this]() { return m_switch; },                          // GetSwitch callback
                [this](Ptr<Packet> packet, Mac48Address targetMac, uint16_t protocolNum) {
                    FindDeviceAndSend(packet, targetMac, protocolNum); // SendPacket callback
                },
                [this]() {
                    // TryTransmit callback - trigger transmission attempt
                    if (m_currentPkt && m_txMachineState == READY) {
                        TransmitStart(m_currentPkt);
                    }
                },
                peerMac                                                 // Connected peer MAC
            );
        }
        else
        {
            // Initialize LLR node manager for regular NICs
            m_llrNodeManager->Initialize(
                m_llrEnabled,
                m_llrWindowSize,
                m_llrTimeout,
                m_AckAddHeaderDelay,
                m_AckProcessDelay,
                SuePacketUtils::ACK_REV,
                m_numVcs,
                [this]() { return GetLocalMac(); },                    // GetLocalMac callback
                [this]() { return GetNode(); },                        // GetNode callback
                [this]() { return GetRemoteMac(); },                   // GetRemoteMac callback
                [this](Ptr<Packet> packet, Mac48Address targetMac, uint16_t protocolNum) {
                    FindDeviceAndSend(packet, targetMac, protocolNum); // SendPacket callback
                },
                [this]() {
                    // TryTransmit callback - trigger transmission attempt
                    if (m_currentPkt && m_txMachineState == READY) {
                        TransmitStart(m_currentPkt);
                    }
                }
            );
        }
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
        SueStatsUtils::ProcessPacketDropStats(droppedPacket, GetNode()->GetId(), GetIfIndex() - 1, "VCQueueFull");
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

        // Reconfigure CBFC manager with new queue size parameters
        SueConfigUtils::ReconfigureCbfcWithQueueSize(m_cbfcManager, m_numVcs,
                                                     m_initialCredits, m_enableLinkCBFC,
                                                     m_creditBatchSize);
    }

    uint32_t PointToPointSueNetDevice::GetVcQueueMaxBytes(void) const
    {
        return m_vcQueueMaxBytes;
    }

    
    // Add sequence number to PPP header, modify accordingly
    void
    PointToPointSueNetDevice::AddHeader (Ptr<Packet> p, uint16_t protocolNumber)
    {
        NS_LOG_FUNCTION (this << p << protocolNumber);
        SuePppHeader ppp;
        ppp.SetProtocol(SuePacketUtils::EtherToPpp(protocolNumber));
        p->AddHeader(ppp);
    }

    bool
    PointToPointSueNetDevice::ProcessHeader(Ptr<Packet> p, uint16_t& protocol)
    {
        NS_LOG_FUNCTION(this << p << protocol);
        SuePppHeader ppp;
        p->RemoveHeader(ppp);
        protocol = SuePacketUtils::PppToEther(ppp.GetProtocol());

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
        if (!IsSwitchDevice(m_address))
        {
            SueTag::UpdateTimestampInPacket(p, Simulator::Now());
            NS_LOG_DEBUG("Updated SUE tag timestamp for packet UID " << p->GetUid()
                        << " at time " << Simulator::Now().GetNanoSeconds() << "ns");
        }

        Time txTime = m_bps.CalculateBytesTxTime(p->GetSize());
        Time txCompleteTime = txTime + m_tInterframeGap;

        NS_LOG_LOGIC("Schedule TransmitCompleteEvent in " << txCompleteTime.As(Time::S));
        Simulator::Schedule(txCompleteTime, &PointToPointSueNetDevice::TransmitComplete, this);

        Simulator::Schedule(txCompleteTime, [this, p]() {
            SueStatsUtils::ProcessSentPacketStats(p, m_vcBytesSent, GetNode()->GetId(), GetIfIndex() - 1);
        });

        // Switch egress port: credit return only after packet transmission
        SuePppHeader ppp;
        p->PeekHeader(ppp);

        if (IsSwitchDevice(m_address) && ppp.GetProtocol() != SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_UPDATE))
        {
            // Extract VC ID from packet
            uint8_t vcId = SuePacketUtils::ExtractVcIdFromPacket(p);

            // Switch egress port: replace Source Destination MAC with current device MAC
            // to enable universal credit calculation based on SourceMac.
            Mac48Address targetMac = SuePacketUtils::ExtractSourceMac(p, true, GetLocalMac());

            Simulator::Schedule(txCompleteTime, [this, targetMac, vcId]() {
                if (m_cbfcManager) {
                    EthernetHeader tempEthHeader;
                    tempEthHeader.SetSource(targetMac);                   
                    m_cbfcManager->HandleCreditReturn(tempEthHeader, vcId);
                    m_cbfcManager->CreditReturn(targetMac, vcId);
                }
            });
        }

        bool result = m_channel->TransmitStart(p, this, txTime);
        if (result == false)
        {
            m_phyTxDropTrace(p);
            // TODO: Link-level retransmission
        }
        return result;
    }

    
    
  
    // Core function to check all queues and trigger transmission
    void
    PointToPointSueNetDevice::TryTransmit()
    {
        // Reset the scheduled flag when actually executing
        m_tryTransmitScheduled = false;

        if (m_txMachineState != READY)
        {
            return;
        }

        // 1. Prioritize checking high-priority main queue (for credit packets)
        if (!m_queue->IsEmpty())
        {
            Ptr<Packet> packet = m_queue->Dequeue();
            SuePppHeader ppp;
            packet->PeekHeader(ppp);

            // Trigger main queue statistics (event-driven after main queue dequeue)
            SueStatsUtils::ProcessMainQueueStats(m_queue, GetNode()->GetId(), GetIfIndex() - 1);

            if ((!IsSwitchDevice(m_address)) && ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_UPDATE))
            {
                NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                          << "] sending credit packet from main queue"
                                          << " (main queue size now: " << m_queue->GetNPackets() << " packets)");
            }
            else if ((!IsSwitchDevice(m_address)) && ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::ACK_REV)) {
                NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                        << "] sending ACK packet from main queue"
                        << " (main queue size now: " << m_queue->GetNPackets() << " packets)");
            }
            else {
                NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                        << "] sending NACK packet from main queue"
                        << " (main queue size now: " << m_queue->GetNPackets() << " packets)");
            } // Add ACK/NACK logging

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
                if (m_queueManager && !m_queueManager->IsVcQueueEmpty(currentVC) && m_cbfcManager->GetTxCredits(GetRemoteMac(), currentVC) > 0)
                {
                    if (m_enableLinkCBFC)
                    {
                        m_cbfcManager->DecrementTxCredits(GetRemoteMac(), currentVC);
                        SueStatsUtils::ProcessCreditChangeStats(GetRemoteMac(), currentVC, m_cbfcManager->GetTxCredits(GetRemoteMac(), currentVC), GetNode()->GetId(), GetIfIndex() - 1);
                    }
                    Ptr<Packet> packet = m_queueManager->DequeueFromVcQueue(currentVC);

                        // Trigger VC queue statistics (event-driven after VC dequeue)
                        SueStatsUtils::ProcessVCQueueStats(m_queueManager, m_cbfcManager,
                                                         m_numVcs, m_vcQueueMaxBytes,
                                                         GetNode()->GetId(), GetIfIndex() - 1);

                        if (!IsSwitchDevice(m_address))
                        {
                            NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex() << "] sending packet for VC " << (uint32_t)currentVC
                                                    << ". Credits left: " << m_cbfcManager->GetTxCredits(GetRemoteMac(), currentVC)
                                                    << " (VC queue size now: " << m_queueManager->GetVcQueueSize(currentVC) << " packets)");
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
            // Schedule TryTransmit event if not already scheduled
            if (!m_tryTransmitScheduled)
            {
                m_tryTransmitEvent = Simulator::Schedule(m_vcSchedulingDelay, &PointToPointSueNetDevice::TryTransmit, this);
                m_tryTransmitScheduled = true;
            }
        }
        else
        {
            // Schedule TryTransmit event if not already scheduled
            if (!m_tryTransmitScheduled)
            {
                m_tryTransmitEvent = Simulator::ScheduleNow(&PointToPointSueNetDevice::TryTransmit, this);
                m_tryTransmitScheduled = true;
            }
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
                AddHeader(packet, SuePacketUtils::PROT_CBFC_UPDATE);
                p2pDev->Receive(packet->Copy());
            }
        }
    }

    void PointToPointSueNetDevice::Receive(Ptr<Packet> packet)
    {
        if (!m_cbfcManager || !m_cbfcManager->IsInitialized())
        {
            InitializeCbfc();
        }
        // Initialize LLR if enabled
        if (m_llrEnabled)
        {
            InitializeLlr();
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

        SuePppHeader ppp;
        packet->PeekHeader(ppp);

        if(m_llrEnabled){
                // Received ACK packet
            if(ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::ACK_REV)){
                if(m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager){
                    Simulator::Schedule(m_AckProcessDelay, &LlrSwitchPortManager::ProcessLlrAck, m_llrSwitchPortManager, packet);
                } else if((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager){
                    Simulator::Schedule(m_AckProcessDelay, &LlrNodeManager::ProcessLlrAck, m_llrNodeManager, packet);
                }
                return;
            }

            // Received NACK packet
            if(ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::NACK_REV)){
                if(m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager){
                    Simulator::Schedule(m_AckProcessDelay, &LlrSwitchPortManager::ProcessLlrNack, m_llrSwitchPortManager, packet);
                } else if((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager){
                    Simulator::Schedule(m_AckProcessDelay, &LlrNodeManager::ProcessLlrNack, m_llrNodeManager, packet);
                }
                return;
            }
        }


        if (ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_UPDATE))
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
            if (!IsSwitchDevice(GetLocalMac()) || !IsSwitchDevice(sourceMac))
            { // XPU or switch egress port
                Time processingTime = m_processingRate.CalculateBytesTxTime(originalPacket->GetSize());
                // Schedule processing completion event
                Simulator::Schedule(processingTime, [this, originalPacket]() {
                    SueStatsUtils::ProcessReceivedPacketStats(originalPacket, m_vcBytesReceived, GetNode()->GetId(), GetIfIndex() - 1);
                });
            }

            if (credits > 0)
            {
                if (m_cbfcManager)
                {
                    m_cbfcManager->AddTxCredits(sourceMac, vcId, credits);
                    SueStatsUtils::ProcessCreditChangeStats(sourceMac, vcId, m_cbfcManager->GetTxCredits(sourceMac, vcId), GetNode()->GetId(), GetIfIndex() - 1);
                    if (!IsSwitchDevice(m_address))
                    {
                        NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex() << "] received " << credits
                                                  << " credits for VC " << (uint32_t)vcId
                                                  << ". Total now: " << m_cbfcManager->GetTxCredits(sourceMac, vcId));
                    }

                    // Check if TryTransmit is not already scheduled, then schedule it
                    if (!m_tryTransmitScheduled)
                    {
                        m_tryTransmitEvent = Simulator::ScheduleNow(&PointToPointSueNetDevice::TryTransmit, this);
                        m_tryTransmitScheduled = true;
                    }
                }
            }
            return;
        }
        else
        { // If it's a data packet
            packet->RemoveHeader(ppp);

            // Extract VC ID from packet
            uint8_t vcId = SuePacketUtils::ExtractVcIdFromPacket(packet);
            uint16_t protocol = SuePacketUtils::PppToEther(ppp.GetProtocol());
            Mac48Address mac = SuePacketUtils::ExtractSourceMac(packet, false, Mac48Address());

            // Read sequence from tag (required for LLR)
            SueTag tag;
            if (!packet->PeekPacketTag(tag)) {
                NS_LOG_WARN("Receive: no tag found, cannot process LLR");
                return;
            }

            uint32_t seq = tag.GetSequence();
            NS_LOG_DEBUG("Receive: read seq " << seq << " from tag (linkType=" << (uint32_t)tag.GetLinkType() << ")");

            // LLR related processing, send ACK or NACK packet
            if(m_llrEnabled){
                bool shouldProcess = false;
                if(m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager){
                    shouldProcess = m_llrSwitchPortManager->LlrReceivePacket(packet, vcId, seq, mac);
                } else if((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager){
                    m_llrNodeManager->LlrReceivePacket(packet, vcId, mac, seq);
                    shouldProcess = true;
                }
                if(!shouldProcess){
                    return; // Packet was discarded by LLR (duplicate/out-of-order)
                }
            }

            // Put into processing queue (check byte-level capacity)
            uint32_t packetSize = packet->GetSize();
            ProcessItem item = {originalPacket, packet, vcId, protocol};

            // Check byte-level capacity limit
            if (m_currentProcessingQueueBytes + packetSize <= m_processingQueueMaxBytes)
            {
                m_processingQueue.push(item);
                m_currentProcessingQueueSize++;
                m_currentProcessingQueueBytes += packetSize;

                // Trigger processing queue statistics (event-driven)
                SueStatsUtils::ProcessProcessingQueueStats(m_currentProcessingQueueBytes, m_processingQueueMaxBytes, GetNode()->GetId(), GetIfIndex() - 1);
            }
            else
            {
                // Queue is full, drop packet
                // Log processing queue packet drop (event-driven)
                SueStatsUtils::ProcessPacketDropStats(packet, GetNode()->GetId(), GetIfIndex() - 1, "ProcessingQueueFull");

                if (!IsSwitchDevice(m_address))
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

    void PointToPointSueNetDevice::StartProcessing()
    {
        if (m_processingQueue.empty())
        {
            m_isProcessing = false;
            return;
        }

        ProcessItem item = m_processingQueue.front();

        Time processingTime = m_processingRate.CalculateBytesTxTime(item.packet->GetSize());

        // Processing queue statistics will be triggered in CompleteProcessing

        // Schedule processing completion event
        Simulator::Schedule(processingTime, &PointToPointSueNetDevice::CompleteProcessing, this, item);
    }

    void PointToPointSueNetDevice::CompleteProcessing(ProcessItem item)
    {
        // Actually process packet
        if (!m_promiscCallback.IsNull())
        {
            m_macPromiscRxTrace(item.originalPacket);
            m_promiscCallback(this, item.packet, item.protocol, GetRemote(), GetAddress(), NetDevice::PACKET_HOST);
        }

        // Switch forwarding logic - delegate to SueSwitch module
        EthernetHeader ethHeader;
        item.packet->PeekHeader(ethHeader);
        bool forward = false;

        // Check if this device is a switch device and forward accordingly
        if (IsSwitchDevice(m_address))
        {
            bool forwarded = m_switch->ProcessSwitchForwarding(item.packet, ethHeader, this, item.protocol, item.vcId);
            if (forwarded)
            {
                forward = true;
                m_processingQueue.pop();
                m_currentProcessingQueueSize--;
                m_currentProcessingQueueBytes -= item.packet->GetSize();
            }
            else
            {
                // do nothing
            }
        }
        else
        {
            m_processingQueue.pop();
            m_currentProcessingQueueSize--;
            m_currentProcessingQueueBytes -= item.packet->GetSize();

            // Non-switch device
            // Queue operations have been completed in StartProcessing
            m_macRxTrace(item.originalPacket);

            // Remove Ethernet header for easier reception
            EthernetHeader removeEthHeader;
            item.packet->RemoveHeader(removeEthHeader);

            m_rxCallback(this, item.packet, item.protocol, GetRemote());
            m_cbfcManager->HandleCreditReturn(ethHeader, item.vcId);
            // TODO delay to be set currently: receiver is XPU and directly returns credits upon reception
            if (m_cbfcManager) {
                m_cbfcManager->CreditReturn(ethHeader.GetSource(), item.vcId);
            }
        }

        if(forward){
            // Trigger processing queue statistics (event-driven) - after processing completion
            SueStatsUtils::ProcessProcessingQueueStats(m_currentProcessingQueueBytes, m_processingQueueMaxBytes, GetNode()->GetId(), GetIfIndex() - 1);
            // Trigger receive packet statistics (event-driven) - after processing completion
            SueStatsUtils::ProcessReceivedPacketStats(item.originalPacket, m_vcBytesReceived, GetNode()->GetId(), GetIfIndex() - 1);
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
        if (!m_cbfcManager || !m_cbfcManager->IsInitialized())
        {
            InitializeCbfc();
        }
        // Initialize LLR if enabled
        if (m_llrEnabled)
        {
            InitializeLlr();
        }
        NS_LOG_FUNCTION(this << packet);

        // Extract VC ID from packet header
        uint8_t vcId = SuePacketUtils::ExtractVcIdFromPacket(packet);
        uint32_t  seq_rev;
        // Safety check for valid PPP header: only considered present if protocol belongs to known set
        auto HasValidPppHeader = [this](Ptr<Packet> p, SuePppHeader &out) -> bool {
            if (p->GetSize() < out.GetSerializedSize()) return false;
            Ptr<Packet> copy = p->Copy();
            SuePppHeader tmp;
            if (!copy->RemoveHeader(tmp)) return false; // Parsing failed
            uint16_t proto = tmp.GetProtocol();
            // Known PPP protocol set (using PPP format)
            if (proto == SuePacketUtils::EtherToPpp(0x0800) || proto == SuePacketUtils::EtherToPpp(0x86DD) ||
                proto == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_UPDATE) || proto == SuePacketUtils::EtherToPpp(SuePacketUtils::ACK_REV) || proto == SuePacketUtils::EtherToPpp(SuePacketUtils::NACK_REV))
            {
                out = tmp;
                return true;
            }
            return false;
        };

        SuePppHeader ppp;
        bool hasPpp = HasValidPppHeader(packet, ppp);
        if (hasPpp)
        {
            Mac48Address mac = SuePacketUtils::ExtractSourceMac(packet, false, Mac48Address()); // Extract and possibly overwrite source MAC

            // Read sequence from tag (required for LLR)
            SueTag tag;
            if (!packet->PeekPacketTag(tag)) {
                NS_LOG_WARN("EnqueueToVcQueue: no tag found, cannot process LLR");
                return false;
            }

            seq_rev = tag.GetSequence();
            NS_LOG_DEBUG("EnqueueToVcQueue: read seq " << seq_rev << " from tag (linkType=" << (uint32_t)tag.GetLinkType() << ")");

            uint16_t protocol = SuePacketUtils::PppToEther(ppp.GetProtocol());

            NS_LOG_DEBUG("EnqueueToVcQueue: detected internal packet with PPP proto=0x" << std::hex << ppp.GetProtocol() << std::dec
                          << ", etherProto=0x" << std::hex << protocol << std::dec << ", seq=" << seq_rev);

            // Directly handle ACK / NACK
            if (m_llrEnabled)
            {
                if (protocol == SuePacketUtils::ACK_REV)
                {
                    if(m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager){
                        Simulator::Schedule(m_AckProcessDelay, &LlrSwitchPortManager::ProcessLlrAck, m_llrSwitchPortManager, packet->Copy());
                    } else if((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager){
                        Simulator::Schedule(m_AckProcessDelay, &LlrNodeManager::ProcessLlrAck, m_llrNodeManager, packet->Copy());
                    }
                    return true;
                }
                if (protocol == SuePacketUtils::NACK_REV)
                {
                    if(m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager){
                        Simulator::Schedule(m_AckProcessDelay, &LlrSwitchPortManager::ProcessLlrNack, m_llrSwitchPortManager, packet->Copy());
                    } else if((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager){
                        Simulator::Schedule(m_AckProcessDelay, &LlrNodeManager::ProcessLlrNack, m_llrNodeManager, packet->Copy());
                    }
                    return true;
                }
            }

            // Internal forwarding: receiver-side processing for LLR
            if (m_llrEnabled)
            {
                if(m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager){
                    m_llrSwitchPortManager->LlrReceivePacket(packet, vcId, seq_rev, mac);
                } else if((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager){
                    m_llrNodeManager->LlrReceivePacket(packet, vcId, mac, seq_rev);
                }
            }

            // Remove PPP header, prepare for sending to peer (second stage)
            SuePppHeader ppp_rev;
            packet->RemoveHeader(ppp_rev);

            // Extract VC ID from packet for LlrSendPacket
            vcId = SuePacketUtils::ExtractVcIdFromPacket(packet); // Update vcId from packet

            Mac48Address mac_dst = GetRemoteMac();
            if(m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager){
                m_llrSwitchPortManager->LlrSendPacket(packet, vcId, mac_dst);
            } else if((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager){
                m_llrNodeManager->LlrSendPacket(packet, vcId);
            }

            m_queueManager->EnqueueToVcQueue(packet, vcId);
            
            NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                      << "] internal packet enqueued to VC " << static_cast<uint32_t>(vcId)
                                      << " (queue size now: " << (m_queueManager ? m_queueManager->GetVcQueueSize(vcId) : 0) << " packets)");

            // Trigger VC queue statistics (event-driven after VC enqueue)
            SueStatsUtils::ProcessVCQueueStats(m_queueManager, m_cbfcManager,
                                             m_numVcs, m_vcQueueMaxBytes,
                                             GetNode()->GetId(), GetIfIndex() - 1);

            // Schedule TryTransmit event if not already scheduled
            if (!m_tryTransmitScheduled)
            {
                m_tryTransmitEvent = Simulator::Schedule(m_dataAddHeadDelay, &PointToPointSueNetDevice::TryTransmit, this);
                m_tryTransmitScheduled = true;
            }
            return true;
        }
        else
        {
            NS_LOG_DEBUG("EnqueueToVcQueue: no valid PPP header detected; treating as external packet (will add headers). Packet size=" << packet->GetSize());
        }
        NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                  << "] EnqueueToVcQueue extracted VC ID: " << (uint32_t)vcId);

    // The first and third stages are both transmission logic
    // Obtain the peer MAC to determine the sequence number for the third stage
        Mac48Address mac_dst = GetRemoteMac();

        if(m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager){
            m_llrSwitchPortManager->LlrSendPacket(packet, vcId, mac_dst);
        } else if((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager){
            m_llrNodeManager->LlrSendPacket(packet, vcId);
        }
        // Get source MAC to check if it's a forwarded packet
        Mac48Address sourceMac = SuePacketUtils::ExtractSourceMac(packet, false, Mac48Address());
        if (IsSwitchDevice(sourceMac) && m_cbfcManager)
        {
            // For switch devices, we need to handle credit returns differently
            // This is handled by the HandleCreditReturn method when packets are received
            EthernetHeader tempEthHeader;
            tempEthHeader.SetSource(sourceMac);
            // m_cbfcManager->HandleCreditReturn(tempEthHeader, vcId);
        }

        m_macTxTrace(packet);

        m_queueManager->EnqueueToVcQueue(packet, vcId);

        NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                  << "] packet enqueued to VC " << static_cast<uint32_t>(vcId)
                                  << " (queue size now: " << (m_queueManager ? m_queueManager->GetVcQueueSize(vcId) : 0) << " packets)");

        // Trigger VC queue statistics (event-driven after VC enqueue)
        SueStatsUtils::ProcessVCQueueStats(m_queueManager, m_cbfcManager,
                                         m_numVcs, m_vcQueueMaxBytes,
                                         GetNode()->GetId(), GetIfIndex() - 1);

        // Schedule TryTransmit event if not already scheduled
        if (!m_tryTransmitScheduled)
        {
            m_tryTransmitEvent = Simulator::Schedule(m_dataAddHeadDelay, &PointToPointSueNetDevice::TryTransmit, this);
            m_tryTransmitScheduled = true;
        }

        return true;
    }


    Ptr<Queue<Packet>>
    PointToPointSueNetDevice::GetQueue(void) const
    {
        NS_LOG_FUNCTION(this);
        return m_queue;
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
        if (!m_cbfcManager || !m_cbfcManager->IsInitialized())
        {
            InitializeCbfc();
        }
        // Initialize LLR if enabled
        if (m_llrEnabled)
        {
            InitializeLlr();
        }

    // Credit update packets enter high-priority main queue
        if (protocolNumber == SuePacketUtils::PROT_CBFC_UPDATE)
        {
            // Credit packet structure - only CBFC header, PPP header added below
            // PPP Header
            AddHeader(packet, protocolNumber);
            if (!m_queue->Enqueue(packet))
            {
                // Log main queue packet drop (event-driven)
                SueStatsUtils::ProcessPacketDropStats(packet, GetNode()->GetId(), GetIfIndex() - 1, "MainQueueFull");
                if (!IsSwitchDevice(m_address))
                {
                    NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                              << "] credit packet DROPPED (main queue full: "
                                              << m_queue->GetNPackets() << "/"
                                              << m_queue->GetMaxSize().GetValue() << " packets)");
                }

                m_macTxDropTrace(packet);
                return false;
            }
            if (!IsSwitchDevice(m_address))
            {
                NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                          << "] credit packet enqueued to main queue"
                                          << " (size now: " << m_queue->GetNPackets() << " packets)");
            }

            // Trigger main queue statistics (event-driven after main queue enqueue)
            SueStatsUtils::ProcessMainQueueStats(m_queue, GetNode()->GetId(), GetIfIndex() - 1);

            // Delay is between enqueue and transmission
            // Schedule TryTransmit event if not already scheduled
            if (!m_tryTransmitScheduled)
            {
                m_tryTransmitEvent = Simulator::Schedule(m_creUpdateAddHeadDelay, &PointToPointSueNetDevice::TryTransmit, this);
                m_tryTransmitScheduled = true;
            }
        }
        else if(protocolNumber == SuePacketUtils::ACK_REV || protocolNumber == SuePacketUtils::NACK_REV){// ACK/NACK packets enter high-priority main queue
            m_queue->Enqueue(packet);

            // Trigger main queue statistics (event-driven after main queue enqueue)
            SueStatsUtils::ProcessMainQueueStats(m_queue, GetNode()->GetId(), GetIfIndex() - 1);

            // Schedule TryTransmit event if not already scheduled
            if (!m_tryTransmitScheduled)
            {
                m_tryTransmitEvent = Simulator::Schedule(m_dataAddHeadDelay, &PointToPointSueNetDevice::TryTransmit, this);
                m_tryTransmitScheduled = true;
            }
        }
        else
        {
            if (!IsSwitchDevice(m_address))
            { // Add EthernetHeader when XPU device sends
                // Header processing logic: extract destination IP from IPv4 header, add EthernetHeader
                // Packet structure: SUEHeader | UDP | IPv4 | Ethernet | CBFC | PPP

                // Extract destination IP from packet
                Ipv4Address destIp = SuePacketUtils::ExtractDestIpFromPacket(packet);

                // Query destination MAC address
                Mac48Address destMac = SuePacketUtils::GetMacForIp(destIp);

                // Add Ethernet header
                SuePacketUtils::AddEthernetHeader(packet, destMac, GetLocalMac());

                NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                          << "] added EthernetHeader for IP " << destIp << " -> MAC " << destMac);
            }

            // Data packet enters corresponding VC queue
            EnqueueToVcQueue(packet);
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

    void PointToPointSueNetDevice::AddEthernetHeader(Ptr<Packet> packet, Mac48Address destMac)
    {
        EthernetHeader ethHeader;
        ethHeader.SetSource(m_address);
        ethHeader.SetDestination(destMac);
        ethHeader.SetLengthType(0x0800); // IPv4
        packet->AddHeader(ethHeader);
    }

    // Switch support methods implementation
    Ptr<SueSwitch>
    PointToPointSueNetDevice::GetSwitch() const
    {
        return m_switch;
    }

    void
    PointToPointSueNetDevice::SetSwitch(Ptr<SueSwitch> switchModule)
    {
        m_switch = switchModule;
    }

    bool
    PointToPointSueNetDevice::IsSwitchDevice(Mac48Address mac) const
    {
        NS_LOG_FUNCTION(this << mac);

        uint8_t buffer[6];
        mac.CopyTo(buffer);
        uint8_t lastByte = buffer[5]; // Last byte of MAC address
        // TODO: Simplistic logic; needs modification for proper XPU/switch identification
        return (lastByte % 2 == 0); // Even numbers are switch devices
    }

    
    Ptr<CbfcManager>
    PointToPointSueNetDevice::GetCbfcManager() const
    {
        return m_cbfcManager;
    }

    Ptr<SueQueueManager>
    PointToPointSueNetDevice::GetQueueManager() const
    {
        return m_queueManager;
    }

    bool
    PointToPointSueNetDevice::GetLlrEnabled() const
    {
        return m_llrEnabled;
    }

    Time
    PointToPointSueNetDevice::GetSwitchForwardDelay() const
    {
        return m_switchForwardDelay;
    }
} // namespace ns3
