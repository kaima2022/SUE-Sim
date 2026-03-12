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
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <limits>
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

    namespace {
    struct RegisteredDeviceMeta
    {
        bool isSwitchDevice = false;
        bool hasXpuId = false;
        bool hasPortId = false;
        uint32_t xpuId = 0;
        uint32_t portId = 0; // 1-based global port id (consistent with XPU port GetIfIndex())
    };
    } // namespace

    static std::map<Mac48Address, RegisteredDeviceMeta> g_registeredDeviceMeta;

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
                                .AddAttribute("ErrorModelApplyToControlPackets",
                                              "Whether to apply the receiver error model to control packets "
                                              "(CBFC update + LLR ACK/NACK). If false, only data packets (IPv4/IPv6) "
                                              "are subject to ReceiveErrorModel corruption.",
                                              BooleanValue(true),
                                              MakeBooleanAccessor(&PointToPointSueNetDevice::m_errorModelApplyToControlPackets),
                                              MakeBooleanChecker())
                                .AddAttribute("ErrorModelApplyToSyncPackets",
                                              "Whether to apply the receiver error model to CBFC_SYNC packets. "
                                              "If false, sync packets are always delivered reliably (simulating "
                                              "a protected out-of-band channel for credit synchronization).",
                                              BooleanValue(true),
                                              MakeBooleanAccessor(&PointToPointSueNetDevice::m_errorModelApplyToSyncPackets),
                                              MakeBooleanChecker())
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
                                .AddAttribute("LinkCreditMode",
                                              "Link credit allocation mode: 0 = SHARED (all VCs share one pool), "
                                              "1 = EXCLUSIVE (each VC gets total/numVcs).",
                                              UintegerValue(0),
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::m_linkCreditModeRaw),
                                              MakeUintegerChecker<uint8_t>(0, 1))
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
                                .AddAttribute("SwitchCredits",
                                              "Switch-internal CBFC egress credit budget (total across all VCs). "
                                              "0 means auto: floor(VcQueueMaxBytes / BytesPerCredit) * NumVcs.",
                                              UintegerValue(0),
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::m_switchCredits),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("AdditionalHeaderSize",
                                              "Additional header size for capacity reservation (default 46 bytes)",
                                              UintegerValue(46), // Default value: 46 bytes
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::m_additionalHeaderSize),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("HeaderSize",
                                              "Header size for dynamic credit calculation (Ethernet + SUE headers, default 52 bytes)",
                                              UintegerValue(52), // Default value: 52 bytes
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::m_headerSize),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("TransactionSize",
                                              "Transaction size for dynamic credit calculation (default 256 bytes)",
                                              UintegerValue(256), // Default value: 256 bytes
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::m_transactionSize),
                                              MakeUintegerChecker<uint32_t>())
                                .AddAttribute("CbfcCreditWindowPacketBytes",
                                              "CBFC init-time window validation packet size hint (bytes). "
                                              "If >0, CBFC will validate that LinkCredits and the per-peer "
                                              "share of SwitchCredits are sufficient to carry at least one "
                                              "packet of this size (ceil(bytes/BytesPerCredit) credits). "
                                              "0 disables the check.",
                                              UintegerValue(0),
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::m_cbfcCreditWindowPacketBytes),
                                              MakeUintegerChecker<uint32_t>())
                                // Credit-to-byte mapping attributes
                                .AddAttribute("BytesPerCredit",
                                              "Bytes per credit for linear mapping (default: 256 bytes/credit)",
                                              UintegerValue(256), // Default value: 256 bytes per credit
                                              MakeUintegerAccessor(&PointToPointSueNetDevice::m_bytesPerCredit),
                                              MakeUintegerChecker<uint32_t>())
                                                                .AddAttribute("CreUpdateAddHeadDelay",
                                              "Credit Update packet Add Head Delay",
                                              TimeValue(NanoSeconds(3)),
                                              MakeTimeAccessor(&PointToPointSueNetDevice::m_creUpdateAddHeadDelay),
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
                                .AddAttribute("EnableCreditSync",
                                              "Enable periodic credit sync packets to correct credit drift/leaks "
                                              "(requires link-layer CBFC semantics to make sense).",
                                              BooleanValue(false),
                                              MakeBooleanAccessor(&PointToPointSueNetDevice::m_enableCreditSync),
                                              MakeBooleanChecker())
                                .AddAttribute("CreditSyncInterval",
                                              "Periodic credit sync interval (0 disables).",
                                              TimeValue(Seconds(0.0)),
                                              MakeTimeAccessor(&PointToPointSueNetDevice::m_creditSyncInterval),
                                              MakeTimeChecker())
                                .AddAttribute("SwitchForwardDelay",
                                              "Delay before forwarding packets in switch",
                                              TimeValue(NanoSeconds(150)),
                                              MakeTimeAccessor(&PointToPointSueNetDevice::m_switchForwardDelay),
                                              MakeTimeChecker())
                                .AddAttribute("VcSchedulingDelay",
                                              "VC queue scheduling delay",
                                              // NOTE: Modeling this as a per-packet *gap* directly reduces
                                              // achievable line rate and can create artificial egress queue
                                              // overflows (DROP storms) even under steady 1:1 traffic (e.g.,
                                              // ring all-reduce on a single switch). In real NICs/switch ports,
                                              // arbitration is deeply pipelined and does not necessarily
                                              // manifest as a serial gap on the wire. Default to 0 to avoid
                                              // injecting a non-physical throughput bottleneck.
                                              TimeValue(NanoSeconds(0)),
                                              MakeTimeAccessor(&PointToPointSueNetDevice::m_vcSchedulingDelay),
                                              MakeTimeChecker())
                                .AddAttribute("ProcessingQueueScheduleDelay",
                                              "Processing queue scheduling delay",
                                              TimeValue(NanoSeconds(5)),   // Default: 5 nanoseconds
                                              MakeTimeAccessor(&PointToPointSueNetDevice::m_processingQueueScheduleDelay),
                                              MakeTimeChecker())
                                //LLR
                                .AddAttribute("EnableLLR",
                                            "If enable LLR.",
                                            BooleanValue(false),
                                            MakeBooleanAccessor(&PointToPointSueNetDevice::m_llrEnabled),
                                            MakeBooleanChecker())
                                .AddAttribute("LlrProtectCbfcUpdates",
                                            "When EnableLLR=true, also apply LLR sequencing to CBFC update packets. "
                                            "If false, CBFC update packets are sent without LLR reliability.",
                                            BooleanValue(true),
                                            MakeBooleanAccessor(&PointToPointSueNetDevice::m_llrProtectCbfcUpdates),
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
                                            MakeUintegerChecker<uint32_t>(1, 65535))
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
          m_errorModelApplyToControlPackets(true),
          m_errorModelApplyToSyncPackets(true),
          m_linkUp(false),
          m_currentPkt(0),
          // CBFC configuration values (in header file order)
          m_initialCredits(0),
          m_numVcs(0),
          m_creditBatchSize(10),
          m_switchCredits(0),
          m_vcQueueMaxBytes(2 * 1024 * 1024), // Default VC queue max capacity 2MB (2*1024*1024 bytes)
          m_additionalHeaderSize(46), // Default 46 bytes
          m_headerSize(52), // Default header size for dynamic credit calculation
          m_transactionSize(256), // Default transaction size for dynamic credit calculation
          m_cbfcCreditWindowPacketBytes(0),
          m_enableLinkCBFC(false),
          m_linkCreditModeRaw(0),
          // Initialize credit-to-byte mapping parameters
          m_bytesPerCredit(256), // Default: 256 bytes per credit
          m_currentProcessingQueueSize(0),
          m_currentProcessingQueueBytes(0),
          m_processingDelay(NanoSeconds(10)),
          m_processingQueueMaxBytes(2 * 1024 * 1024), // Default processing queue max capacity 2MB
          m_needCredit(false),                        // Default: no credit needed
	          m_processingScheduled(false),               // Default: processing not scheduled
	          m_creUpdateAddHeadDelay(NanoSeconds(3)),
	          m_creditGenerateDelay(NanoSeconds(10)),
	          m_enableCreditSync(false),
	          m_creditSyncInterval(Seconds(0.0)),
	          m_creditSyncEvent(EventId()),
	          m_switchForwardDelay(NanoSeconds(150)),
	          m_vcSchedulingDelay(NanoSeconds(0)),
	          m_loggingEnabled(true), // Enable logging by default
	          m_processingRate(m_bps), // Default same as transmission rate
	          m_processingRateString("200Gbps"), // Default processing rate string
	          //LLR
	          m_llrEnabled(false),
              m_llrProtectCbfcUpdates(true),
		          m_llrInitialized(false),
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

        // ------------------------------------------------------------
        // Auto-calibrate CBFC credits from buffer capacities (optional)
        // ------------------------------------------------------------
        //
        // Motivation:
        // - In production CBFC, the credit window must be consistent with the receiver's buffering;
        //   otherwise "CBFC enabled but still drops" (oversubscription) or "CBFC enabled but stalls"
        //   (per-ingress window < one packet) can occur.
        //
        // Convention used here:
        // - Link `InitialCredits`: if set to 0, auto-derive from ProcessingQueueMaxBytes.
        // - Switch-internal `SwitchCredits`: if set to 0, auto-derive from VcQueueMaxBytes
        //   (switch egress VC queue capacity) * NumVcs to represent total across all VCs.
        if (m_enableLinkCBFC)
        {
            if (m_bytesPerCredit == 0)
            {
                NS_ABORT_MSG("BytesPerCredit must be > 0 when EnableLinkCBFC=true");
            }

            // Link peer credits (sender->receiver link): approximate receiver buffering using the
            // ingress processing queue capacity.
            if (m_initialCredits == 0)
            {
                const uint32_t autoLinkCredits = std::max(1u, m_processingQueueMaxBytes / m_bytesPerCredit);
                m_initialCredits = autoLinkCredits;
                NS_LOG_INFO("Auto LinkCredits (InitialCredits)=floor(ProcessingQueueMaxBytes/BytesPerCredit)="
                            << m_initialCredits << " (ProcessingQueueMaxBytes=" << m_processingQueueMaxBytes
                            << ", BytesPerCredit=" << m_bytesPerCredit << ")");
            }

            // Switch internal credits (ingress->egress within the same switch): approximate egress
            // buffering using the egress VC queue capacity.
            if (m_switch && IsSwitchDevice(m_address) && m_switchCredits == 0)
            {
                const uint32_t vcFactor = std::max(1u, static_cast<uint32_t>(m_numVcs));
                const uint32_t autoSwitchCredits =
                    std::max(1u, (m_vcQueueMaxBytes / m_bytesPerCredit) * vcFactor);
                m_switchCredits = autoSwitchCredits;
                NS_LOG_INFO("Auto SwitchCredits=floor(VcQueueMaxBytes/BytesPerCredit)*NumVcs="
                            << m_switchCredits << " (VcQueueMaxBytes=" << m_vcQueueMaxBytes
                            << ", BytesPerCredit=" << m_bytesPerCredit
                            << ", NumVcs=" << vcFactor << ")");
            }
        }

        // Configure CBFC credit calculation parameters before initializing peer credits,
        // so init-time checks (e.g., creditWindowPacketBytes) use the correct BytesPerCredit.
        m_cbfcManager->SetDynamicCreditMode(true, 1, m_transactionSize, m_headerSize);
        m_cbfcManager->SetAdvancedCreditCalculation(m_bytesPerCredit);
        m_cbfcManager->SetLinkCreditMode(static_cast<LinkCreditMode>(m_linkCreditModeRaw));
        
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
            m_switchCredits,                          // Switch credits
            m_cbfcCreditWindowPacketBytes             // credit window packet bytes hint
        );

        // Initialize queue manager directly with drop callback
        m_queueManager->Initialize(m_numVcs, m_vcQueueMaxBytes, m_additionalHeaderSize,
                                 MakeCallback(&PointToPointSueNetDevice::HandlePacketDrop, this));

        if (!m_switch || !IsSwitchDevice(m_address))
        {
            NS_LOG_INFO("Link: Initialized on Node " << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                                     << " with " << (uint32_t)m_numVcs << " VCs and " << m_initialCredits << " initial credits.");
        }

        // Start periodic credit sync if enabled. This runs on all devices (NICs + switch ports)
        // and is intended to correct credit drift/leaks under lossy/drop modeling.
        if (m_enableCreditSync && !m_creditSyncInterval.IsZero())
        {
            if (!m_creditSyncEvent.IsPending())
            {
                m_creditSyncEvent = Simulator::Schedule(m_creditSyncInterval,
                                                       &PointToPointSueNetDevice::CreditSyncTick,
                                                       this);
            }
        }
    }

    void
    PointToPointSueNetDevice::CreditSyncTick()
    {
        if (!m_enableCreditSync || m_creditSyncInterval.IsZero())
        {
            return;
        }

        // Credit sync is only meaningful under link-level CBFC.
        if (!m_enableLinkCBFC)
        {
            m_creditSyncEvent = Simulator::Schedule(m_creditSyncInterval,
                                                   &PointToPointSueNetDevice::CreditSyncTick,
                                                   this);
            return;
        }

        if (!m_cbfcManager || !m_cbfcManager->IsInitialized())
        {
            InitializeCbfc();
        }

        // In DROP/losy configurations, switch-internal CBFC credits can leak when a packet is
        // dropped before it is transmitted on the egress port (e.g., switch egress VC queue full).
        // Unlike link peers, these "internal peers" do not have a physical link and therefore do
        // not participate in the on-wire CBFC_SYNC exchange. To prevent permanent deadlocks, we
        // periodically re-sync switch-internal Tx credits back to the configured SwitchCredits.
        //
        // NOTE: We do this locally (without emitting control packets) to avoid an O(N^2) burst of
        // internal sync packets on large switches.
        if (m_switch && IsSwitchDevice(m_address))
        {
            Ptr<Node> node = GetNode();
            if (node)
            {
                const Mac48Address localMac = GetLocalMac();
                struct PeerPort
                {
                    Mac48Address mac;
                    Ptr<PointToPointSueNetDevice> dev;
                    uint32_t outPortIndex = 0;
                };

                std::vector<PeerPort> peers;
                std::unordered_map<uint32_t, uint32_t> portIdCounts;
                bool havePortIdMeta = false;

                // Count switch ports per portId (lane) to match initialization semantics.
                for (uint32_t i = 0; i < node->GetNDevices(); ++i)
                {
                    Ptr<PointToPointSueNetDevice> p2pDev =
                        DynamicCast<PointToPointSueNetDevice>(node->GetDevice(i));
                    if (!p2pDev)
                    {
                        continue;
                    }
                    const Mac48Address mac = Mac48Address::ConvertFrom(p2pDev->GetAddress());
                    bool isSwitch = false;
                    uint32_t portId = 0;
                    if (PointToPointSueNetDevice::LookupRegisteredDeviceMeta(mac,
                                                                             &isSwitch,
                                                                             nullptr,
                                                                             &portId) &&
                        isSwitch)
                    {
                        havePortIdMeta = true;
                        portIdCounts[portId] += 1;
                    }
                }

                for (uint32_t i = 0; i < node->GetNDevices(); ++i)
                {
                    Ptr<PointToPointSueNetDevice> p2pDev =
                        DynamicCast<PointToPointSueNetDevice>(node->GetDevice(i));
                    if (!p2pDev)
                    {
                        continue;
                    }
                    const Mac48Address peerMac = Mac48Address::ConvertFrom(p2pDev->GetAddress());
                    if (peerMac == localMac)
                    {
                        continue;
                    }
                    peers.push_back(PeerPort{peerMac, p2pDev, p2pDev->GetIfIndex()});
                }

                const uint32_t peerCount = static_cast<uint32_t>(peers.size());
                if (peerCount > 0 && m_bytesPerCredit > 0)
                {
                    for (const auto& peer : peers)
                    {
                        uint32_t competitorCount = peerCount; // fallback
                        if (havePortIdMeta)
                        {
                            bool peerIsSwitch = false;
                            uint32_t peerPortId = 0;
                            if (PointToPointSueNetDevice::LookupRegisteredDeviceMeta(peer.mac,
                                                                                     &peerIsSwitch,
                                                                                     nullptr,
                                                                                     &peerPortId) &&
                                peerIsSwitch)
                            {
                                auto itCnt = portIdCounts.find(peerPortId);
                                if (itCnt != portIdCounts.end() && itCnt->second > 1)
                                {
                                    competitorCount = itCnt->second - 1;
                                }
                            }
                        }
                        if (competitorCount == 0)
                        {
                            competitorCount = 1;
                        }

                        const uint32_t vcFactor = std::max(1u, static_cast<uint32_t>(m_numVcs));
                        const uint32_t perPeerMax =
                            (vcFactor > 0) ? (m_switchCredits / competitorCount / vcFactor) : 0u;

                        // Overwrite internal Tx credits based on the egress VC queue free capacity.
                        // This both prevents credit inflation (over-admission) and replenishes leaked
                        // credits (deadlock prevention) in DROP/lossy runs.
                        if (peer.dev)
                        {
                            peer.dev->InitializeCbfc();
                        }
                        Ptr<SueQueueManager> qm = peer.dev ? peer.dev->GetQueueManager() : nullptr;
                        for (uint8_t vcId = 0; vcId < m_numVcs; ++vcId)
                        {
                            uint32_t perPeerCredits = perPeerMax;
                            if (qm)
                            {
                                const uint32_t maxBytes = qm->GetVcQueueMaxBytes();
                                const uint32_t usedBytes = qm->GetVcQueueBytes(vcId);
                                const uint64_t reservedBytes =
                                    m_switch ? m_switch->GetReservedEgressVcQueueBytes(peer.outPortIndex, vcId) : 0;
                                const uint64_t usedPlusReserved =
                                    static_cast<uint64_t>(usedBytes) + reservedBytes;
                                const uint32_t freeBytes =
                                    (usedPlusReserved >= maxBytes)
                                        ? 0u
                                        : static_cast<uint32_t>(maxBytes - usedPlusReserved);
                                const uint32_t freeCreditsTotal = freeBytes / m_bytesPerCredit;
                                // Do NOT evenly split the instantaneous free-credit budget across all
                                // contenders here.
                                //
                                // Reason:
                                // - If we split, each per-peer share can fall below the credits required
                                //   to send a single max-sized packet (creditWindowPacketBytes), even
                                //   though the egress VC queue has enough free bytes for *one* packet.
                                // - That creates a deadlock where nobody can make forward progress.
                                //
                                // Instead, treat this as a leak/inflation correction mechanism:
                                // - Clamp each peer to the per-peer maximum
                                //   (SwitchCredits/competitorCount/NumVcs)
                                // - Also clamp to the current total free credits (derived from egress
                                //   used+reserved bytes)
                                //
                                // Over-admission is still prevented by the authoritative egress
                                // reservation check (TryReserveEgressVcQueueBytes()).
                                // Internal credit sync should primarily *repair leaks* (increase when
                                // credits drift downward), not aggressively clamp downward. Clamping
                                // can deadlock when short-term occupancy is high: per-peer credits can
                                // fall below the minimum needed for one packet, preventing any forward
                                // progress even though egress admission already enforces capacity.
                                //
                                // Therefore, only raise credits toward the safe bound derived from
                                // current free bytes, but do not reduce below the existing counter.
                                const uint32_t targetCredits = std::min(perPeerMax, freeCreditsTotal);
                                const uint32_t currentCredits =
                                    m_cbfcManager ? m_cbfcManager->GetTxCredits(peer.mac, vcId) : 0;
                                perPeerCredits = std::max(currentCredits, targetCredits);
                            }
                            m_cbfcManager->SetTxCredits(peer.mac, vcId, perPeerCredits);
                        }
                    }
                }
            }
        }

        // Link-level CBFC sync: send the *current available credits at this receiver*
        // (derived from processing-queue free bytes), so the peer can safely overwrite
        // its Tx credit counter without inflating beyond receiver capacity.
        const uint32_t usedBytes = m_currentProcessingQueueBytes;
        const uint32_t maxBytes = m_processingQueueMaxBytes;
        const uint32_t freeBytes = (maxBytes > usedBytes) ? (maxBytes - usedBytes) : 0;
        const uint32_t freeCreditsTotal = freeBytes / m_bytesPerCredit;
        // Safety margin for sync-based overwrite:
        //
        // Credit-sync packets carry an *instantaneous* receiver-side free-credit estimate.
        // If the sender overwrites its credit counter to this value while there are still
        // packets "in flight" (sent-but-not-yet-accounted at the receiver), the sender can
        // transiently overrun the receiver processing queue by ~1 packet and trigger
        // ProcessingQueueFull drops. This is especially visible at the first sync tick
        // (e.g., 100us) under heavy injection.
        //
        // We therefore subtract a 1-packet safety margin (in credits) based on the
        // configured max on-wire packet size hint.
        uint32_t syncSafetyCredits = 0;
        // In lossy experiments (ErrorModelDrop), drops are expected and LLR is responsible for
        // recovery. Using an aggressive safety margin here can make the advertised credit
        // budget fall below one packet and cause long stalls. Therefore, only apply the
        // 1-packet safety margin when the receiver error model is effectively disabled.
        bool receiverIsLossy = false;
        if (m_receiveErrorModel)
        {
            Ptr<RateErrorModel> rem = DynamicCast<RateErrorModel>(m_receiveErrorModel);
            if (rem)
            {
                DoubleValue er;
                rem->GetAttribute("ErrorRate", er);
                receiverIsLossy = (er.Get() > 0.0);
            }
        }

        if (!receiverIsLossy)
        {
            if (m_cbfcCreditWindowPacketBytes > 0)
            {
                syncSafetyCredits =
                    (m_cbfcCreditWindowPacketBytes + m_bytesPerCredit - 1) / m_bytesPerCredit;
            }
            if (syncSafetyCredits == 0)
            {
                syncSafetyCredits = 1;
            }
        }

        Mac48Address targetMac = GetRemoteMac();
        // Credits required to carry one max-sized packet (credit window hint).
        const uint32_t needCreditsForPacket =
            (m_cbfcCreditWindowPacketBytes > 0 && m_bytesPerCredit > 0)
                ? ((m_cbfcCreditWindowPacketBytes + m_bytesPerCredit - 1) / m_bytesPerCredit)
                : 1;
        const bool receiverCanAcceptOnePacket =
            (m_cbfcCreditWindowPacketBytes > 0 && freeBytes >= m_cbfcCreditWindowPacketBytes);

        // Determine link credit mode from the CBFC manager
        const LinkCreditMode creditMode = m_cbfcManager ? m_cbfcManager->GetLinkCreditMode ()
                                                        : LinkCreditMode::SHARED;

        if (creditMode == LinkCreditMode::SHARED)
        {
            // SHARED mode: send ONE sync packet with total free credits (vc=0, sender ignores vc)
            uint32_t syncCredits =
                (syncSafetyCredits == 0)
                    ? freeCreditsTotal
                    : ((freeCreditsTotal > syncSafetyCredits) ? (freeCreditsTotal - syncSafetyCredits) : 0);

            if (receiverCanAcceptOnePacket && syncCredits < needCreditsForPacket)
            {
                syncCredits = std::min(freeCreditsTotal, needCreditsForPacket);
            }

            EthernetHeader ethHeader;
            ethHeader.SetSource(GetLocalMac());
            ethHeader.SetDestination(targetMac);
            ethHeader.SetLengthType(0x0800);

            SueCbfcHeader syncHeader;
            syncHeader.SetVcId(0);
            syncHeader.SetCredits(syncCredits);

            Ptr<Packet> p = Create<Packet>();
            p->AddHeader(ethHeader);
            p->AddHeader(syncHeader);

            Send(p, GetRemote(), SuePacketUtils::PROT_CBFC_SYNC);
        }
        else
        {
            // EXCLUSIVE mode: per-VC sync with divided credits
            const uint32_t vcFactor = std::max(1u, static_cast<uint32_t>(m_numVcs));
            for (uint8_t vcId = 0; vcId < m_numVcs; ++vcId)
            {
                uint32_t perVcCredits =
                    (syncSafetyCredits == 0)
                        ? (freeCreditsTotal / vcFactor)
                        : (((freeCreditsTotal > syncSafetyCredits)
                                ? (freeCreditsTotal - syncSafetyCredits)
                                : 0) / vcFactor);

                // Per-VC deadlock protection
                const uint32_t perVcNeed = needCreditsForPacket;
                const bool perVcCanAccept =
                    (m_cbfcCreditWindowPacketBytes > 0 &&
                     freeBytes >= m_cbfcCreditWindowPacketBytes);
                if (perVcCanAccept && perVcCredits < perVcNeed)
                {
                    perVcCredits = std::min(freeCreditsTotal / vcFactor, perVcNeed);
                }

                EthernetHeader ethHeader;
                ethHeader.SetSource(GetLocalMac());
                ethHeader.SetDestination(targetMac);
                ethHeader.SetLengthType(0x0800);

                SueCbfcHeader syncHeader;
                syncHeader.SetVcId(vcId);
                syncHeader.SetCredits(perVcCredits);

                Ptr<Packet> p = Create<Packet>();
                p->AddHeader(ethHeader);
                p->AddHeader(syncHeader);

                Send(p, GetRemote(), SuePacketUtils::PROT_CBFC_SYNC);
            }
        }

        m_creditSyncEvent = Simulator::Schedule(m_creditSyncInterval,
                                               &PointToPointSueNetDevice::CreditSyncTick,
                                               this);
    }

    uint8_t
    PointToPointSueNetDevice::GetLlrNumVcs () const
    {
        if (!m_llrProtectCbfcUpdates)
        {
            return m_numVcs;
        }
        if (m_numVcs == std::numeric_limits<uint8_t>::max())
        {
            return m_numVcs;
        }
        return static_cast<uint8_t>(m_numVcs + 1);
    }

    uint8_t
    PointToPointSueNetDevice::GetLlrControlVcId () const
    {
        // Only used when LLR protection for CBFC updates is enabled.
        if (m_numVcs == std::numeric_limits<uint8_t>::max())
        {
            // Fallback: share the last data VC if we cannot allocate an extra control VC.
            return static_cast<uint8_t>(m_numVcs - 1);
        }
        return m_numVcs;
    }

    void
    PointToPointSueNetDevice::InitializeLlr()
    {
        if (m_llrInitialized)
            return;

        // Check if this is a switch device
        bool isSwitchDevice = m_switch && IsSwitchDevice(m_address);

        if (isSwitchDevice)
        {
            // Initialize LLR switch port manager
            Mac48Address peerMac = GetRemoteMac();
            const uint8_t llrNumVcs = GetLlrNumVcs();
            m_llrSwitchPortManager->Initialize(
                m_llrEnabled,
                m_llrWindowSize,
                m_llrTimeout,
                m_AckAddHeaderDelay,
                m_AckProcessDelay,
                0x0800,
                llrNumVcs,
                [this]() { return GetLocalMac(); },                    // GetLocalMac callback
                [this]() { return GetNode(); },                        // GetNode callback
                [this]() { return m_switch; },                          // GetSwitch callback
                [this](Ptr<Packet> packet, Mac48Address targetMac, uint16_t protocolNum) {
                    FindDeviceAndSend(packet, targetMac, protocolNum); // SendPacket callback
                },
                [this]() {
                    // TryTransmit callback - trigger transmission attempt
                    Simulator::ScheduleNow(&PointToPointSueNetDevice::TryTransmit, this);
                },
                peerMac                                                 // Connected peer MAC
            );
        }
        else
        {
            // Initialize LLR node manager for regular NICs
            const uint8_t llrNumVcs = GetLlrNumVcs();
            m_llrNodeManager->Initialize(
                m_llrEnabled,
                m_llrWindowSize,
                m_llrTimeout,
                m_AckAddHeaderDelay,
                m_AckProcessDelay,
                0x0800,
                llrNumVcs,
                [this]() { return GetLocalMac(); },                    // GetLocalMac callback
                [this]() { return GetNode(); },                        // GetNode callback
                [this]() { return GetRemoteMac(); },                   // GetRemoteMac callback
                [this](Ptr<Packet> packet, Mac48Address targetMac, uint16_t protocolNum) {
                    FindDeviceAndSend(packet, targetMac, protocolNum); // SendPacket callback
                },
                [this]() {
                    // TryTransmit callback - trigger transmission attempt
                    Simulator::ScheduleNow(&PointToPointSueNetDevice::TryTransmit, this);
                }
            );
        }

        m_llrInitialized = true;
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
        if (m_loggingEnabled)
        {
            SueStatsUtils::ProcessPacketDropStats(droppedPacket, GetNode()->GetId(), GetIfIndex() - 1, "VCQueueFull");
        }
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

        if (m_loggingEnabled)
        {
            Ptr<Packet> statPacket = p->Copy ();
            // Schedule statistics logging for packet transmission
            Simulator::Schedule(txCompleteTime, [this, statPacket]() {
                SueStatsUtils::ProcessSentPacketStats(statPacket, GetNode()->GetId(), GetIfIndex() - 1);
            });
        }

        // Switch egress port: credit return only after packet transmission.
        // Under LLR, the same data packet may be transmitted multiple times (retransmissions),
        // but credits must be returned only once per unique sequence number; otherwise credits
        // can grow unbounded and exceed the configured initial/switch credit limits.
        bool scheduleCreditReturn = false;
        uint8_t creditReturnVcId = 0;
        Mac48Address creditReturnTargetMac;
        const uint32_t creditReturnPktSize = p->GetSize();

        SuePppHeader ppp;
        p->PeekHeader(ppp);
        const uint16_t pppProto = ppp.GetProtocol();
        if (IsSwitchDevice(m_address) &&
            pppProto != SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_UPDATE) &&
            pppProto != SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_SYNC) &&
            pppProto != SuePacketUtils::EtherToPpp(SuePacketUtils::ACK_REV) &&
            pppProto != SuePacketUtils::EtherToPpp(SuePacketUtils::NACK_REV))
        {
            creditReturnVcId = SuePacketUtils::ExtractVcIdFromPacket(p);

            // Replace Source MAC with current device MAC for downstream; return credits to the original source MAC.
            creditReturnTargetMac = SuePacketUtils::ExtractSourceMac(p, true, GetLocalMac());
            scheduleCreditReturn = true;

            if (m_llrEnabled)
            {
                // Under LLR, the same data packet may be transmitted multiple times
                // (retransmissions). Credits must be returned only once per *original*
                // transmission; otherwise credits can inflate.
                //
                // Do not use a monotonic "last seq" heuristic here: packets can be transmitted
                // out-of-order across VCs even without retransmission, and suppressing credit
                // return in that case leaks credits and can deadlock.
                //
                // Instead, rely on LLR's explicit resend mode: when a VC is resending on this
                // port, packets transmitted are retransmissions and must not trigger credit return.
                const Mac48Address peerMac = GetRemoteMac();
                if (m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager)
                {
                    if (m_llrSwitchPortManager->IsLlrResending(creditReturnVcId, peerMac))
                    {
                        scheduleCreditReturn = false;
                    }
                }
                else if ((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager)
                {
                    if (m_llrNodeManager->IsLlrResending(creditReturnVcId))
                    {
                        scheduleCreditReturn = false;
                    }
                }
            }
        }

        bool result = m_channel->TransmitStart(p, this, txTime);
        if (result == false)
        {
            m_phyTxDropTrace(p);
            return false;
        }

        if (scheduleCreditReturn)
        {
            Simulator::Schedule(txCompleteTime, [this, creditReturnTargetMac, creditReturnVcId, creditReturnPktSize]() {
                if (m_cbfcManager)
                {
                    EthernetHeader tempEthHeader;
                    tempEthHeader.SetSource(creditReturnTargetMac);
                    m_cbfcManager->HandleCreditReturn(tempEthHeader, creditReturnVcId, creditReturnPktSize);
                    m_cbfcManager->CreditReturn(creditReturnTargetMac, creditReturnVcId);
                }
            });
        }

        return true;
    }

    // Core function to check all queues and trigger transmission
    void
    PointToPointSueNetDevice::TryTransmit()
    {
        // Enhanced time-based transmission control
        if (m_txMachineState != READY)
        {
            return;
        }

	        // 1. LLR retransmissions have the highest priority so recovery can proceed even if
	        // the main queue is continuously populated (e.g., CBFC credit updates under loss).
        if (m_llrEnabled)
        {
            static uint8_t lastResendVc = 0;
            const uint8_t llrNumVcs = GetLlrNumVcs();
            for (uint8_t i = 0; i < llrNumVcs; ++i)
            {
                uint8_t currentVc = (lastResendVc + i) % llrNumVcs;
                Ptr<Packet> resendPacket;

	                if (m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager)
	                {
	                    resendPacket = m_llrSwitchPortManager->LlrResendPacket(currentVc, GetRemoteMac());
	                }
	                else if ((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager)
	                {
	                    resendPacket = m_llrNodeManager->LlrResendPacket(currentVc);
	                }

		                if (resendPacket)
		                {
		                    // Retransmissions:
		                    // - Data retransmissions should still respect CBFC to avoid overrunning
		                    //   receiver buffers.
		                    // - Control retransmissions (CBFC update/sync, ACK/NACK) must *bypass*
		                    //   CBFC gating, otherwise control-plane loss can deadlock the fabric:
		                    //   credits become low -> cannot resend credit updates -> credits never recover.
		                    bool isControlPacket = false;
		                    {
		                        SuePppHeader ppp;
		                        if (resendPacket->PeekHeader(ppp))
		                        {
		                            const uint16_t proto = ppp.GetProtocol();
		                            isControlPacket =
		                                (proto == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_UPDATE) ||
		                                 proto == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_SYNC) ||
		                                 proto == SuePacketUtils::EtherToPpp(SuePacketUtils::ACK_REV) ||
		                                 proto == SuePacketUtils::EtherToPpp(SuePacketUtils::NACK_REV));
		                        }
		                    }

		                    // Avoid starving the control plane:
		                    // If there are queued control packets in the main queue (CBFC updates/sync, ACK/NACK),
		                    // prioritize them over *data* retransmissions. Otherwise, persistent loss can
		                    // keep the fabric in a state where credits/acks are never delivered, leading to
		                    // deadlocks and watchdog kills.
		                    if (!isControlPacket && !m_queue->IsEmpty())
		                    {
		                        continue;
		                    }

                    // Retransmissions reuse the original credit reservation; do NOT
                    // re-check or re-consume CBFC credits here. Otherwise, control-loss
                    // or credit-update drops can permanently stall LLR recovery.

                    lastResendVc = (currentVc + 1) % llrNumVcs;
	                    m_snifferTrace(resendPacket);
	                    m_promiscSnifferTrace(resendPacket);
	                    TransmitStart(resendPacket);
	                    return;
	                }
	            }
	        }

        // 2. Prioritize checking high-priority main queue (for credit packets / ACK / NACK)
        if (!m_queue->IsEmpty())
        {
            Ptr<Packet> packet = m_queue->Dequeue();
            SuePppHeader ppp;
            packet->PeekHeader(ppp);

            if (m_loggingEnabled)
            {
                // Trigger main queue statistics (event-driven after main queue dequeue)
                SueStatsUtils::ProcessMainQueueStats(m_queue, GetNode()->GetId(), GetIfIndex() - 1);
            }

            if ((!IsSwitchDevice(m_address)) &&
                (ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_UPDATE) ||
                 ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_SYNC)))
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

        // 3. Poll all VC queues (weighted round robin)
        static uint8_t lastVC = 0;
        // If main queue is empty, poll all VC queues
        // TODO link layer
        for (uint8_t i = 0; i < m_numVcs; ++i)
        {
            uint8_t currentVC = (lastVC + i) % m_numVcs;
            if (m_queueManager && !m_queueManager->IsVcQueueEmpty(currentVC))
            {
                // LLR send-side gating:
                // - While recovering from a loss (resending), do not inject new packets.
                // - Enforce a per-VC send window to avoid massive out-of-order storms under loss.
                if (m_llrEnabled)
                {
                    Mac48Address peerMac = GetRemoteMac ();
                    bool resending = false;
                    bool withinWindow = true;

                    Ptr<Queue<Packet>> vcQueue = m_queueManager->GetVcQueue (currentVC);
                    Ptr<const Packet> headPacket = vcQueue ? vcQueue->Peek () : nullptr;
                    SueTag tag;
                    if (headPacket && headPacket->PeekPacketTag (tag))
                    {
                        const uint32_t seq = tag.GetSequence ();
                        if (m_switch && IsSwitchDevice (m_address) && m_llrSwitchPortManager)
                        {
                            resending = m_llrSwitchPortManager->IsLlrResending (currentVC, peerMac);
                            withinWindow = m_llrSwitchPortManager->IsWithinSendWindow (currentVC, seq, peerMac);
                        }
                        else if ((!m_switch || !IsSwitchDevice (m_address)) && m_llrNodeManager)
                        {
                            resending = m_llrNodeManager->IsLlrResending (currentVC);
                            withinWindow = m_llrNodeManager->IsWithinSendWindow (currentVC, seq);
                        }
                    }

                    if (resending || !withinWindow)
                    {
                        continue;
                    }
                }

                // Check CBFC only if enabled
                if (m_enableLinkCBFC)
                {
                    // CBFC enabled: check dynamic credits before sending
                    uint32_t packetSize = m_queueManager->GetFirstPacketSize(currentVC);
	                    if (packetSize > 0 && m_cbfcManager->HasEnoughCredits(GetRemoteMac(), currentVC, packetSize))
	                    {
	                        // Dequeue packet and consume credits based on packet size
	                        Ptr<Packet> packet = m_queueManager->DequeueFromVcQueue(currentVC);
	                        if (packet && m_cbfcManager->ConsumeDynamicCredits(GetRemoteMac(), currentVC, packet->GetSize()))
	                        {
	                            if (m_llrEnabled)
	                            {
	                                SueTag tag;
	                                if (packet->PeekPacketTag(tag))
	                                {
	                                    const uint32_t seq = tag.GetSequence();
	                                    Mac48Address peerMac = GetRemoteMac();
	                                    if (m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager)
	                                    {
	                                        m_llrSwitchPortManager->MarkPacketSent(currentVC, seq, peerMac);
	                                    }
	                                    else if ((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager)
	                                    {
	                                        m_llrNodeManager->MarkPacketSent(currentVC, seq);
	                                    }
	                                }
	                            }

	                            if (m_loggingEnabled)
	                            {
	                                // Process VC queue delay statistics
	                                SueStatsUtils::ProcessVcQueueDelayStats(packet, GetNode()->GetId(), GetIfIndex() - 1);

                                SueStatsUtils::ProcessCreditChangeStats(GetRemoteMac(), currentVC, m_cbfcManager->GetTxCredits(GetRemoteMac(), currentVC), GetNode()->GetId(), GetIfIndex() - 1);

                                // Trigger VC queue statistics (event-driven after VC dequeue)
                                SueStatsUtils::ProcessVCQueueStats(m_queueManager, m_cbfcManager,
                                                                 m_numVcs, m_vcQueueMaxBytes,
                                                                 GetNode()->GetId(), GetIfIndex() - 1);
                            }

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
                    // No credits available, continue to next VC
                }
                else
                {
                    // CBFC disabled: send packet directly
                    Ptr<Packet> packet = m_queueManager->DequeueFromVcQueue(currentVC);

                    if (m_loggingEnabled)
                    {
                        // Process VC queue delay statistics
                        SueStatsUtils::ProcessVcQueueDelayStats(packet, GetNode()->GetId(), GetIfIndex() - 1);

                        // Trigger VC queue statistics (event-driven after VC dequeue)
                        SueStatsUtils::ProcessVCQueueStats(m_queueManager, m_cbfcManager,
                                                         m_numVcs, m_vcQueueMaxBytes,
                                                         GetNode()->GetId(), GetIfIndex() - 1);
                    }

                    if (!IsSwitchDevice(m_address))
                    {
                        NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex() << "] sending packet for VC " << (uint32_t)currentVC
                                                << " (CBFC disabled, VC queue size now: " << m_queueManager->GetVcQueueSize(currentVC) << " packets)");
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

        // Check if there are packets to process and schedule next iteration
        bool hasPacketsToProcess = (!m_queue->IsEmpty());
        if (!hasPacketsToProcess) {
            // Check VC queues
            for (uint8_t i = 0; i < m_numVcs; ++i) {
                if (m_queueManager && !m_queueManager->IsVcQueueEmpty(i)) {
                    hasPacketsToProcess = true;
                    break;
                }
            }
        }
        if (!hasPacketsToProcess && m_llrEnabled)
        {
            Mac48Address peerMac = GetRemoteMac();
            for (uint8_t i = 0; i < m_numVcs; ++i)
            {
                if (m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager)
                {
                    if (m_llrSwitchPortManager->IsLlrResending(i, peerMac))
                    {
                        hasPacketsToProcess = true;
                        break;
                    }
                }
                else if ((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager)
                {
                    if (m_llrNodeManager->IsLlrResending(i))
                    {
                        hasPacketsToProcess = true;
                        break;
                    }
                }
            }
        }

        if (hasPacketsToProcess) {
            Simulator::Schedule(m_vcSchedulingDelay, &PointToPointSueNetDevice::TryTransmit, this);
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


    bool PointToPointSueNetDevice::FindDeviceAndSend(Ptr<Packet> packet, Mac48Address targetMac, uint16_t protocolNum)
    {
        // For non-CBFC packets (e.g., LLR ACK/NACK), this function should behave as a plain
        // "find device and send" helper without touching CBFC credit state.
        if (protocolNum != SuePacketUtils::PROT_CBFC_UPDATE)
        {
            if (targetMac == GetRemoteMac())
            {
                return Send(packet->Copy(), GetRemote(), protocolNum);
            }

            for (uint32_t i = 0; i < GetNode()->GetNDevices(); i++)
            {
                Ptr<NetDevice> dev = GetNode()->GetDevice(i);
                Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice>(dev);
                if (!p2pDev)
                {
                    continue;
                }

                Mac48Address mac = Mac48Address::ConvertFrom(p2pDev->GetAddress());
                if (mac == targetMac)
                {
                    // When bypassing the channel and delivering directly to another NetDevice on the same node
                    // (e.g., switch internal forwarding), ensure the PPP header is present. In normal channel
                    // transmission, Send() would have added it.
                    Ptr<Packet> toDeliver = packet->Copy();
                    SuePppHeader ppp;
                    bool hasPppHeader = false;
                    if (toDeliver->GetSize() >= ppp.GetSerializedSize())
                    {
                        toDeliver->PeekHeader(ppp);
                        const uint16_t proto = ppp.GetProtocol();
                        hasPppHeader = (proto == 0x0021 /*IPv4*/ || proto == 0x0057 /*IPv6*/ ||
                                        proto == 0x00FB /*CBFC_UPDATE*/ || proto == 0x00FC /*CBFC_SYNC*/ ||
                                        proto == 0x1111 /*LLR_ACK*/ || proto == 0x2222 /*LLR_NACK*/);
                    }
                    if (!hasPppHeader)
                    {
                        AddHeader(toDeliver, protocolNum);
                    }

                    Simulator::ScheduleNow(&PointToPointSueNetDevice::CompleteCreditSend, this, p2pDev, toDeliver);
                    return true;
                }
            }

            return false;
        }

        SueCbfcHeader creditHeader;
        packet->PeekHeader(creditHeader);
        uint8_t vcId = creditHeader.GetVcId();
        uint32_t credits = creditHeader.GetCredits();

        // First check if it's credit to be returned to the peer device
        if (targetMac == GetRemoteMac())
        {
            bool success = Send(packet->Copy(), GetRemote(), protocolNum);
            if (success && m_cbfcManager)
            {
                m_cbfcManager->ClearCreditsToReturn(targetMac, vcId);
            }
            return success;
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
                AddHeader(packet, SuePacketUtils::PROT_CBFC_UPDATE);
                // Calculate processing time based on packet processing rate plus a fixed per-packet overhead.
                Time processingTime =
                    m_processingDelay + m_processingRate.CalculateBytesTxTime(packet->GetSize());
                if (m_loggingEnabled)
                {
                    SueStatsUtils::ProcessCreditSendStats(targetMac, vcId, credits, GetNode()->GetId(), GetIfIndex() - 1);
                }

                if (m_cbfcManager)
                {
                    m_cbfcManager->ClearCreditsToReturn(targetMac, vcId);
                }

                // Schedule credit processing with delay
                Simulator::Schedule(processingTime + m_switchForwardDelay, &PointToPointSueNetDevice::CompleteCreditSend,
                                   this, p2pDev, packet->Copy());

                // Found device and scheduled successfully
                return true;
            }
        }
        return false;
    }

    void PointToPointSueNetDevice::CompleteCreditSend(Ptr<PointToPointSueNetDevice> targetDevice, Ptr<Packet> packet)
    {
        NS_LOG_FUNCTION(this << targetDevice << packet);
        targetDevice->ReceiveInternal(packet);
    }

    void
    PointToPointSueNetDevice::Receive (Ptr<Packet> packet)
    {
        ReceiveCommon (packet, true);
    }

    void
    PointToPointSueNetDevice::ReceiveInternal (Ptr<Packet> packet)
    {
        ReceiveCommon (packet, false);
    }

    void
    PointToPointSueNetDevice::ReceiveCommon (Ptr<Packet> packet, bool applyErrorModel)
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

        SuePppHeader ppp;
        packet->PeekHeader(ppp);
        bool isSyncPacket = (ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_SYNC));
        bool isControlPacket = (ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_UPDATE) ||
                                isSyncPacket ||
                                ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::ACK_REV) ||
                                ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::NACK_REV));

        bool shouldApplyErrorModel = (applyErrorModel && m_receiveErrorModel &&
                                      (m_errorModelApplyToControlPackets || !isControlPacket) &&
                                      (!isSyncPacket || m_errorModelApplyToSyncPackets));

        if (shouldApplyErrorModel && m_receiveErrorModel->IsCorrupt(packet))
        {
            if (m_loggingEnabled)
            {
                SueStatsUtils::ProcessPacketDropStats(packet, GetNode()->GetId(), GetIfIndex() - 1, "ErrorModelDrop");
            }
            m_phyRxDropTrace(packet);
            return;
        }

        m_snifferTrace(packet);
        m_promiscSnifferTrace(packet);
        m_phyRxEndTrace(packet);
        Ptr<Packet> originalPacket = packet->Copy();

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


        const bool isCbfcUpdate = (ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_UPDATE));
        const bool isCbfcSync = (ppp.GetProtocol() == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_SYNC));

        if (isCbfcUpdate || isCbfcSync)
        { // If it's an update packet

            packet->RemoveHeader(ppp);
            SueCbfcHeader creditHeader;
            packet->RemoveHeader(creditHeader);
            EthernetHeader ethHeader;
            packet->RemoveHeader(ethHeader);

            uint8_t vcId = creditHeader.GetVcId();
            uint32_t credits = creditHeader.GetCredits();
            Mac48Address sourceMac = ethHeader.GetSource();

            // LLR reliability for CBFC control packets (so credits won't be lost under link loss)
            //
            // IMPORTANT: When protection is enabled, sequence control packets on a dedicated
            // LLR VC to avoid data/control reordering (control packets ride the main queue
            // and can overtake data in VC queues). Mixing them in the same LLR sequence can
            // create persistent out-of-order stalls.
            if (m_llrEnabled && m_llrProtectCbfcUpdates)
            {
                SueTag tag;
                if (!packet->PeekPacketTag(tag))
                {
                    NS_LOG_WARN("Receive: CBFC update packet has no SueTag; skipping LLR processing");
                }
                else
                {
                    uint32_t seq = tag.GetSequence();
                    bool shouldProcess = true;
                    const uint8_t llrVcId = GetLlrControlVcId();
                    if (m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager)
                    {
                        shouldProcess = m_llrSwitchPortManager->LlrReceivePacket(packet, llrVcId, seq, sourceMac);
                    }
                    else if ((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager)
                    {
                        shouldProcess = m_llrNodeManager->LlrReceivePacket(packet, llrVcId, sourceMac, seq);
                    }

                    if (!shouldProcess)
                    {
                        // For CBFC control packets, we do NOT drop credit updates on LLR
                        // out-of-order/duplicate detection. Credits are critical for forward
                        // progress; dropping them can permanently stall the fabric, especially
                        // under loss or heavy control traffic. LLR still provides ACK/NACK to
                        // the sender, but we allow the credit update to be applied regardless.
                        //
                        // Credit sync packets are idempotent (overwrite), and periodic sync will
                        // correct any drift if a duplicate update temporarily inflates credits.
                    }
                }
            }

            // Do not count internal switch credit reception
            if (!IsSwitchDevice(GetLocalMac()) || !IsSwitchDevice(sourceMac))
            { // XPU or switch egress port
                Time processingTime =
                    m_processingDelay + m_processingRate.CalculateBytesTxTime(originalPacket->GetSize());
                if (m_loggingEnabled)
                {
                    // Schedule processing completion event
                    Simulator::Schedule(processingTime, [this, originalPacket]() {
                        SueStatsUtils::ProcessReceivedPacketStats(originalPacket, GetNode()->GetId(), GetIfIndex() - 1);
                    });
                }
            }

            if (m_cbfcManager)
            {
                if (isCbfcSync)
                {
                    // Sync semantics: overwrite sender-side credits to reflect the receiver's
                    // *current available* credit budget (derived from receiver processing-queue
                    // free bytes). This repairs credit drift/leaks and avoids permanent stalls.
                    m_cbfcManager->SetTxCredits(sourceMac, vcId, credits);
                    if (m_loggingEnabled)
                    {
                        SueStatsUtils::ProcessCreditChangeStats(sourceMac, vcId,
                                                               m_cbfcManager->GetTxCredits(sourceMac, vcId),
                                                               GetNode()->GetId(), GetIfIndex() - 1);
                    }
                }
                else if (credits > 0)
                {
                    // Update semantics: add credits (delta).
                    m_cbfcManager->AddTxCredits(sourceMac, vcId, credits);
                    if (m_loggingEnabled)
                    {
                        SueStatsUtils::ProcessCreditReceptionStats(sourceMac, vcId, credits,
                                                                  GetNode()->GetId(), GetIfIndex() - 1);
                        SueStatsUtils::ProcessCreditChangeStats(sourceMac, vcId,
                                                               m_cbfcManager->GetTxCredits(sourceMac, vcId),
                                                               GetNode()->GetId(), GetIfIndex() - 1);
                    }
                    if (!IsSwitchDevice(m_address))
                    {
                        NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                                  << "] received " << credits
                                                  << " credits for VC " << (uint32_t)vcId
                                                  << ". Total now: " << m_cbfcManager->GetTxCredits(sourceMac, vcId));
                    }
                }
            }

            if (m_txMachineState == READY)
            {
                m_tryTransmitEvent = Simulator::Schedule(m_vcSchedulingDelay,
                                                         &PointToPointSueNetDevice::TryTransmit,
                                                         this);
            }
            return;
        }
        else
        { // If it's a data packet
            packet->RemoveHeader(ppp);

            // Extract VC ID from packet
            uint8_t vcId = SuePacketUtils::ExtractVcIdFromPacket(packet);
            uint16_t protocol = SuePacketUtils::PppToEther(ppp.GetProtocol());
            EthernetHeader ethHeader;
            packet->PeekHeader(ethHeader);
            Mac48Address mac = ethHeader.GetSource();

            // Read sequence from tag (required for LLR)
            SueTag tag;
            if (!packet->PeekPacketTag(tag)) {
                if (m_loggingEnabled)
                {
                    SueStatsUtils::ProcessPacketDropStats(packet, GetNode()->GetId(), GetIfIndex() - 1, "MissingSueTag");
                }
                NS_LOG_WARN("Receive: no tag found, cannot process packet (MissingSueTag)");
                m_phyRxDropTrace(packet);
                return;
            }

            uint32_t seq = tag.GetSequence();
	            NS_LOG_DEBUG("Receive: read seq " << seq << " from tag (linkType=" << (uint32_t)tag.GetLinkType() << ")");

	            // Admission control: ensure we never emit LLR ACK/NACK for a packet we cannot buffer.
	            const uint32_t packetSize = packet->GetSize();
	            if (m_currentProcessingQueueBytes + packetSize > m_processingQueueMaxBytes)
            {
                // Admission control drop: do not advance LLR receiver state, but do provide
                // fast feedback (ACK for duplicates / NACK(expected) otherwise) to avoid
                // timeout-only recovery under sustained overload.
                if (m_llrEnabled)
                {
                    if (m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager)
                    {
                        m_llrSwitchPortManager->LlrNotifyDropBeforeReceive(vcId, seq, mac);
                    }
                    else if ((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager)
                    {
                        m_llrNodeManager->LlrNotifyDropBeforeReceive(vcId, seq, mac);
                    }
                }

                if (m_loggingEnabled)
                {
                    SueStatsUtils::ProcessPacketDropStats(packet, GetNode()->GetId(), GetIfIndex() - 1, "ProcessingQueueFull");
                }
                NS_LOG_INFO("Receive processing queue full! DROPPED packet on VC " << (uint32_t)vcId);
                m_phyRxDropTrace(packet);
	                return;
	            }

		            // LLR related processing, send ACK or NACK packet
			            const bool switchDropMode = (m_switch && IsSwitchDevice(m_address) &&
			                                         m_switch->GetEgressOverflowPolicy() == SueSwitch::EgressOverflowPolicy::DROP);
			            // LLR receive-side placement under switch DROP:
			            // - Strictly, to avoid ACK/NACK for packets later dropped at switch egress admission,
			            //   LLR should run after egress admission.
			            // - Practically, our switch ingress pipeline may reorder packets (HoL rotation),
			            //   and delaying LLR can cause spurious NACK/timeout storms even under ErrorRate=0.
			            //
			            // Therefore we allow early LLR processing in switch DROP mode only when:
			            // - link CBFC is enabled (we rely on the invariant that correctly-provisioned
			            //   CBFC prevents switch egress admission overflow in steady-state), AND
			            // - we are NOT running "control-loss protect" experiments (LlrProtectCbfcUpdates=true),
			            //   where we explicitly want DROP semantics to manifest as real drops and be
			            //   recovered by LLR/credit-sync. In that regime, emitting ACK/NACK before
			            //   switch egress admission can break correctness (ACK for a packet that later
			            //   gets dropped), so we defer LLR receive processing to post-admission.
			            const bool allowEarlyLlrInSwitchDropMode =
			                (switchDropMode && m_enableLinkCBFC && !m_llrProtectCbfcUpdates);
			            if (m_llrEnabled && (!switchDropMode || allowEarlyLlrInSwitchDropMode)){
			                bool shouldProcess = false;
			                if(m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager){
			                    shouldProcess = m_llrSwitchPortManager->LlrReceivePacket(packet, vcId, seq, mac);
			                } else if((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager){
			                    shouldProcess = m_llrNodeManager->LlrReceivePacket(packet, vcId, mac, seq);
		                }
		                if(!shouldProcess){
		                    // LLR dropped a duplicate/out-of-order packet before we enqueue it.
		                    // Under CBFC, this packet should not consume receiver buffering, so
		                    // return credits immediately to avoid credit leaks under retransmission.
		                    if (m_enableLinkCBFC && m_cbfcManager)
		                    {
		                        m_cbfcManager->HandleCreditReturn(ethHeader, vcId, originalPacket->GetSize());
		                        m_cbfcManager->CreditReturn(ethHeader.GetSource(), vcId);
		                    }
		                    return; // Packet was discarded by LLR (duplicate/out-of-order)
		                }
		            }

            // Put into processing queue (capacity already checked above).
            ProcessItem item = {originalPacket, packet, vcId, protocol};
            EnqueueToProcessingQueue(item);
        }
    }
    void PointToPointSueNetDevice::EnqueueToProcessingQueue(ProcessItem item)
    {
        SueTag::AddProcessingQueueDelayTag(item.packet);

        m_processingQueue.push(item);
        m_currentProcessingQueueSize++;
        m_currentProcessingQueueBytes += item.packet->GetSize();

        if (m_loggingEnabled)
        {
            SueStatsUtils::ProcessProcessingQueueStats(m_currentProcessingQueueBytes, m_processingQueueMaxBytes, GetNode()->GetId(), GetIfIndex() - 1);
            SueStatsUtils::ProcessReceivedPacketStats(item.originalPacket, GetNode()->GetId(), GetIfIndex() - 1);
        }
        
        if (!m_processingScheduled){
            Simulator::Schedule(m_processingQueueScheduleDelay, &PointToPointSueNetDevice::StartProcessing, this);
        }
    }

    void PointToPointSueNetDevice::StartProcessing()
    {
        if(m_processingQueue.empty()){
            return;
        }

        if(!m_processingScheduled){
            const ProcessItem &item = m_processingQueue.front();
            Time processingTime =
                m_processingRate.CalculateBytesTxTime(item.packet->GetSize());
            Simulator::Schedule(processingTime, &PointToPointSueNetDevice::ProcessingReceivedPacket, this);
            m_processingScheduled = true;           
        }
    }    

	    void PointToPointSueNetDevice::ProcessingReceivedPacket()
	    {
	        if (m_processingQueue.empty())
	        {
	            m_processingScheduled = false;
	            return;
	        }

        m_processingScheduled = false;

        // Actually process packet
        ProcessItem &item = m_processingQueue.front();
        if (!m_promiscCallback.IsNull())
        {
            m_macPromiscRxTrace(item.originalPacket);
            m_promiscCallback(this, item.packet, item.protocol, GetRemote(), GetAddress(), NetDevice::PACKET_HOST);
        }

	        // Switch forwarding logic - delegate to SueSwitch module
	        EthernetHeader ethHeader;
	        item.packet->PeekHeader(ethHeader);
	        const Mac48Address ingressSourceMac = ethHeader.GetSource();
	        const uint32_t ingressOnWireSizeBytes = item.originalPacket ? item.originalPacket->GetSize() : item.packet->GetSize();

	        auto ReturnIngressLinkCreditsIfNeeded = [&]() {
	            if (!m_enableLinkCBFC || !m_cbfcManager)
	            {
	                return;
	            }
	            // The sender consumed credits based on the on-wire packet size (PPP included).
	            // For switch ports, the PPP header was removed during ReceiveCommon() before
	            // enqueueing into the processing queue, so use the original packet copy.
	            m_cbfcManager->HandleCreditReturn(ethHeader, item.vcId, ingressOnWireSizeBytes);
	            m_cbfcManager->CreditReturn(ingressSourceMac, item.vcId);
	        };

		        // Check if this device is a switch device and forward accordingly
		        if (IsSwitchDevice(m_address))
		        {
            // In DROP mode, do not run LLR ReceiveCommon until we know this packet is admitted
            // w.r.t. the switch egress VC queue. Otherwise we could emit ACK/NACK for a packet
            // that is later dropped due to egress admission failure, violating correctness.
            //
            // We also store admission state in the queued ProcessItem so that if forwarding is
            // temporarily blocked (e.g., switch-internal CBFC), we do not re-run LLR/admission
            // when the item is retried.
		            if (m_switch &&
		                m_switch->GetEgressOverflowPolicy() == SueSwitch::EgressOverflowPolicy::DROP &&
		                !item.switchDropAdmissionDone)
		            {
	                // If switch-internal CBFC is enabled, we must ensure the packet is actually
	                // forwardable (credits available) *before* we reserve egress VC queue bytes.
	                // Otherwise we may reserve egress capacity for a packet that is later blocked
	                // by internal CBFC, eventually causing avoidable drops in DROP mode.
	                bool blockedByInternalCbfc = false;
	                Mac48Address internalCbfcPeerMac;
	                uint32_t internalCbfcBytes = 0;

	                const Mac48Address dst = ethHeader.GetDestination();
	                uint32_t outPortIndex = 0;
	                const bool found = m_switch->LookupOutPortIndex(dst, &outPortIndex);
	                NS_ASSERT_MSG(found, "No forwarding entry for destination " << dst);

	                if (outPortIndex != GetIfIndex())
	                {
	                    Ptr<PointToPointSueNetDevice> outDev = m_switch->FindPortDevice(GetNode(), outPortIndex);
	                    SuePppHeader pppProj;
	                    const uint32_t projectedBytes = item.packet->GetSize() + pppProj.GetSerializedSize();

	                    if (m_cbfcManager && m_cbfcManager->IsLinkCbfcEnabled())
	                    {
	                        if (!outDev)
	                        {
	                            // No egress device: let the normal admission path treat it as an error/drop.
	                            blockedByInternalCbfc = false;
	                        }
	                        else
	                        {
	                            internalCbfcPeerMac = Mac48Address::ConvertFrom(outDev->GetAddress());
	                            internalCbfcBytes = projectedBytes;
	                            if (!m_cbfcManager->HasEnoughCredits(internalCbfcPeerMac, item.vcId, internalCbfcBytes))
	                            {
	                                blockedByInternalCbfc = true;
	                            }
	                        }
	                    }

	                    if (blockedByInternalCbfc)
	                    {
	                        // Do not admit / ACK/NACK yet. Rotate the processing queue to reduce HoL
	                        // and retry later when credits are returned by the egress port.
	                        if (!m_processingQueue.empty())
	                        {
	                            ProcessItem blocked = m_processingQueue.front();
	                            m_processingQueue.pop();
	                            m_processingQueue.push(blocked);
	                        }
	                        if (!m_processingQueue.empty())
	                        {
	                            Simulator::Schedule(m_processingQueueScheduleDelay, &PointToPointSueNetDevice::StartProcessing, this);
	                        }
	                        return;
	                    }

			                    if (!outDev || !m_switch->TryReserveEgressVcQueueBytes(outPortIndex, item.vcId, projectedBytes, outDev))
			                    {
			                        // Switch egress admission failed.
			                        //
			                        // - If we already ran early LLR receive processing at ReceiveCommon()
			                        //   (switch DROP + CBFC + NOT control-loss-protect), dropping here would
			                        //   violate correctness (ACK/NACK may already be emitted). In that case,
			                        //   we treat the failure as transient backpressure: rotate and retry.
			                        //
			                        // - Otherwise (no early LLR), we can model the real DROP consequence:
			                        //   drop the packet here and rely on LLR (and/or credit sync) to recover.
			                        const bool switchDropMode =
			                            (m_switch && m_switch->GetEgressOverflowPolicy() == SueSwitch::EgressOverflowPolicy::DROP);
			                        const bool earlyLlrProcessed =
			                            (m_llrEnabled && switchDropMode && m_enableLinkCBFC && !m_llrProtectCbfcUpdates);
			                        if (earlyLlrProcessed)
			                        {
			                            if (!m_processingQueue.empty())
			                            {
			                                ProcessItem blocked = m_processingQueue.front();
			                                m_processingQueue.pop();
			                                m_processingQueue.push(blocked);
			                            }
			                            if (!m_processingQueue.empty())
			                            {
			                                Simulator::Schedule(m_processingQueueScheduleDelay,
			                                                   &PointToPointSueNetDevice::StartProcessing,
			                                                   this);
			                            }
			                            return;
			                        }

			                        // Switch egress admission drop under DROP mode (no early LLR):
			                        // do not advance LLR receiver state, but do send fast feedback so the
			                        // upstream can recover without waiting for timeout.
			                        if (m_llrEnabled && m_llrSwitchPortManager)
			                        {
			                            SueTag tag;
			                            if (item.packet->PeekPacketTag(tag))
			                            {
			                                const uint32_t seq = tag.GetSequence();
			                                const Mac48Address src = ethHeader.GetSource();
			                                m_llrSwitchPortManager->LlrNotifyDropBeforeReceive(item.vcId, seq, src);
			                            }
			                        }

			                        if (m_loggingEnabled)
			                        {
		                            SueStatsUtils::ProcessPacketDropStats(item.packet,
		                                                                 GetNode()->GetId(),
		                                                                 GetIfIndex() - 1,
		                                                                 "SwitchEgressVcQueueFull");
			                        }
			                        NS_LOG_INFO("ProcessingReceivedPacket: switch egress VC queue full! DROPPED packet on VC "
			                                    << static_cast<uint32_t>(item.vcId) << " dst=" << dst
			                                    << " outPortIndex=" << outPortIndex);
			                        m_phyRxDropTrace(item.packet);

			                        // Packet is being dropped from the switch ingress processing queue:
			                        // return ingress link credits so the upstream sender can continue.
			                        ReturnIngressLinkCreditsIfNeeded();

			                        const uint32_t queuedBytes = item.packet->GetSize();
			                        m_processingQueue.pop();
			                        m_currentProcessingQueueSize--;
			                        m_currentProcessingQueueBytes -= queuedBytes;
			                        if (m_loggingEnabled)
		                        {
		                            SueStatsUtils::ProcessProcessingQueueStats(m_currentProcessingQueueBytes,
		                                                                      m_processingQueueMaxBytes,
		                                                                      GetNode()->GetId(),
		                                                                      GetIfIndex() - 1);
		                        }

		                        if(!m_processingQueue.empty()){
		                            Simulator::Schedule(m_processingQueueScheduleDelay, &PointToPointSueNetDevice::StartProcessing, this);
		                        }
		                        return;
		                    }

	                    item.switchDropReservedOutPortIndex = outPortIndex;
	                    item.switchDropReservedBytes = projectedBytes;

	                    // Switch-internal CBFC (ingress -> egress): once we admit (reserve) bytes
	                    // against the egress VC queue, consume internal credits immediately so
	                    // we don't over-admit and later get blocked in SueSwitch forwarding.
	                    if (m_cbfcManager && m_cbfcManager->IsLinkCbfcEnabled())
	                    {
	                        NS_ASSERT_MSG(internalCbfcBytes == projectedBytes,
	                                      "internal CBFC byte accounting mismatch");
	                        if (!m_cbfcManager->ConsumeDynamicCredits(internalCbfcPeerMac, item.vcId, internalCbfcBytes))
	                        {
	                            // Should not happen because HasEnoughCredits() succeeded; treat as
	                            // a temporary block. Release the admission reservation and retry later.
	                            m_switch->ReleaseEgressVcQueueBytes(outPortIndex, item.vcId, projectedBytes);
	                            item.switchDropReservedBytes = 0;
	                            item.switchDropReservedOutPortIndex = 0;
	                            if (!m_processingQueue.empty())
	                            {
	                                ProcessItem blocked = m_processingQueue.front();
	                                m_processingQueue.pop();
	                                m_processingQueue.push(blocked);
	                            }
	                            if (!m_processingQueue.empty())
	                            {
	                                Simulator::Schedule(m_processingQueueScheduleDelay, &PointToPointSueNetDevice::StartProcessing, this);
	                            }
	                            return;
	                        }
	                        item.switchDropInternalCreditsConsumed = true;
	                        item.switchDropInternalCreditPeerMac = internalCbfcPeerMac;
	                        item.switchDropInternalCreditBytes = internalCbfcBytes;
	                    }
	                }

		                    // When we allow early LLR ReceiveCommon in switch DROP mode (CBFC-enabled),
		                    // do not re-run LLR processing here.
			                const bool switchDropMode =
			                    (m_switch && m_switch->GetEgressOverflowPolicy() == SueSwitch::EgressOverflowPolicy::DROP);
			                const bool earlyLlrProcessed =
			                    (m_llrEnabled && switchDropMode && m_enableLinkCBFC && !m_llrProtectCbfcUpdates);
	                if (m_llrEnabled && !earlyLlrProcessed)
	                {
	                    SueTag tag;
	                    if (!item.packet->PeekPacketTag(tag))
	                    {
                        if (m_loggingEnabled)
                        {
                            SueStatsUtils::ProcessPacketDropStats(item.packet, GetNode()->GetId(), GetIfIndex() - 1, "MissingSueTag");
                        }
                        NS_LOG_WARN("ProcessingReceivedPacket: no tag found, cannot process packet (MissingSueTag)");
                        m_phyRxDropTrace(item.packet);

	                        if (item.switchDropReservedBytes > 0)
	                        {
	                            m_switch->ReleaseEgressVcQueueBytes(item.switchDropReservedOutPortIndex,
	                                                                item.vcId,
	                                                                item.switchDropReservedBytes);
	                        }
	                        if (item.switchDropInternalCreditsConsumed && m_cbfcManager)
	                        {
	                            const uint32_t credits =
	                                m_cbfcManager->CalculateCreditsForPacket(item.switchDropInternalCreditBytes);
	                            m_cbfcManager->AddTxCredits(item.switchDropInternalCreditPeerMac,
	                                                       item.vcId,
	                                                       credits);
	                            item.switchDropInternalCreditsConsumed = false;
	                        }

	                        const uint32_t queuedBytes = item.packet->GetSize();
	                        m_processingQueue.pop();
                        m_currentProcessingQueueSize--;
                        m_currentProcessingQueueBytes -= queuedBytes;
                        if (m_loggingEnabled)
                        {
                            SueStatsUtils::ProcessProcessingQueueStats(m_currentProcessingQueueBytes,
                                                                      m_processingQueueMaxBytes,
                                                                      GetNode()->GetId(),
                                                                      GetIfIndex() - 1);
                        }

                        if(!m_processingQueue.empty()){
                            Simulator::Schedule(m_processingQueueScheduleDelay, &PointToPointSueNetDevice::StartProcessing, this);
                        }
                        return;
                    }

                    const uint32_t seq = tag.GetSequence();
                    const Mac48Address src = ethHeader.GetSource();

	                    bool shouldProcess = false;
	                    if (m_llrSwitchPortManager)
	                    {
	                        shouldProcess = m_llrSwitchPortManager->LlrReceivePacket(item.packet, item.vcId, seq, src);
	                    }

	                    if (!shouldProcess)
	                    {
	                        if (item.switchDropReservedBytes > 0)
	                        {
	                            m_switch->ReleaseEgressVcQueueBytes(item.switchDropReservedOutPortIndex,
	                                                                item.vcId,
	                                                                item.switchDropReservedBytes);
	                        }
	                        if (item.switchDropInternalCreditsConsumed && m_cbfcManager)
	                        {
	                            const uint32_t credits =
	                                m_cbfcManager->CalculateCreditsForPacket(item.switchDropInternalCreditBytes);
	                            m_cbfcManager->AddTxCredits(item.switchDropInternalCreditPeerMac,
	                                                       item.vcId,
	                                                       credits);
	                            item.switchDropInternalCreditsConsumed = false;
	                        }

	                        // Consume duplicate/out-of-order packet locally (no forwarding).
	                        const uint32_t queuedBytes = item.packet->GetSize();
                        m_processingQueue.pop();
                        m_currentProcessingQueueSize--;
                        m_currentProcessingQueueBytes -= queuedBytes;
                        if (m_loggingEnabled)
                        {
                            SueStatsUtils::ProcessProcessingQueueStats(m_currentProcessingQueueBytes,
                                                                      m_processingQueueMaxBytes,
                                                                      GetNode()->GetId(),
                                                                      GetIfIndex() - 1);
                        }

                        if(!m_processingQueue.empty()){
                            Simulator::Schedule(m_processingQueueScheduleDelay, &PointToPointSueNetDevice::StartProcessing, this);
                        }
                        return;
                    }
                }

	                item.switchDropAdmissionDone = true;
	            }

		            bool forwarded = m_switch->ProcessSwitchForwarding(item.packet, ethHeader, this, item.protocol, item.vcId,
		                                                              item.switchDropInternalCreditsConsumed);
		            if (forwarded)
		            { 
	                // Packet leaves the ingress processing queue (buffer freed): return link credits to
	                // the upstream sender (XPU or previous switch port).
	                ReturnIngressLinkCreditsIfNeeded();

	                // Process processing queue delay statistics before dequeuing
	                if (m_loggingEnabled)
	                {
	                    SueStatsUtils::ProcessProcessingQueueDelayStats(item.packet, GetNode()->GetId(), GetIfIndex() - 1);
	                }

                const uint32_t queuedBytes = item.packet->GetSize();
                m_processingQueue.pop();
                m_currentProcessingQueueSize--;
                m_currentProcessingQueueBytes -= queuedBytes;

                if (m_loggingEnabled)
                {
                    SueStatsUtils::ProcessProcessingQueueStats(m_currentProcessingQueueBytes, m_processingQueueMaxBytes, GetNode()->GetId(), GetIfIndex() - 1);
                }
		            }
			            else
			            {
		                // If DROP-mode admission already consumed internal CBFC credits, but forwarding
	                // still failed, refund those credits to avoid a permanent leak/deadlock.
	                if (item.switchDropInternalCreditsConsumed && m_cbfcManager)
	                {
	                    const uint32_t credits =
	                        m_cbfcManager->CalculateCreditsForPacket(item.switchDropInternalCreditBytes);
	                    m_cbfcManager->AddTxCredits(item.switchDropInternalCreditPeerMac, item.vcId, credits);
	                    item.switchDropInternalCreditsConsumed = false;
	                }

	                // Head-of-line blocking mitigation:
	                // If forwarding is blocked (typically due to switch-internal CBFC credits),
	                // do not keep retrying the same front packet forever while starving other
                // queued packets that might be forwardable to different egress ports.
                //
                // Move the blocked packet to the back of the processing queue so that the
                // next StartProcessing() can attempt to forward other packets and reduce
                // unnecessary drops at the ingress processing queue limit.
			                if (!m_processingQueue.empty ())
			                {
			                    ProcessItem blocked = m_processingQueue.front ();
			                    m_processingQueue.pop ();
			                    m_processingQueue.push (blocked);
			                }
			            }

			            // Continue draining the ingress processing queue as long as it is non-empty.
			            // Without this, forwarding can stall once upstream injection is backpressured
			            // (no new arrivals to trigger scheduling), leaving credits stuck at 0 and
			            // deadlocking collectives.
			            if (!m_processingQueue.empty())
			            {
			                Simulator::Schedule(m_processingQueueScheduleDelay, &PointToPointSueNetDevice::StartProcessing, this);
			            }
		        }
        else
        {
            // Copy out the front item before popping: we still need its packet pointers after
            // removing it from the queue.
            Ptr<Packet> originalPacket = item.originalPacket;
            Ptr<Packet> packet = item.packet;
            const uint8_t vcId = item.vcId;
            const uint16_t protocol = item.protocol;
            const uint32_t queuedBytes = packet->GetSize();

            // Process processing queue delay statistics before dequeuing
            if (m_loggingEnabled)
            {
                SueStatsUtils::ProcessProcessingQueueDelayStats(packet, GetNode()->GetId(), GetIfIndex() - 1);
            }

            m_processingQueue.pop();
            m_currentProcessingQueueSize--;
            m_currentProcessingQueueBytes -= queuedBytes;

            if (m_loggingEnabled)
            {
                SueStatsUtils::ProcessProcessingQueueStats(m_currentProcessingQueueBytes, m_processingQueueMaxBytes, GetNode()->GetId(), GetIfIndex() - 1);
            }
            // Non-switch device
            m_macRxTrace(originalPacket);
            // Remove Ethernet header for easier reception
            EthernetHeader removeEthHeader;
            packet->RemoveHeader(removeEthHeader);

            m_rxCallback(this, packet, protocol, GetRemote());
            // Return credits using the same on-wire packet size used for TX credit consumption.
            m_cbfcManager->HandleCreditReturn(ethHeader, vcId, originalPacket->GetSize());
            // TODO delay to be set currently: receiver is XPU and directly returns credits upon reception
            if (m_cbfcManager) {
                m_cbfcManager->CreditReturn(ethHeader.GetSource(), vcId);
            }
        }

        if(!m_processingQueue.empty()){
            Simulator::Schedule(m_processingQueueScheduleDelay, &PointToPointSueNetDevice::StartProcessing, this);
        }
    }

    bool
    PointToPointSueNetDevice::SpecDevEnqueueToVcQueue(Ptr<PointToPointSueNetDevice> p2pDev, Ptr<Packet> packet)
    {
        if (!p2pDev)
        {
            return false;
        }
        return p2pDev->EnqueueToVcQueue(packet);
    }

    bool
    PointToPointSueNetDevice::EnqueueToVcQueue(Ptr<Packet> packet)
    {
        if (!m_cbfcManager || !m_cbfcManager->IsInitialized())
        {
            InitializeCbfc();
        }
        // Initialize LLR managers even when LLR is disabled, because LlrSendPacket() is
        // also responsible for ensuring PPP framing + SueTag in non-LLR mode.
        InitializeLlr();
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
                proto == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_UPDATE) ||
                proto == SuePacketUtils::EtherToPpp(SuePacketUtils::PROT_CBFC_SYNC) ||
                proto == SuePacketUtils::EtherToPpp(SuePacketUtils::ACK_REV) ||
                proto == SuePacketUtils::EtherToPpp(SuePacketUtils::NACK_REV))
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

	            // Remove PPP header, prepare for sending to peer (second stage)
	            SuePppHeader ppp_rev;
	            packet->RemoveHeader(ppp_rev);

	            // Extract VC ID from packet for LlrSendPacket
	            vcId = SuePacketUtils::ExtractVcIdFromPacket(packet); // Update vcId from packet

                // Backpressure on endhosts (and always under LLR) instead of silently dropping due to VC queue overflow.
                // NOTE: LlrSendPacket() mutates sequence state; ensure enqueue can succeed before calling it.
                //
                // IMPORTANT: do not use GetVcAvailableCapacity() here because it subtracts
                // SueQueueManager reserved capacity. SueClient may pre-reserve capacity for
                // a batch of scheduled sends, and using "available (after reserve)" would
                // incorrectly reject packets that were already accounted for, causing
                // silent loss / deadlocks in end-to-end completion mode.
                const uint32_t projectedBytes = packet->GetSize() + ppp_rev.GetSerializedSize();
                if (m_queueManager)
                {
                    const uint32_t usedBytes = m_queueManager->GetVcQueueBytes(vcId);
                    const uint32_t maxBytes = m_queueManager->GetVcQueueMaxBytes();
                    if (usedBytes + projectedBytes > maxBytes)
                    {
                        // Switch internal forwarding: do not "drop" a best-effort copy. Let SueSwitch
                        // retry later to preserve lossless behavior under CBFC.
                        if (IsSwitchDevice(m_address))
                        {
                            return false;
                        }

                        // Endhost behavior: treat as a drop (no implicit retry at UDP).
                        if (m_loggingEnabled)
                        {
                            SueStatsUtils::ProcessPacketDropStats(packet, GetNode()->GetId(), GetIfIndex() - 1,
                                                                 "VcQueueFullPrecheck");
                        }
                        NS_LOG_WARN("EnqueueToVcQueue: VC queue full precheck drop on Node"
                                    << GetNode()->GetId() << " Dev" << GetIfIndex()
                                    << " VC " << static_cast<uint32_t>(vcId)
                                    << " used=" << usedBytes << " projected=" << projectedBytes
                                    << " max=" << maxBytes);
                        m_macTxDropTrace(packet);
                        return false;
                    }
                }

	            Mac48Address mac_dst = GetRemoteMac();
	            if(m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager){
	                m_llrSwitchPortManager->LlrSendPacket(packet, vcId, mac_dst);
	            } else if((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager){
	                m_llrNodeManager->LlrSendPacket(packet, vcId);
	            }

	            bool enqOk = m_queueManager && m_queueManager->EnqueueToVcQueue(packet, vcId);
                if (!enqOk)
                {
                    return false;
                }
	            
	            NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
	                                      << "] internal packet enqueued to VC " << static_cast<uint32_t>(vcId)
	                                      << " (queue size now: " << (m_queueManager ? m_queueManager->GetVcQueueSize(vcId) : 0) << " packets)");

            if (m_loggingEnabled)
            {
                // Trigger VC queue statistics (event-driven after VC enqueue)
                SueStatsUtils::ProcessVCQueueStats(m_queueManager, m_cbfcManager,
                                                 m_numVcs, m_vcQueueMaxBytes,
                                                 GetNode()->GetId(), GetIfIndex() - 1);
            }
            if (m_txMachineState == READY)
            {
                m_tryTransmitEvent = Simulator::Schedule(m_vcSchedulingDelay, &PointToPointSueNetDevice::TryTransmit, this);
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

            // Backpressure on endhosts (and always under LLR) instead of silently dropping due to VC queue overflow.
            // NOTE: LlrSendPacket() mutates sequence state; ensure enqueue can succeed before calling it.
            //
            // IMPORTANT: do not use GetVcAvailableCapacity() here because it subtracts
            // SueQueueManager reserved capacity (see comment above).
            SuePppHeader ppp_proj;
            const uint32_t projectedBytes = packet->GetSize() + ppp_proj.GetSerializedSize();
            if (m_queueManager)
            {
                const uint32_t usedBytes = m_queueManager->GetVcQueueBytes(vcId);
                const uint32_t maxBytes = m_queueManager->GetVcQueueMaxBytes();
                if (usedBytes + projectedBytes > maxBytes)
                {
                    // Switch internal forwarding: let SueSwitch retry (backpressure), do not
                    // record an on-wire drop for a packet that will be resent.
                    if (IsSwitchDevice(m_address))
                    {
                        return false;
                    }

                    if (m_loggingEnabled)
                    {
                        SueStatsUtils::ProcessPacketDropStats(packet, GetNode()->GetId(), GetIfIndex() - 1,
                                                             "VcQueueFullPrecheck");
                    }
                    NS_LOG_WARN("EnqueueToVcQueue: VC queue full precheck drop on Node"
                                << GetNode()->GetId() << " Dev" << GetIfIndex()
                                << " VC " << static_cast<uint32_t>(vcId)
                                << " used=" << usedBytes << " projected=" << projectedBytes
                                << " max=" << maxBytes);
                    m_macTxDropTrace(packet);
                    return false;
                }
            }

	        if(m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager){
	            m_llrSwitchPortManager->LlrSendPacket(packet, vcId, mac_dst);
	        } else if((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager){
	            m_llrNodeManager->LlrSendPacket(packet, vcId);
	        }
	        m_macTxTrace(packet);

	        // Add VC queue delay tag before enqueueing
	        SueTag::AddVcQueueDelayTag(packet, GetNode()->GetId(), GetIfIndex() - 1, vcId);

	        bool enqOk = m_queueManager && m_queueManager->EnqueueToVcQueue(packet, vcId);
            if (!enqOk)
            {
                return false;
            }

	        NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
	                                  << "] packet enqueued to VC " << static_cast<uint32_t>(vcId)
	                                  << " (queue size now: " << (m_queueManager ? m_queueManager->GetVcQueueSize(vcId) : 0) << " packets)");

        if (m_loggingEnabled)
        {
            // Trigger VC queue statistics (event-driven after VC enqueue)
            SueStatsUtils::ProcessVCQueueStats(m_queueManager, m_cbfcManager,
                                             m_numVcs, m_vcQueueMaxBytes,
                                             GetNode()->GetId(), GetIfIndex() - 1);
        }

        if (m_txMachineState == READY)
        {
            m_tryTransmitEvent = Simulator::Schedule(m_vcSchedulingDelay, &PointToPointSueNetDevice::TryTransmit, this);
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

        // CBFC control packets (update + sync) enter high-priority main queue
        if (protocolNumber == SuePacketUtils::PROT_CBFC_UPDATE ||
            protocolNumber == SuePacketUtils::PROT_CBFC_SYNC)
        {
            // Credit packet structure - only CBFC header, PPP header added below
            // PPP Header
            AddHeader(packet, protocolNumber);
            if (!m_queue->Enqueue(packet))
            {
                // Log main queue packet drop (event-driven)
                if (m_loggingEnabled)
                {
                    SueStatsUtils::ProcessPacketDropStats(packet, GetNode()->GetId(), GetIfIndex() - 1, "MainQueueFull");
                }
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

            // Apply LLR sequencing to CBFC update packets so they can be retransmitted under loss.
            // Use a dedicated LLR VC for control to avoid data/control reordering issues.
            if (m_llrEnabled && m_llrProtectCbfcUpdates)
            {
                Ptr<Packet> copy = packet->Copy();
                SuePppHeader tmpPpp;
                copy->RemoveHeader(tmpPpp);
                SueCbfcHeader cbfcHeader;
                copy->PeekHeader(cbfcHeader);

                const uint8_t llrVcId = GetLlrControlVcId();
                Mac48Address peerMac = GetRemoteMac();
                if (m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager)
                {
                    m_llrSwitchPortManager->LlrSendPacket(packet, llrVcId, peerMac);
                }
                else if ((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager)
                {
                    m_llrNodeManager->LlrSendPacket(packet, llrVcId);
                }
            }
            if (!IsSwitchDevice(m_address))
            {
                NS_LOG_INFO("Link: [Node" << GetNode()->GetId() + 1 << " Device " << GetIfIndex()
                                          << "] credit packet enqueued to main queue"
                                          << " (size now: " << m_queue->GetNPackets() << " packets)");
            }

            if (m_loggingEnabled)
            {
                // Trigger main queue statistics (event-driven after main queue enqueue)
                SueStatsUtils::ProcessMainQueueStats(m_queue, GetNode()->GetId(), GetIfIndex() - 1);
            }

            if (m_txMachineState == READY)
            {
                m_tryTransmitEvent = Simulator::Schedule(m_vcSchedulingDelay, &PointToPointSueNetDevice::TryTransmit, this);
            }
        }
	        else if(protocolNumber == SuePacketUtils::ACK_REV || protocolNumber == SuePacketUtils::NACK_REV){// ACK/NACK packets enter high-priority main queue
	            if (!m_queue->Enqueue(packet))
	            {
	                // Log main queue packet drop (event-driven)
	                if (m_loggingEnabled)
	                {
	                    SueStatsUtils::ProcessPacketDropStats(packet, GetNode()->GetId(), GetIfIndex() - 1, "MainQueueFull");
	                }
	                m_macTxDropTrace(packet);
	                return false;
	            }

	            if (m_loggingEnabled)
	            {
	                // Trigger main queue statistics (event-driven after main queue enqueue)
	                SueStatsUtils::ProcessMainQueueStats(m_queue, GetNode()->GetId(), GetIfIndex() - 1);
            }

            if (m_txMachineState == READY)
            {
                m_tryTransmitEvent = Simulator::Schedule(m_vcSchedulingDelay, &PointToPointSueNetDevice::TryTransmit, this);
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
	            return EnqueueToVcQueue(packet);
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

        bool isSwitch = false;
        if (LookupRegisteredDeviceRole(mac, &isSwitch))
        {
            return isSwitch;
        }

        uint8_t buffer[6];
        mac.CopyTo(buffer);
        uint8_t lastByte = buffer[5]; // Last byte of MAC address
        // Fallback heuristic for legacy setups: switch devices get even MAC addresses.
        return (lastByte % 2 == 0); // Even numbers are switch devices
    }

    void
    PointToPointSueNetDevice::RegisterDeviceRole(Mac48Address mac, bool isSwitchDevice)
    {
        RegisterDeviceMeta(mac, isSwitchDevice, 0 /*xpuId*/, 0 /*portId*/);
    }

    void
    PointToPointSueNetDevice::RegisterDeviceMeta(Mac48Address mac,
                                                 bool isSwitchDevice,
                                                 uint32_t xpuId,
                                                 uint32_t portId)
    {
        RegisteredDeviceMeta meta;
        meta.isSwitchDevice = isSwitchDevice;
        meta.hasXpuId = true;
        meta.hasPortId = true;
        meta.xpuId = xpuId;
        meta.portId = portId;
        g_registeredDeviceMeta[mac] = meta;
    }

    void
    PointToPointSueNetDevice::ClearRegisteredDeviceRoles()
    {
        g_registeredDeviceMeta.clear();
    }

    bool
    PointToPointSueNetDevice::LookupRegisteredDeviceRole(Mac48Address mac, bool* isSwitchDeviceOut)
    {
        auto it = g_registeredDeviceMeta.find(mac);
        if (it == g_registeredDeviceMeta.end())
        {
            return false;
        }
        if (isSwitchDeviceOut != nullptr)
        {
            *isSwitchDeviceOut = it->second.isSwitchDevice;
        }
        return true;
    }

    bool
    PointToPointSueNetDevice::LookupRegisteredDeviceMeta(Mac48Address mac,
                                                         bool* isSwitchDeviceOut,
                                                         uint32_t* xpuIdOut,
                                                         uint32_t* portIdOut)
    {
        auto it = g_registeredDeviceMeta.find(mac);
        if (it == g_registeredDeviceMeta.end())
        {
            return false;
        }
        if (isSwitchDeviceOut != nullptr)
        {
            *isSwitchDeviceOut = it->second.isSwitchDevice;
        }
        if (xpuIdOut != nullptr && it->second.hasXpuId)
        {
            *xpuIdOut = it->second.xpuId;
        }
        if (portIdOut != nullptr && it->second.hasPortId)
        {
            *portIdOut = it->second.portId;
        }
        return true;
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
    PointToPointSueNetDevice::IsLlrResending(Mac48Address mac, uint8_t vcId) const
    {
        if (!m_llrEnabled)
        {
            return false;
        }

        if (m_switch && IsSwitchDevice(m_address) && m_llrSwitchPortManager)
        {
            return m_llrSwitchPortManager->IsLlrResending(vcId, mac);
        }
        if ((!m_switch || !IsSwitchDevice(m_address)) && m_llrNodeManager)
        {
            return m_llrNodeManager->IsLlrResending(vcId);
        }
        return false;
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

    DataRate
    PointToPointSueNetDevice::GetDataRate() const
    {
        return m_bps;
    }
} // namespace ns3
