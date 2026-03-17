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

#ifndef PARAMETER_CONFIG_H
#define PARAMETER_CONFIG_H

#include "ns3/core-module.h"
#include <limits>
#include <string>

namespace ns3 {

// Sentinel values for auto-selection in fine-grained traffic configs.
constexpr uint32_t kAutoSueId = std::numeric_limits<uint32_t>::max();
constexpr uint32_t kAutoSuePort = std::numeric_limits<uint32_t>::max();
constexpr uint8_t kAutoVcId = std::numeric_limits<uint8_t>::max();

/**
 * \brief Timing configuration parameters
 */
struct TimingConfig
{
    double simulationTime;        //!< Total simulation time (seconds)
    double serverStart;           //!< Server start time (seconds)
    double clientStart;           //!< Client start time (seconds)
    double clientStopOffset;      //!< Client stop time offset
    double serverStopOffset;      //!< Server stop time offset
    double threadStartInterval;   //!< Thread start interval
};

/**
 * \brief Network topology configuration parameters
 */
struct NetworkConfig
{
    uint32_t nXpus;           //!< Number of XPU nodes
    uint32_t portsPerXpu;     //!< Number of ports per XPU
    uint32_t portsPerSue;     //!< Number of ports managed by each SUE (1/2/4)
    uint32_t suesPerXpu;      //!< Number of SUEs per XPU (calculated)
};

/**
 * \brief Fine-grained traffic flow configuration entry
 */
struct FineGrainedTrafficFlow
{
    uint32_t sourceXpuId;         //!< Source XPU ID
    uint32_t destXpuId;           //!< Destination XPU ID
    uint32_t sueId;               //!< SUE ID to use for sending
    uint32_t suePort;             //!< SUE port to use for sending
    double dataRate;              //!< Data rate for this flow (Mbps)
    uint32_t totalBytes;          //!< Total bytes to send for this flow
    uint8_t vcId;                 //!< Virtual channel ID (0-3, optional)
    double startTime;             //!< Start time relative to clientStart (seconds, parsed from ns in CSV)

    /**
     * \brief Constructor
     */
    FineGrainedTrafficFlow () : sourceXpuId(0), destXpuId(0), sueId(0), suePort(0),
                               dataRate(0.0), totalBytes(0), vcId(0), startTime(0.0) {}

    /**
     * \brief Constructor with parameters
     */
    FineGrainedTrafficFlow (uint32_t src, uint32_t dst, uint32_t sue, uint32_t port,
                           double rate, uint32_t bytes, uint8_t vc = 0, double start = 0.0)
        : sourceXpuId(src), destXpuId(dst), sueId(sue), suePort(port),
          dataRate(rate), totalBytes(bytes), vcId(vc), startTime(start) {}
};

/**
 * \brief Traffic generation configuration parameters
 */
struct TrafficConfig
{
    uint32_t transactionSize;     //!< Transaction size (bytes)
    uint32_t maxBurstSize;        //!< Maximum burst size (bytes)
    uint32_t Mtu;                 //!< Maximum transmission unit
    uint8_t vcNum;                //!< Number of virtual channels
    double threadRate;            //!< Thread rate (Mbps)
    uint32_t totalBytesToSend;    //!< Total bytes to send (MB)
    bool enableTraceMode;         //!< Enable trace-based traffic generation
    std::string traceFilePath;    //!< Path to trace file for trace-based generation

    // New fine-grained traffic control parameters
    bool enableFineGrainedMode;   //!< Enable fine-grained traffic control mode
    std::string fineGrainedConfigFile; //!< Path to fine-grained traffic configuration file
    std::vector<FineGrainedTrafficFlow> fineGrainedFlows; //!< Parsed fine-grained traffic flows
};

/**
 * \brief Link layer configuration parameters
 */
struct LinkConfig
{
    double errorRate;              //!< Error rate
    std::string errorRateStopAfter; //!< Disable receiver error model after this duration since clientStart (e.g., 500us). Empty means never stop.
    bool errorModelApplyToControlPackets; //!< Apply error model to CBFC/ACK/NACK control packets
    bool errorModelApplyToSyncPackets;    //!< Apply error model to CBFC_SYNC packets
    std::string processingDelay;   //!< Processing delay per packet
    uint8_t numVcs;                //!< Number of link-layer VCs
    std::string LinkDataRate;      //!< Link data rate
    std::string ProcessingRate;    //!< Processing rate
    std::string LinkDelay;         //!< Link propagation delay
};

/**
 * \brief Queue buffer configuration parameters
 */
struct QueueConfig
{
    double vcQueueMaxKB;              //!< VC queue maximum size (KB)
    uint32_t vcQueueMaxBytes;         //!< VC queue max bytes (calculated)
    double processingQueueMaxKB;      //!< Processing queue maximum size (KB)
    uint32_t processingQueueMaxBytes; //!< Processing queue max bytes (calculated)
    double destQueueMaxKB;           //!< Destination queue maximum size (KB)
    uint32_t destQueueMaxBytes;      //!< Destination queue max bytes (calculated)
    uint32_t switchEgressOverflowPolicy; //!< Switch egress overflow policy (0=retry, 1=drop)
};

/**
 * \brief CBFC flow control configuration parameters
 */
struct CbfcConfig
{
    bool EnableLinkCBFC;      //!< Link CBFC enable
    uint32_t LinkCredits;     //!< Link layer initial CBFC credits
    uint32_t CreditBatchSize; //!< Credit accumulation threshold
    uint32_t SwitchCredits;   //!< Switch credits
    uint32_t HeaderSize;      //!< Header size (Ethernet + SUE headers)
    uint32_t BaseCredit;      //!< Base credit value for minimum packet

