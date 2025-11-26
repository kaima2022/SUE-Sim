/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2025 SUE-Sim Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
#include "ns3/common-utils.h"
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

    // === Simulation Parameters Configuration ===
    SueSimulationConfig config;
    config.ParseCommandLine (argc, argv);
    config.ValidateAndCalculate ();

    // Configure logging using parsed parameters
    SueUtils::ConfigureLogging (config);

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
