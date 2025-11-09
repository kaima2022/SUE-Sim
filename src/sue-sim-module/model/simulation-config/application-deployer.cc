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

#include "application-deployer.h"
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ApplicationDeployer");

ApplicationDeployer::ApplicationDeployer ()
{
}

ApplicationDeployer::~ApplicationDeployer ()
{
}

void
ApplicationDeployer::DeployApplications (const SueSimulationConfig& config, TopologyBuilder& topologyBuilder)
{
    NS_LOG_INFO ("Deploying applications on topology");

    InstallServers (config, topologyBuilder);
    InstallClientsAndTrafficGenerators (config, topologyBuilder);

    NS_LOG_INFO ("Application deployment completed");
}

void
ApplicationDeployer::InstallServers (const SueSimulationConfig& config, TopologyBuilder& topologyBuilder)
{
    uint32_t nXpus = config.network.nXpus;
    uint32_t portsPerXpu = config.network.portsPerXpu;
    uint32_t transactionSize = config.traffic.transactionSize;
    double serverStart = config.timing.serverStart;
    double serverStop = config.GetServerStop ();

    NodeContainer* xpuNodes = topologyBuilder.GetXpuNodes ();

    // Install server applications (Each port of each XPU)
    for (uint32_t xpuIdx = 0; xpuIdx < nXpus; ++xpuIdx)
    {
        for (uint32_t portIdx = 0; portIdx < portsPerXpu; ++portIdx)
        {
            Ptr<SueServer> serverApp = CreateObject<SueServer>();
            serverApp->SetAttribute("Port", UintegerValue(8080 + portIdx));
            serverApp->SetAttribute("TransactionSize", UintegerValue(transactionSize));
            serverApp->SetPortInfo(xpuIdx, portIdx);

            xpuNodes->Get(xpuIdx)->AddApplication(serverApp);
            serverApp->SetStartTime(Seconds(serverStart));
            serverApp->SetStopTime(Seconds(serverStop));
        }
    }
}