    // Credit-to-byte mapping parameters
    uint32_t BytesPerCredit;  //!< Bytes per credit (default: 32 bytes/credit)

    // Periodic credit sync (to correct credit drift/leaks under loss/drop)
    bool EnableCreditSync;       //!< Enable periodic credit sync
    std::string CreditSyncInterval; //!< Credit sync interval (e.g., 10us)

    // Link credit allocation mode
    uint32_t LinkCreditMode;     //!< 0=SHARED (all VCs share one pool), 1=EXCLUSIVE (per-VC equal split)
};

/**
 * \brief Load balancing configuration parameters
 */
struct LoadBalanceConfig
{
    uint32_t loadBalanceAlgorithm;  //!< Load balancing algorithm
    uint32_t hashSeed;              //!< Hash seed
    uint32_t prime1;                //!< First prime number for hash algorithms
    uint32_t prime2;                //!< Second prime number for enhanced hash
    bool useVcInHash;               //!< Include VC ID in hash calculation
    bool enableBitOperations;       //!< Enable bit mixing operations
    bool enableAlternativePath;     //!< Enable alternative SUE path search when target is full
};

/**
 * \brief Trace sampling configuration parameters
 */
struct TraceConfig
{
    bool statLoggingEnabled;         //!< Link layer statistics collection enable
    std::string ClientStatInterval; //!< Client statistics interval
};

/**
 * \brief Delay configuration parameters
 */
struct DelayConfig
{
    // Transmitter delays
    std::string SchedulingInterval;              //!< Transmitter scheduler polling interval
    std::string PackingDelayPerPacket;           //!< Packet packing processing time
    std::string transactionClassificationDelay;  //!< Transaction classification delay
    std::string packetCombinationDelay;         //!< Packet combination delay
    std::string ackProcessingDelay;             //!< ACK processing delay

    // Link layer delays
    std::string vcSchedulingDelay;               //!< VC queue scheduling delay
    
    // Credit-related delays
    std::string creditGenerateDelay;            //!< Credit packet generation delay
    std::string CreUpdateAddHeadDelay;           //!< Credit packet header addition delay
    std::string creditReturnProcessingDelay;     //!< Credit return processing delay
    std::string batchCreditAggregationDelay;     //!< Batch credit aggregation delay
    std::string switchForwardDelay;            //!< Switch internal forwarding delay

    // Processing queue delays
    std::string processingQueueScheduleDelay;   //!< Processing queue scheduling delay

    // Capacity reservation parameters
    uint32_t additionalHeaderSize;              //!< Additional header size for capacity reservation
};

/**
 * \brief LLR configuration parameters
 */
struct LlrConfig
{
    bool m_llrEnabled;                    //!< Enable Link Layer Reliability
    bool LlrProtectCbfcUpdates;           //!< Also apply LLR sequencing to CBFC update packets
    std::string LlrTimeout;               //!< LLR timeout value
    uint32_t LlrWindowSize;               //!< LLR window size
    std::string AckAddHeaderDelay;        //!< ACK/NACK header adding delay
    std::string AckProcessDelay;          //!< ACK/NACK processing delay
};

/**
 * \brief Logging configuration parameters
 */
struct LoggingConfig
{
    std::string logLevel;                 //!< Log level for all components (LOG_LEVEL_DEBUG, LOG_LEVEL_INFO, etc.)
    bool enableAllComponents;             //!< Enable logging for all SUE simulation components

    /**
     * \brief Constructor with default values
     */
    LoggingConfig () : logLevel ("LOG_LEVEL_INFO"), enableAllComponents (true) {}
};

/**
 * \brief Main configuration structure containing all sub-configurations
 */
struct SueSimulationConfig
{
    TimingConfig timing;        //!< Timing-related parameters
    NetworkConfig network;      //!< Network topology parameters
    TrafficConfig traffic;      //!< Traffic generation parameters
    LinkConfig link;            //!< Link layer parameters
    QueueConfig queue;          //!< Queue buffer parameters
    CbfcConfig cbfc;            //!< CBFC flow control parameters
    LoadBalanceConfig loadBalance; //!< Load balancing parameters
    TraceConfig trace;          //!< Trace sampling parameters
    DelayConfig delay;          //!< Delay-related parameters
    LlrConfig llr;              //!< Llr related parameters
    LoggingConfig logging;      //!< Logging configuration parameters

    /**
     * \brief Constructor with default values
     */
    SueSimulationConfig ();

    /**
     * \brief Parse command line arguments
     * \param argc Argument count
     * \param argv Argument vector
     */
    void ParseCommandLine (int argc, char* argv[]);

    /**
     * \brief Validate and calculate derived parameters
     */
    void ValidateAndCalculate ();

    /**
     * \brief Print configuration information
     */
    void PrintConfiguration () const;

    /**
     * \brief Calculate precise time points
     * \return Client stop time
     */
    double GetClientStop () const;

    /**
     * \brief Calculate precise time points
     * \return Server stop time
     */
    double GetServerStop () const;
};

} // namespace ns3

#endif /* PARAMETER_CONFIG_H */
