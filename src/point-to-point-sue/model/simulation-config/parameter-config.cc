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

#include "parameter-config.h"
#include "ns3/core-module.h"
#include <iostream>
#include <iomanip>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ParameterConfig");

SueSimulationConfig::SueSimulationConfig ()
{
    // Initialize timing configuration
    timing.simulationTime = 3.00;
    timing.serverStart = 1.0;
    timing.clientStart = 2.0;
    timing.clientStopOffset = 0.1;
    timing.serverStopOffset = 0.01;
    timing.threadStartInterval = 0.1;

    // Initialize network configuration
    network.nXpus = 4;
    network.portsPerXpu = 8;
    network.portsPerSue = 2;
    network.suesPerXpu = 0; // Will be calculated

    // Initialize traffic configuration
    traffic.transactionSize = 256;
    traffic.maxBurstSize = 2048;
    traffic.Mtu = 2500;
    traffic.vcNum = 4;
    traffic.threadRate = 3500000;
    traffic.totalBytesToSend = 50;

    // Initialize link configuration
    link.errorRate = 0.00;
    link.processingDelay = "10ns";
    link.numVcs = 4;
    link.LinkDataRate = "200Gbps";
    link.ProcessingRate = "200Gbps";
    link.LinkDelay = "10ns";

    // Initialize queue configuration
    queue.vcQueueMaxMB = 0.3;
    queue.vcQueueMaxBytes = 0; // Will be calculated
    queue.processingQueueMaxMB = 0.3;
    queue.processingQueueMaxBytes = 0; // Will be calculated
    queue.destQueueMaxMB = 0.03;
    queue.destQueueMaxBytes = 0; // Will be calculated

    // Initialize CBFC configuration
    cbfc.EnableLinkCBFC = true;
    cbfc.LinkCredits = 85;
    cbfc.CreditBatchSize = 1;

    // Initialize load balance configuration
    loadBalance.loadBalanceAlgorithm = 3;
    loadBalance.hashSeed = 12345;
    loadBalance.prime1 = 7919;
    loadBalance.prime2 = 9973;
    loadBalance.useVcInHash = true;
    loadBalance.enableBitOperations = true;

    // Initialize trace configuration
    trace.statLoggingEnabled = true;
    trace.ClientStatInterval = "10us";
    trace.LinkStatInterval = "10us";

    // Initialize delay configuration
    delay.SchedulingInterval = "5ns";
    delay.PackingDelayPerPacket = "3ns";
    delay.destQueueSchedulingDelay = "5ns";
    delay.transactionClassificationDelay = "0ns";
    delay.packetCombinationDelay = "12ns";
    delay.ackProcessingDelay = "15ns";
    delay.vcSchedulingDelay = "8ns";
    delay.DataAddHeadDelay = "5ns";
    delay.additionalHeaderSize = 46;
    delay.creditGenerateDelay = "10ns";
    delay.CreUpdateAddHeadDelay = "3ns";
    delay.creditReturnProcessingDelay = "8ns";
    delay.batchCreditAggregationDelay = "5ns";
    delay.switchForwardDelay = "130ns";
}