void
ApplicationDeployer::InstallClientsAndTrafficGenerators (const SueSimulationConfig& config, TopologyBuilder& topologyBuilder)
{
    uint32_t nXpus = config.network.nXpus;
    uint32_t suesPerXpu = config.network.suesPerXpu;
    double clientStart = config.timing.clientStart;
    double clientStop = config.GetClientStop ();

    NodeContainer* xpuNodes = topologyBuilder.GetXpuNodes ();

    // Install client applications and traffic generators (SUE-based creation method)
    for (uint32_t xpuIdx = 0; xpuIdx < nXpus; ++xpuIdx)
    {
        // Create SueClient list for this XPU - Now based on SUE count
        std::vector<Ptr<SueClient>> sueClientsForXpu(suesPerXpu);

        // Create all SUE clients for this XPU
        for (uint32_t sueIdx = 0; sueIdx < suesPerXpu; ++sueIdx)
        {
            Ptr<SueClient> sueClient = CreateSueClient (xpuIdx, sueIdx, config, topologyBuilder);
            sueClientsForXpu[sueIdx] = sueClient;
        }

        // Create load balancer
        Ptr<LoadBalancer> loadBalancer = CreateLoadBalancer (xpuIdx, sueClientsForXpu, config);

        // Create traffic generator for this XPU
        if (config.traffic.enableFineGrainedMode)
        {
            /***************************************/
            // Fine-grained traffic control mode
            /**************************************/
            NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Creating ConfigurableTrafficGenerator");
            Ptr<ConfigurableTrafficGenerator> configGen = CreateConfigurableTrafficGenerator (xpuIdx, loadBalancer, config);

            // Install configurable traffic generator to XPU node
            xpuNodes->Get(xpuIdx)->AddApplication(configGen);
            configGen->SetStartTime(Seconds(clientStart));
            configGen->SetStopTime(Seconds(clientStop));

            NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Configurable traffic generator installed from "
                    << clientStart << "s to " << clientStop << "s");
        }
        else if (config.traffic.enableTraceMode) {
            /***************************************/
            // Trace mode
            /**************************************/
            NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Creating TraceTrafficGenerator");
            Ptr<TraceTrafficGenerator> traceGen = CreateTraceTrafficGenerator (xpuIdx, loadBalancer, config);

            // Install trace traffic generator to XPU node
            xpuNodes->Get(xpuIdx)->AddApplication(traceGen);
            traceGen->SetStartTime(Seconds(clientStart));
            traceGen->SetStopTime(Seconds(clientStop));

            NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Trace traffic generator installed from "
                    << clientStart << "s to " << clientStop << "s");
        } else {
            /***************************************/
            // Uniform traffic generation mode
            /**************************************/
            NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Creating traditional TrafficGenerator");
            Ptr<TrafficGenerator> trafficGen = CreateTrafficGenerator (xpuIdx, loadBalancer, config);

            // Install traditional traffic generator to XPU node
            xpuNodes->Get(xpuIdx)->AddApplication(trafficGen);
            trafficGen->SetStartTime(Seconds(clientStart));
            trafficGen->SetStopTime(Seconds(clientStop));

            NS_LOG_INFO("XPU" << xpuIdx + 1 << ": "
                    << std::to_string(config.traffic.threadRate)
                    << "Mbps traffic generator from "
                    << clientStart << "s to " << clientStop << "s");
        }

        // Set destination queue space available callback for each SUE
        for (uint32_t sueIdx = 0; sueIdx < suesPerXpu; ++sueIdx) {
            sueClientsForXpu[sueIdx]->SetDestQueueSpaceCallback([loadBalancer](uint32_t sueId, uint32_t destXpuId, uint8_t vcId) {
                loadBalancer->NotifyDestQueueSpaceAvailable(sueId, destXpuId, vcId);
            });
        }
        NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Destination queue space callbacks set for all SUEs");

        // Connect trace sources to PerformanceLogger
        PerformanceLogger& logger = PerformanceLogger::GetInstance();

        // Connect buffer queue change trace
        loadBalancer->TraceConnectWithoutContext("BufferQueueChange",
                                                MakeCallback(&PerformanceLogger::BufferQueueChangeTraceCallback, &logger));

        NS_LOG_INFO("XPU" << xpuIdx + 1 << ": LoadBalancer trace callbacks connected to PerformanceLogger");
    }
}

Ptr<SueClient>
ApplicationDeployer::CreateSueClient (uint32_t xpuIdx, uint32_t sueIdx,
                                      const SueSimulationConfig& config, TopologyBuilder& topologyBuilder)
{
    uint32_t transactionSize = config.traffic.transactionSize;
    uint32_t maxBurstSize = config.traffic.maxBurstSize;
    uint32_t destQueueMaxBytes = config.queue.destQueueMaxBytes;
    uint8_t vcNum = config.traffic.vcNum;
    std::string SchedulingInterval = config.delay.SchedulingInterval;
    std::string PackingDelayPerPacket = config.delay.PackingDelayPerPacket;
    std::string ClientStatInterval = config.trace.ClientStatInterval;
    uint32_t portsPerSue = config.network.portsPerSue;
    double clientStart = config.timing.clientStart;
    double serverStop = config.GetServerStop ();

    Ptr<SueClient> sueClient = CreateObject<SueClient>();
    sueClient->SetAttribute("TransactionSize", UintegerValue(transactionSize));
    sueClient->SetAttribute("MaxBurstSize", UintegerValue(maxBurstSize));
    sueClient->SetAttribute("DestQueueMaxBytes", UintegerValue(destQueueMaxBytes));
    sueClient->SetAttribute("vcNum", UintegerValue(vcNum));
    sueClient->SetAttribute("SchedulingInterval", StringValue(SchedulingInterval));
    sueClient->SetAttribute("PackingDelayPerPacket", StringValue(PackingDelayPerPacket));
    sueClient->SetAttribute("ClientStatInterval", StringValue(ClientStatInterval));

    // Set SUE information (no longer single port, but SUE identifier)
    sueClient->SetXpuInfo(xpuIdx, sueIdx);
    sueClient->SetSueId(sueIdx);

    // Prepare device list managed by SUE
    std::vector<std::vector<Ptr<NetDevice>>>& xpuDevices = topologyBuilder.GetXpuDevices();
    std::vector<Ptr<PointToPointSueNetDevice>> managedDevices;
    for (uint32_t portInSue = 0; portInSue < portsPerSue; ++portInSue) {
        uint32_t globalPortIdx = sueIdx * portsPerSue + portInSue;
        Ptr<NetDevice> netDev = xpuDevices[xpuIdx][globalPortIdx];
        Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice>(netDev);
        if (p2pDev) {
            managedDevices.push_back(p2pDev);
        }
    }

    // Set SUE managed devices
    sueClient->SetManagedDevices(managedDevices);

    NodeContainer* xpuNodes = topologyBuilder.GetXpuNodes();
    xpuNodes->Get(xpuIdx)->AddApplication(sueClient);
    sueClient->SetStartTime(Seconds(clientStart));
    sueClient->SetStopTime(Seconds(serverStop));

    NS_LOG_INFO("Created SUE" << (sueIdx + 1) << " for XPU" << (xpuIdx + 1)
               << " managing " << managedDevices.size() << " ports");

    return sueClient;
}

