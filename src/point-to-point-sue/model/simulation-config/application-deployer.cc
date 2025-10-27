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
            serverApp->SetPortInfo(xpuIdx+1, portIdx+1);

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
        Ptr<TrafficGenerator> trafficGen = CreateTrafficGenerator (xpuIdx, loadBalancer, config);

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

        xpuNodes->Get(xpuIdx)->AddApplication(trafficGen);
        trafficGen->SetStartTime(Seconds(clientStart));
        trafficGen->SetStopTime(Seconds(clientStop));

        NS_LOG_INFO("XPU" << xpuIdx + 1 << ": "
                << std::to_string(config.traffic.threadRate)
                << "Mbps traffic generator from "
                << clientStart << "s to " << clientStop << "s");
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
    sueClient->SetXpuInfo(xpuIdx+1, sueIdx+1);
    sueClient->SetSueId(sueIdx+1);

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

} // namespace ns3