void
SueSimulationConfig::ParseCommandLine (int argc, char* argv[])
{
    CommandLine cmd;

    // Timing parameters
    cmd.AddValue ("simulationTime", "Total simulation duration in seconds", timing.simulationTime);
    cmd.AddValue ("serverStart", "Server start time (seconds)", timing.serverStart);
    cmd.AddValue ("clientStart", "Client start time (seconds)", timing.clientStart);
    cmd.AddValue ("clientStopOffset", "Client stop time offset from simulation end (seconds)", timing.clientStopOffset);
    cmd.AddValue ("serverStopOffset", "Server stop time offset from simulation end (seconds)", timing.serverStopOffset);
    cmd.AddValue ("threadStartInterval", "Interval between thread start times (seconds)", timing.threadStartInterval);

    // Network topology parameters
    cmd.AddValue ("nXpus", "The number of XPU nodes", network.nXpus);
    cmd.AddValue ("portsPerXpu", "Number of ports per XPU", network.portsPerXpu);
    cmd.AddValue ("portsPerSue", "Number of ports per SUE (1/2/4)", network.portsPerSue);
    cmd.AddValue("threadRate", "Traffic generation rate per thread (Mbps)", traffic.threadRate);
    cmd.AddValue("totalBytesToSend", "Total bytes to send per XPU (MB)", traffic.totalBytesToSend);

    // Traffic generation parameters
    cmd.AddValue("transactionSize", "Size per transaction in bytes", traffic.transactionSize);
    cmd.AddValue("maxBurstSize", "Maximum burst size in bytes", traffic.maxBurstSize);
    cmd.AddValue("Mtu", "Maximum Transmission Unit in bytes", traffic.Mtu);
    cmd.AddValue("vcNum", "Number of virtual channels at application layer", traffic.vcNum);

    // Link layer parameters
    cmd.AddValue("errorRate", "The packet error rate for the links", link.errorRate);
    cmd.AddValue("processingDelay", "Processing delay per packet", link.processingDelay);
    cmd.AddValue("numVcs", "Number of virtual channels at link layer", link.numVcs);
    cmd.AddValue("LinkDataRate", "Link data rate", link.LinkDataRate);
    cmd.AddValue("ProcessingRate", "Link data rate", link.ProcessingRate);
    cmd.AddValue("LinkDelay", "Link propagation delay", link.LinkDelay);

    // Queue buffer size configuration
    cmd.AddValue("VcQueueMaxMB", "Maximum VC queue size in MB (default: 0.3MB)", queue.vcQueueMaxMB);
    cmd.AddValue("ProcessingQueueMaxMB", "Maximum processing queue size in MB (default: 0.3MB)", queue.processingQueueMaxMB);
    cmd.AddValue("DestQueueMaxMB", "Maximum destination queue size in MB (default: 0.03MB)", queue.destQueueMaxMB);

    // CBFC flow control parameters
    cmd.AddValue("EnableLinkCBFC", "Enable Credit-Based Flow Control", cbfc.EnableLinkCBFC);
    cmd.AddValue("LinkCredits", "Initial credits at link layer", cbfc.LinkCredits);
    cmd.AddValue("CreditBatchSize", "Credit accumulation threshold", cbfc.CreditBatchSize);

    // Trace sampling parameters
    cmd.AddValue("StatLoggingEnabled", "Link Layer Stat Logging Enabled Switch", trace.statLoggingEnabled);
    cmd.AddValue("ClientStatInterval", "Client Statistic Interval", trace.ClientStatInterval);
    cmd.AddValue("LinkStatInterval", "Link Statistic Interval", trace.LinkStatInterval);

    // Delay parameters - transmitter scheduling
    cmd.AddValue("SchedulingInterval", "Transmitter scheduler polling interval", delay.SchedulingInterval);
    cmd.AddValue("PackingDelayPerPacket", "Packet packing processing time", delay.PackingDelayPerPacket);
    cmd.AddValue("destQueueSchedulingDelay", "Destination queue scheduling delay", delay.destQueueSchedulingDelay);
    cmd.AddValue("transactionClassificationDelay", "Transaction classification delay", delay.transactionClassificationDelay);
    cmd.AddValue("packetCombinationDelay", "Packet combination delay", delay.packetCombinationDelay);
    cmd.AddValue("ackProcessingDelay", "ACK processing delay", delay.ackProcessingDelay);

    // Link layer delay parameters
    cmd.AddValue("vcSchedulingDelay", "VC queue scheduling delay", delay.vcSchedulingDelay);
    cmd.AddValue("DataAddHeadDelay", "Data packet header addition delay", delay.DataAddHeadDelay);

    // Credit-related delays
    cmd.AddValue("creditGenerateDelay", "Credit packet generation delay", delay.creditGenerateDelay);
    cmd.AddValue("CreUpdateAddHeadDelay", "Credit update packet header addition delay", delay.CreUpdateAddHeadDelay);
    cmd.AddValue("creditReturnProcessingDelay", "Credit return processing delay", delay.creditReturnProcessingDelay);
    cmd.AddValue("batchCreditAggregationDelay", "Batch credit aggregation delay", delay.batchCreditAggregationDelay);
    cmd.AddValue("switchForwardDelay", "Switch internal forwarding delay", delay.switchForwardDelay);

    // Capacity reservation parameters
    cmd.AddValue("AdditionalHeaderSize", "Additional header size for capacity reservation (default: 46 bytes)", delay.additionalHeaderSize);

    // Load balancing parameters
    cmd.AddValue("loadBalanceAlgorithm", "Load balancing algorithm (0=SIMPLE_MOD, 1=MOD_WITH_SEED, 2=PRIME_HASH, 3=ENHANCED_HASH, 4=ROUND_ROBIN, 5=CONSISTENT_HASH)", loadBalance.loadBalanceAlgorithm);
    cmd.AddValue("hashSeed", "Hash seed for load balancing", loadBalance.hashSeed);
    cmd.AddValue("prime1", "First prime number for hash algorithms", loadBalance.prime1);
    cmd.AddValue("prime2", "Second prime number for enhanced hash", loadBalance.prime2);
    cmd.AddValue("useVcInHash", "Include VC ID in hash calculation", loadBalance.useVcInHash);
    cmd.AddValue("enableBitOperations", "Enable bit mixing operations in hash", loadBalance.enableBitOperations);

    cmd.Parse(argc, argv);
}