Ptr<LoadBalancer>
ApplicationDeployer::CreateLoadBalancer (uint32_t xpuIdx,
                                        const std::vector<Ptr<SueClient>>& sueClientsForXpu,
                                        const SueSimulationConfig& config)
{
    uint32_t nXpus = config.network.nXpus;
    uint32_t hashSeed = config.loadBalance.hashSeed;
    uint32_t loadBalanceAlgorithm = config.loadBalance.loadBalanceAlgorithm;
    uint32_t prime1 = config.loadBalance.prime1;
    uint32_t prime2 = config.loadBalance.prime2;
    bool useVcInHash = config.loadBalance.useVcInHash;
    bool enableBitOperations = config.loadBalance.enableBitOperations;
    uint32_t suesPerXpu = config.network.suesPerXpu;

    // Create load balancer
    Ptr<LoadBalancer> loadBalancer = CreateObject<LoadBalancer>();
    loadBalancer->SetAttribute("LocalXpuId", UintegerValue(xpuIdx));
    loadBalancer->SetAttribute("MaxXpuId", UintegerValue(nXpus - 1));
    loadBalancer->SetAttribute("HashSeed", UintegerValue(hashSeed + xpuIdx * 31)); // Use command line parameter seed
    loadBalancer->SetAttribute("LoadBalanceAlgorithm", UintegerValue(loadBalanceAlgorithm));
    loadBalancer->SetAttribute("Prime1", UintegerValue(prime1));
    loadBalancer->SetAttribute("Prime2", UintegerValue(prime2));
    loadBalancer->SetAttribute("UseVcInHash", BooleanValue(useVcInHash));
    loadBalancer->SetAttribute("EnableBitOperations", BooleanValue(enableBitOperations));
    loadBalancer->SetAttribute("EnableAlternativePath", BooleanValue(config.loadBalance.enableAlternativePath));

    // Register SueClient to LoadBalancer
    for (uint32_t sueIdx = 0; sueIdx < suesPerXpu; ++sueIdx) {
        loadBalancer->AddSueClient(sueClientsForXpu[sueIdx], sueIdx);
    }

    return loadBalancer;
}

