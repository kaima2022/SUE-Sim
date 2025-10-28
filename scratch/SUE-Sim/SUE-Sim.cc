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

/**
 * \file
 * \brief SUE-Sim main simulation program
 *
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sue-sim-module-module.h"
#include "ns3/applications-module.h"
#include "ns3/sue-client.h"
#include "ns3/sue-server.h"
#include "ns3/traffic-generator.h"
#include "ns3/load-balancer.h"
#include "ns3/performance-logger.h"
#include "ns3/parameter-config.h"
#include "ns3/topology-builder.h"
#include "ns3/application-deployer.h"
#include "ns3/sue-utils.h"
#include <iomanip>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SueSimulation");

/**
 * \brief Main function for SUE simulation
 * \param argc Argument count
 * \param argv Argument vector
 * \return Exit status
 */
int
main (int argc, char* argv[])
{
    // Initialize timing and performance logging
    std::string sessionId = SueUtils::StartTiming ();
    SueUtils::InitializePerformanceLogger ("performance.csv");
    SueUtils::ConfigureLogging ();

    // === Simulation Parameters Configuration ===
    SueSimulationConfig config;
    config.ParseCommandLine (argc, argv);
    config.ValidateAndCalculate ();
    config.PrintConfiguration ();

    // Extract simulation time for convenience
    double simulationTime = config.timing.simulationTime;

    // ================= Topology Creation =================
    TopologyBuilder topologyBuilder;
    topologyBuilder.BuildTopology (config);

    // ================= Application Deployment =================
    ApplicationDeployer appDeployer;
    appDeployer.DeployApplications (config, topologyBuilder);

    // ================= Run Simulation =================
    NS_LOG_INFO("Starting SUE Simulation with XPU-Switch Topology");
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation completed");

    // ================= End Timing =================
    SueUtils::EndTiming (sessionId);

    return 0;
}