void
SueSimulationConfig::ValidateAndCalculate ()
{
    // Convert MB to bytes for queue configurations
    queue.vcQueueMaxBytes = static_cast<uint32_t>(queue.vcQueueMaxMB * 1024 * 1024);
    queue.processingQueueMaxBytes = static_cast<uint32_t>(queue.processingQueueMaxMB * 1024 * 1024);
    queue.destQueueMaxBytes = static_cast<uint32_t>(queue.destQueueMaxMB * 1024 * 1024);

    // Parameter validation
    if (network.portsPerSue != 1 && network.portsPerSue != 2 && network.portsPerSue != 4) {
        NS_ABORT_MSG("portsPerSue must be 1, 2, or 4. Current value: " << network.portsPerSue);
    }
    if (network.portsPerXpu % network.portsPerSue != 0) {
        NS_ABORT_MSG("portsPerXpu (" << network.portsPerXpu << ") must be divisible by portsPerSue (" << network.portsPerSue << ")");
    }
    if (loadBalance.loadBalanceAlgorithm > 5) {
        NS_ABORT_MSG("loadBalanceAlgorithm must be 0-5 (0=SIMPLE_MOD, 1=MOD_WITH_SEED, 2=PRIME_HASH, 3=ENHANCED_HASH, 4=ROUND_ROBIN, 5=CONSISTENT_HASH). Current value: " << loadBalance.loadBalanceAlgorithm);
    }

    // Recalculate number of SUEs per XPU
    network.suesPerXpu = network.portsPerXpu / network.portsPerSue;
}

void
SueSimulationConfig::PrintConfiguration () const
{
    // Display link layer configuration information
    std::cout << "Link Layer Configuration:" << std::endl;
    std::cout << "  Number of VCs: " << static_cast<int>(link.numVcs) << std::endl;
    std::cout << "  VC Queue Max Size: " << queue.vcQueueMaxMB << " MB ("
              << queue.vcQueueMaxBytes << " bytes)" << std::endl;
    std::cout << "  Processing Queue Max Size: " << queue.processingQueueMaxMB << " MB ("
              << queue.processingQueueMaxBytes << " bytes)" << std::endl;
    std::cout << "  Link Data Rate: " << link.LinkDataRate << std::endl;
    std::cout << "  Processing Rate: " << link.ProcessingRate << std::endl;
    std::cout << "  Link Delay: " << link.LinkDelay << std::endl;
    std::cout << "  Enable Link CBFC: " << (cbfc.EnableLinkCBFC ? "true" : "false") << std::endl;
    std::cout << std::endl;

    // Display LoadBalancer configuration information
    std::cout << "LoadBalancer Configuration:" << std::endl;
    std::cout << "  Algorithm: " << loadBalance.loadBalanceAlgorithm;
    switch (loadBalance.loadBalanceAlgorithm) {
        case 0: std::cout << " (SIMPLE_MOD)"; break;
        case 1: std::cout << " (MOD_WITH_SEED)"; break;
        case 2: std::cout << " (PRIME_HASH)"; break;
        case 3: std::cout << " (ENHANCED_HASH)"; break;
        case 4: std::cout << " (ROUND_ROBIN)"; break;
        case 5: std::cout << " (CONSISTENT_HASH)"; break;
    }
    std::cout << std::endl;
    std::cout << "  Hash Seed: " << loadBalance.hashSeed << std::endl;
    std::cout << "  Prime1: " << loadBalance.prime1 << ", Prime2: " << loadBalance.prime2 << std::endl;
    std::cout << "  Use VC in Hash: " << (loadBalance.useVcInHash ? "true" : "false") << std::endl;
    std::cout << "  Enable Bit Operations: " << (loadBalance.enableBitOperations ? "true" : "false") << std::endl;

    NS_LOG_INFO("Creating XPU-Switch topology with " << network.nXpus << " XPUs ("
               << network.portsPerXpu << " ports/XPU, " << network.portsPerSue << " ports/SUE, "
               << network.suesPerXpu << " SUEs/XPU)");
    NS_LOG_INFO("Total simulation time: " << timing.simulationTime << " seconds");
    NS_LOG_INFO("Servers active: " << timing.serverStart << "s to " << GetServerStop () << "s");
    NS_LOG_INFO("Clients active: " << timing.clientStart << "s to " << GetClientStop () << "s");
    NS_LOG_INFO("Thread start interval: " << timing.threadStartInterval << "s");
}

double
SueSimulationConfig::GetClientStop () const
{
    return timing.simulationTime - timing.clientStopOffset;
}

double
SueSimulationConfig::GetServerStop () const
{
    return timing.simulationTime - timing.serverStopOffset;
}

} // namespace ns3