Ptr<TrafficGenerator>
ApplicationDeployer::CreateTrafficGenerator (uint32_t xpuIdx, Ptr<LoadBalancer> loadBalancer,
                                           const SueSimulationConfig& config)
{
    // Create traditional traffic generator
    NS_LOG_INFO("XPU" << xpuIdx + 1 << ": Creating traditional TrafficGenerator");
    uint32_t transactionSize = config.traffic.transactionSize;
    double threadRate = config.traffic.threadRate;
    uint32_t nXpus = config.network.nXpus;
    uint8_t vcNum = config.traffic.vcNum;
    uint32_t totalBytesToSend = config.traffic.totalBytesToSend;
    uint32_t maxBurstSize = config.traffic.maxBurstSize;

    // Create traffic generator for this XPU
    Ptr<TrafficGenerator> trafficGen = CreateObject<TrafficGenerator>();
    trafficGen->SetAttribute("TransactionSize", UintegerValue(transactionSize));
    trafficGen->SetAttribute("DataRate", DataRateValue(DataRate(std::to_string(threadRate) + "Mbps")));
    trafficGen->SetAttribute("MinXpuId", UintegerValue(0));
    trafficGen->SetAttribute("MaxXpuId", UintegerValue(nXpus - 1));
    trafficGen->SetAttribute("MinVcId", UintegerValue(0));
    trafficGen->SetAttribute("MaxVcId", UintegerValue(vcNum - 1));
    trafficGen->SetAttribute("TotalBytesToSend", UintegerValue(totalBytesToSend));
    trafficGen->SetAttribute("MaxBurstSize", UintegerValue(maxBurstSize));

    // Configure traffic generator: Set load balancer
    trafficGen->SetLoadBalancer(loadBalancer);
    trafficGen->SetLocalXpuId(xpuIdx);  // 0-based

    // Set TrafficGenerator to LoadBalancer (for traffic control)
    loadBalancer->SetTrafficGenerator(trafficGen);

    return trafficGen;
}

Ptr<TraceTrafficGenerator>
ApplicationDeployer::CreateTraceTrafficGenerator (uint32_t xpuIdx, Ptr<LoadBalancer> loadBalancer,
                                                 const SueSimulationConfig& config)
{
    uint32_t transactionSize = config.traffic.transactionSize;
    uint32_t nXpus = config.network.nXpus;
    uint8_t vcNum = config.traffic.vcNum;
    uint32_t maxBurstSize = config.traffic.maxBurstSize;
    std::string traceFilePath = config.traffic.traceFilePath;

    // Create trace traffic generator for this XPU
    Ptr<TraceTrafficGenerator> traceTrafficGen = CreateObject<TraceTrafficGenerator>();
    traceTrafficGen->SetAttribute("TransactionSize", UintegerValue(transactionSize));
    traceTrafficGen->SetAttribute("MinXpuId", UintegerValue(0));
    traceTrafficGen->SetAttribute("MaxXpuId", UintegerValue(nXpus - 1));
    traceTrafficGen->SetAttribute("MinVcId", UintegerValue(0));
    traceTrafficGen->SetAttribute("MaxVcId", UintegerValue(vcNum - 1));
    traceTrafficGen->SetAttribute("MaxBurstSize", UintegerValue(maxBurstSize));
    traceTrafficGen->SetAttribute("TraceFile", StringValue(traceFilePath));

    // Configure trace traffic generator
    traceTrafficGen->SetLoadBalancer(loadBalancer);
    traceTrafficGen->SetLocalXpuId(xpuIdx);  // 0-based

    // Note: LoadBalancer doesn't yet support SetTraceTrafficGenerator
    // This would need to be added to LoadBalancer class for full functionality
    
    // Set TrafficGenerator to LoadBalancer (for traffic control)
    loadBalancer->SetTrafficGenerator(traceTrafficGen);

    return traceTrafficGen;
}

Ptr<ConfigurableTrafficGenerator>
ApplicationDeployer::CreateConfigurableTrafficGenerator (uint32_t xpuIdx, Ptr<LoadBalancer> loadBalancer,
                                                const SueSimulationConfig& config)
{
    NS_LOG_FUNCTION (this << xpuIdx << loadBalancer << &config);

    uint32_t transactionSize = config.traffic.transactionSize;
    uint32_t maxBurstSize = config.traffic.maxBurstSize;

    // Parse fine-grained traffic configuration
    std::vector<FineGrainedTrafficFlow> fineGrainedFlows = ParseFineGrainedTrafficConfig (config);

    // Create configurable traffic generator
    Ptr<ConfigurableTrafficGenerator> configTrafficGen = CreateObject<ConfigurableTrafficGenerator>();

    // Set attributes
    configTrafficGen->SetAttribute("TransactionSize", UintegerValue(transactionSize));
    configTrafficGen->SetAttribute("MaxBurstSize", UintegerValue(maxBurstSize));

    // Configure configurable traffic generator
    configTrafficGen->SetLoadBalancer(loadBalancer);
    configTrafficGen->SetLocalXpuId(xpuIdx);  // 0-based
    configTrafficGen->SetFineGrainedFlows(fineGrainedFlows);

    // Set TrafficGenerator to LoadBalancer (for traffic control)
    loadBalancer->SetTrafficGenerator(configTrafficGen);

    return configTrafficGen;
}

