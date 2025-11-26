/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2025 SUE-Sim Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef TOPOLOGY_BUILDER_H
#define TOPOLOGY_BUILDER_H

#include "parameter-config.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sue-sim-module-module.h"
#include <vector>
#include <map>

namespace ns3 {

/**
 * \brief Network topology builder for SUE simulation
 *
 * This class is responsible for creating and configuring the network topology
 * including XPU nodes, switch nodes, network devices, IP addresses,
 * and forwarding tables for the SUE simulation.
 */
class TopologyBuilder
{
public:
    /**
     * \brief Constructor
     */
    TopologyBuilder ();

    /**
     * \brief Destructor
     */
    ~TopologyBuilder ();

    /**
     * \brief Build the complete network topology
     * \param config Simulation configuration parameters
     */
    void BuildTopology (const SueSimulationConfig& config);

    /**
     * \brief Get XPU nodes container
     * \return Pointer to XPU node container
     */
    NodeContainer* GetXpuNodes ();

    /**
     * \brief Get switch nodes container
     * \return Pointer to switch node container
     */
    NodeContainer* GetSwitchNodes ();

    /**
     * \brief Get XPU network devices
     * \return Reference to XPU devices vector
     */
    std::vector<std::vector<Ptr<NetDevice>>>& GetXpuDevices ();

    /**
     * \brief Get switch network devices
     * \return Reference to switch devices vector
     */
    std::vector<std::vector<Ptr<NetDevice>>>& GetSwitchDevices ();

    /**
     * \brief Get XPU port IP addresses
     * \return Reference to XPU port IPs vector
     */
    std::vector<std::vector<Ipv4Address>>& GetXpuPortIps ();

    /**
     * \brief Get server information list
     * \return Reference to server information vector
     */
    std::vector<std::pair<Ipv4Address, uint16_t>>& GetServerInfos ();

    /**
     * \brief Get XPU MAC addresses
     * \return Reference to XPU MAC addresses vector
     */
    std::vector<std::vector<Mac48Address>>& GetXpuMacAddresses ();

private:
    /**
     * \brief Create XPU and switch nodes based on configuration
     * \param config Simulation configuration parameters
     */
    void CreateNodes (const SueSimulationConfig& config);

    /**
     * \brief Install network protocol stack on all nodes
     * \param config Simulation configuration parameters
     */
    void InstallNetworkStack (const SueSimulationConfig& config);

    /**
     * \brief Configure point-to-point helper with link parameters
     * \param config Simulation configuration parameters
     */
    void ConfigurePointToPointHelper (const SueSimulationConfig& config);

    /**
     * \brief Create XPU-Switch connections using SUE-based topology
     * \param config Simulation configuration parameters
     */
    void CreateConnections (const SueSimulationConfig& config);

    /**
     * \brief Build global switch forwarding tables for efficient routing
     * \param config Simulation configuration parameters
     */
    void BuildForwardingTables (const SueSimulationConfig& config);

    /**
     * \brief Print detailed topology information for debugging
     * \param config Simulation configuration parameters
     */
    void PrintTopologyInfo (const SueSimulationConfig& config) const;

    // Node containers
    NodeContainer m_xpuNodes;
    NodeContainer m_switchNodes;

    // Network devices
    std::vector<std::vector<Ptr<NetDevice>>> m_xpuDevices;
    std::vector<std::vector<Ptr<NetDevice>>> m_switchDevices;

    // IP addresses
    std::vector<std::vector<Ipv4Address>> m_xpuPortIps;
    std::vector<std::pair<Ipv4Address, uint16_t>> m_serverInfos;

    // MAC addresses
    std::vector<std::vector<Mac48Address>> m_xpuMacAddresses;

    // Helper
    PointToPointSueHelper m_p2p;

    // Error model
    Ptr<RateErrorModel> m_errorModel;

    // Global IP to MAC mapping table
    std::map<Ipv4Address, Mac48Address> m_ipToMacMap;
};

} // namespace ns3

#endif /* TOPOLOGY_BUILDER_H */