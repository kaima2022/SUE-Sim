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
#include <string>

namespace ns3 {

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
};

/**
 * \brief Link layer configuration parameters
 */
struct LinkConfig
{
    double errorRate;              //!< Error rate
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
    double vcQueueMaxMB;              //!< VC queue maximum size (MB)
    uint32_t vcQueueMaxBytes;         //!< VC queue max bytes (calculated)
    double processingQueueMaxMB;      //!< Processing queue maximum size (MB)
    uint32_t processingQueueMaxBytes; //!< Processing queue max bytes (calculated)
    double destQueueMaxMB;           //!< Destination queue maximum size (MB)
    uint32_t destQueueMaxBytes;      //!< Destination queue max bytes (calculated)
};

/**
 * \brief CBFC flow control configuration parameters
 */
struct CbfcConfig
{
    bool EnableLinkCBFC;      //!< Link CBFC enable
    uint32_t LinkCredits;     //!< Link layer initial CBFC credits
    uint32_t CreditBatchSize; //!< Credit accumulation threshold
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
};

/**
 * \brief Trace sampling configuration parameters
 */
struct TraceConfig
{
    bool statLoggingEnabled;         //!< Link layer statistics collection enable
    std::string ClientStatInterval; //!< Client statistics interval
    std::string LinkStatInterval;   //!< Link statistics interval
};

/**
 * \brief Delay configuration parameters
 */
struct DelayConfig
{
    // Transmitter delays
    std::string SchedulingInterval;              //!< Transmitter scheduler polling interval
    std::string PackingDelayPerPacket;           //!< Packet packing processing time
    std::string destQueueSchedulingDelay;        //!< Destination queue scheduling delay
    std::string transactionClassificationDelay;  //!< Transaction classification delay
    std::string packetCombinationDelay;         //!< Packet combination delay
    std::string ackProcessingDelay;             //!< ACK processing delay

    // Link layer delays
    std::string vcSchedulingDelay;               //!< VC queue scheduling delay
    std::string DataAddHeadDelay;                //!< Data packet header addition delay

    // Credit-related delays
    std::string creditGenerateDelay;            //!< Credit packet generation delay
    std::string CreUpdateAddHeadDelay;           //!< Credit packet header addition delay
    std::string creditReturnProcessingDelay;     //!< Credit return processing delay
    std::string batchCreditAggregationDelay;     //!< Batch credit aggregation delay
    std::string switchForwardDelay;            //!< Switch internal forwarding delay

    // Capacity reservation parameters
    uint32_t additionalHeaderSize;              //!< Additional header size for capacity reservation
};

/**
 * \brief LLR configuration parameters
 */
struct LlrConfig
{
    bool m_llrEnabled;                    //!< Enable Link Layer Reliability
    std::string LlrTimeout;               //!< LLR timeout value
    uint32_t LlrWindowSize;               //!< LLR window size
    std::string AckAddHeaderDelay;        //!< ACK/NACK header adding delay
    std::string AckProcessDelay;          //!< ACK/NACK processing delay
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