std::vector<FineGrainedTrafficFlow>
ApplicationDeployer::ParseFineGrainedTrafficConfig (const SueSimulationConfig& config)
{
    NS_LOG_FUNCTION (this << &config);

    std::vector<FineGrainedTrafficFlow> flows;

    if (!config.traffic.enableFineGrainedMode || config.traffic.fineGrainedConfigFile.empty())
    {
        NS_LOG_WARN("Fine-grained mode not enabled or config file not specified");
        return flows;
    }

    std::ifstream configFile(config.traffic.fineGrainedConfigFile);
    if (!configFile.is_open())
    {
        NS_LOG_ERROR("Cannot open fine-grained traffic configuration file: " << config.traffic.fineGrainedConfigFile);
        exit(0);
        return flows;
    }

    std::string line;
    uint32_t lineNumber = 0;

    while (std::getline(configFile, line))
    {
        lineNumber++;

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        // Skip CSV header line
        if (line.find("sourceXpuId") != std::string::npos &&
            line.find("destXpuId") != std::string::npos)
        {
            continue;
        }

        // Parse line: sourceXpuId,destXpuId,sueId,suePort,vcId,dataRate,totalBytes
        std::istringstream iss(line);
        std::string token;
        std::vector<std::string> tokens;

        while (std::getline(iss, token, ','))
        {
            // Trim whitespace
            token.erase(0, token.find_first_not_of(" \t"));
            token.erase(token.find_last_not_of(" \t") + 1);
            tokens.push_back(token);
        }

        if (tokens.size() < 7)
        {
            NS_LOG_WARN("Invalid line " << lineNumber << " in fine-grained config: " << line
                      << " (expected at least 7 fields, got " << tokens.size() << ")");
            continue;
        }

        try
        {
            FineGrainedTrafficFlow flow;
            flow.sourceXpuId = static_cast<uint32_t>(std::stoul(tokens[0])); // 0-based
            flow.destXpuId = static_cast<uint32_t>(std::stoul(tokens[1]));   // 0-based
            flow.sueId = static_cast<uint32_t>(std::stoul(tokens[2]));      // 0-based
            flow.suePort = static_cast<uint32_t>(std::stoul(tokens[3]));         // 0-based port
            flow.vcId = static_cast<uint8_t>(std::stoul(tokens[4]));            // VC ID (0-3)
            flow.dataRate = std::stod(tokens[5]);                                // Mbps
            flow.totalBytes = static_cast<uint32_t>(std::stoul(tokens[6]));     // Bytes

            // Validate VC ID range
            if (flow.vcId > 3)
            {
                NS_LOG_WARN("VC ID " << (uint32_t)flow.vcId << " out of range (0-3) on line " << lineNumber
                          << ", using VC 0");
                flow.vcId = 0;
            }

            flows.push_back(flow);

            NS_LOG_INFO("Parsed flow: XPU" << flow.sourceXpuId + 1 << " -> XPU" << flow.destXpuId + 1
                      << " via SUE" << flow.sueId + 1 << ":Port" << flow.suePort << " at " << flow.dataRate << " Mbps"
                      << " on VC" << (uint32_t)flow.vcId << " for " << flow.totalBytes << " bytes");
        }
        catch (const std::exception& e)
        {
            NS_LOG_ERROR("Error parsing line " << lineNumber << " in fine-grained config: " << e.what());
        }
    }

    configFile.close();

    NS_LOG_INFO("Parsed " << flows.size() << " fine-grained traffic flows from "
              << config.traffic.fineGrainedConfigFile);

    return flows;
}

} // namespace ns3