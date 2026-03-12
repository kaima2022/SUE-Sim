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

#ifndef APPLICATION_DEPLOYER_H
#define APPLICATION_DEPLOYER_H

#include "parameter-config.h"
#include "topology-builder.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/sue-client.h"
#include "ns3/sue-server.h"
#include "ns3/traffic-generator.h"
#include "ns3/traffic-generator-trace.h"
#include "ns3/traffic-generator-config.h"
#include "ns3/load-balancer.h"
#include "ns3/performance-logger.h"
#include <vector>

namespace ns3 {

// Forward declaration
class TopologyBuilder;

/**
 * \brief Application deployer for SUE simulation
 *
 * This class is responsible for deploying and configuring all applications
 * in the SUE simulation including servers, clients, traffic generators,
 * and load balancers.
 */
class ApplicationDeployer
{
public:
    /**
     * \brief Constructor
     */
    ApplicationDeployer ();

    /**
     * \brief Destructor
     */
    ~ApplicationDeployer ();

    /**
     * \brief Deploy all applications on the topology
     * \param config Simulation configuration parameters
     * \param topologyBuilder Reference to topology builder
     */
    void DeployApplications (const SueSimulationConfig& config, TopologyBuilder& topologyBuilder);

private:
    /**
     * \brief Install server applications on all XPU ports
     * \param config Simulation configuration parameters
     * \param topologyBuilder Reference to topology builder
     */
    void InstallServers (const SueSimulationConfig& config, TopologyBuilder& topologyBuilder);

    /**
     * \brief Install client applications and traffic generators
     * \param config Simulation configuration parameters
     * \param topologyBuilder Reference to topology builder
     */
    void InstallClientsAndTrafficGenerators (const SueSimulationConfig& config, TopologyBuilder& topologyBuilder);

    /**
     * \brief Create and configure a SUE client
     * \param xpuIdx XPU index (0-based)
     * \param sueIdx SUE index within XPU (0-based)
     * \param config Simulation configuration parameters
     * \param topologyBuilder Reference to topology builder
     * \return Pointer to created SUE client
     */
    Ptr<SueClient> CreateSueClient (uint32_t xpuIdx, uint32_t sueIdx,
                                   const SueSimulationConfig& config, TopologyBuilder& topologyBuilder);

    /**
     * \brief Create and configure a load balancer for an XPU
     * \param xpuIdx XPU index (0-based)
     * \param sueClientsForXpu Vector of SUE clients for this XPU
     * \param config Simulation configuration parameters
     * \return Pointer to created load balancer
     */
    Ptr<LoadBalancer> CreateLoadBalancer (uint32_t xpuIdx,
                                         const std::vector<Ptr<SueClient>>& sueClientsForXpu,
                                         const SueSimulationConfig& config);

    /**
     * \brief Create and configure a traffic generator for an XPU
     * \param xpuIdx XPU index (0-based)
     * \param loadBalancer Pointer to load balancer
     * \param config Simulation configuration parameters
     * \return Pointer to created traffic generator (Application*)
     */
    Ptr<TrafficGenerator> CreateTrafficGenerator (uint32_t xpuIdx, Ptr<LoadBalancer> loadBalancer,
                                           const SueSimulationConfig& config);

    /**
     * \brief Create and configure a trace-based traffic generator for an XPU
     * \param xpuIdx XPU index (0-based)
     * \param loadBalancer Pointer to load balancer
     * \param config Simulation configuration parameters
     * \return Pointer to created trace traffic generator (as Application*)
     */
    Ptr<TraceTrafficGenerator> CreateTraceTrafficGenerator (uint32_t xpuIdx, Ptr<LoadBalancer> loadBalancer,
                                                const SueSimulationConfig& config);

    /**
     * \brief Create and configure a configurable traffic generator for an XPU
     * \param xpuIdx XPU index (0-based)
     * \param loadBalancer Pointer to load balancer
     * \param config Simulation configuration parameters
     * \return Pointer to created configurable traffic generator (as Application*)
     */
    Ptr<ConfigurableTrafficGenerator> CreateConfigurableTrafficGenerator (uint32_t xpuIdx, Ptr<LoadBalancer> loadBalancer,
                                                               const SueSimulationConfig& config);

    /**
     * \brief Parse fine-grained traffic configuration file
     * \param config Simulation configuration parameters
     * \return Vector of parsed traffic flows
     */
    std::vector<FineGrainedTrafficFlow> ParseFineGrainedTrafficConfig (const SueSimulationConfig& config);
};

} // namespace ns3

#endif /* APPLICATION_DEPLOYER_H */