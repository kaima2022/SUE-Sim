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

#include "topology-builder.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-sue-module.h"
#include "ns3/sue-client.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TopologyBuilder");

TopologyBuilder::TopologyBuilder ()
{
}

TopologyBuilder::~TopologyBuilder ()
{
}

void
TopologyBuilder::BuildTopology (const SueSimulationConfig& config)
{
    NS_LOG_INFO ("Building network topology");

    CreateNodes (config);
    InstallNetworkStack (config);
    ConfigurePointToPointHelper (config);
    CreateConnections (config);
    BuildForwardingTables (config);
    PrintTopologyInfo (config);

    NS_LOG_INFO ("Network topology build completed");
}

void
TopologyBuilder::CreateNodes (const SueSimulationConfig& config)
{
    uint32_t nXpus = config.network.nXpus;
    uint32_t suesPerXpu = config.network.suesPerXpu;

    // Create XPU nodes
    m_xpuNodes.Create(nXpus);

    // Create switch nodes - now based on SUE count
    uint32_t totalSwitches = suesPerXpu;  // Number of switches = number of SUEs per XPU
    m_switchNodes.Create(totalSwitches);

    // Print XPU node IDs (base 1)
    std::cout << "XPU Node IDs: ";
    for (uint32_t i = 0; i < m_xpuNodes.GetN(); ++i) {
        std::cout << m_xpuNodes.Get(i)->GetId() + 1;
        if (i != m_xpuNodes.GetN() - 1) std::cout << ", ";
    }
    std::cout << std::endl;

    // Print switch node IDs (base 1)
    std::cout << "Switch Node IDs: ";
    for (uint32_t i = 0; i < m_switchNodes.GetN(); ++i) {
        std::cout << m_switchNodes.Get(i)->GetId() + 1;
        if (i != m_switchNodes.GetN() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
}

void
TopologyBuilder::InstallNetworkStack (const SueSimulationConfig& config)
{
    // Install network protocol stack
    InternetStackHelper stack;
    stack.Install(m_xpuNodes);
    stack.Install(m_switchNodes);
}

void
TopologyBuilder::ConfigurePointToPointHelper (const SueSimulationConfig& config)
{
    // Configure point-to-point links
    m_p2p.SetDeviceAttribute("NumVcs", UintegerValue(config.link.numVcs));
    m_p2p.SetDeviceAttribute("DataRate", StringValue(config.link.LinkDataRate));
    m_p2p.SetDeviceAttribute("ProcessingRate", StringValue(config.link.ProcessingRate));
    m_p2p.SetDeviceAttribute("Mtu", UintegerValue(config.traffic.Mtu));
    m_p2p.SetDeviceAttribute("InitialCredits", UintegerValue(config.cbfc.LinkCredits));
    m_p2p.SetDeviceAttribute("CreditBatchSize", UintegerValue(config.cbfc.CreditBatchSize));
    m_p2p.SetDeviceAttribute("VcQueueMaxBytes", UintegerValue(config.queue.vcQueueMaxBytes));
    m_p2p.SetDeviceAttribute("ProcessingQueueMaxBytes", UintegerValue(config.queue.processingQueueMaxBytes));
    m_p2p.SetDeviceAttribute("ProcessingDelayPerPacket", StringValue(config.link.processingDelay));
    m_p2p.SetChannelAttribute("Delay", StringValue(config.link.LinkDelay));
    m_p2p.SetDeviceAttribute("EnableLinkCBFC", BooleanValue(config.cbfc.EnableLinkCBFC));
    m_p2p.SetDeviceAttribute("LinkStatInterval", StringValue(config.trace.LinkStatInterval));
    m_p2p.SetDeviceAttribute("CreUpdateAddHeadDelay", StringValue(config.delay.CreUpdateAddHeadDelay));
    m_p2p.SetDeviceAttribute("DataAddHeadDelay", StringValue(config.delay.DataAddHeadDelay));
    m_p2p.SetDeviceAttribute("StatLoggingEnabled", BooleanValue(config.trace.statLoggingEnabled));
    m_p2p.SetDeviceAttribute("CreditGenerateDelay", StringValue(config.delay.creditGenerateDelay));
    m_p2p.SetDeviceAttribute("SwitchForwardDelay", StringValue(config.delay.switchForwardDelay));
    m_p2p.SetDeviceAttribute("AdditionalHeaderSize", UintegerValue(config.delay.additionalHeaderSize));

    // Link layer delay parameter configuration - Activate queue scheduling and transmission only
    m_p2p.SetDeviceAttribute("VcSchedulingDelay", StringValue(config.delay.vcSchedulingDelay));

    // Error rate model
    m_errorModel = CreateObject<RateErrorModel>();
    m_errorModel->SetAttribute("ErrorRate", DoubleValue(config.link.errorRate));
}

void
TopologyBuilder::CreateConnections (const SueSimulationConfig& config)
{
    uint32_t nXpus = config.network.nXpus;
    uint32_t portsPerSue = config.network.portsPerSue;
    uint32_t suesPerXpu = config.network.suesPerXpu;
    uint32_t totalSwitches = suesPerXpu;

    // IP address allocation
    Ipv4AddressHelper address;
    m_xpuPortIps.resize(nXpus);

    // Containers for storing device pointers and MAC addresses - Modified to SUE-based storage
    m_xpuDevices.resize(nXpus);
    m_switchDevices.resize(totalSwitches);  // Based on total switch count

    // Create XPU-Switch connections (SUE-based connection method)
    // Devices managed by the same SUE connect to the same switch
    for (uint32_t xpuIdx = 0; xpuIdx < nXpus; ++xpuIdx)
    {
        for (uint32_t sueIdx = 0; sueIdx < suesPerXpu; ++sueIdx)
        {
            // Calculate switch index: All XPUs with the same SUE index correspond to the same switch
            uint32_t switchIdx = sueIdx;

            for (uint32_t portInSue = 0; portInSue < portsPerSue; ++portInSue)
            {
                // Calculate global port index
                uint32_t globalPortIdx = sueIdx * portsPerSue + portInSue;

                // Create network device (Connect XPU's globalPortIdx port to corresponding switch)
                NodeContainer linkNodes(m_xpuNodes.Get(xpuIdx), m_switchNodes.Get(switchIdx));
                NetDeviceContainer devices = m_p2p.Install(linkNodes);
                devices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(m_errorModel));

                // Assign IP address (10.<XPU>.<port>.0/30)
                std::ostringstream subnet;
                subnet << "10." << xpuIdx + 1 << "." << globalPortIdx + 1 << ".0";
                address.SetBase(subnet.str().c_str(), "255.255.255.252");
                Ipv4InterfaceContainer interfaces = address.Assign(devices);

                // Save port IP
                Ipv4Address xpuPortIp = interfaces.GetAddress(0);
                m_xpuPortIps[xpuIdx].push_back(xpuPortIp);

                // Add to server list (All ports are potential servers)
                m_serverInfos.push_back({xpuPortIp, 8080 + globalPortIdx});

                // Get and store device pointers
                Ptr<NetDevice> xpuDev = devices.Get(0);
                Ptr<NetDevice> switchDev = devices.Get(1);
                m_xpuDevices[xpuIdx].push_back(xpuDev);
                m_switchDevices[switchIdx].push_back(switchDev);

                // Establish IP -> MAC mapping after IP address assignment
                Ipv4Address ip = interfaces.GetAddress(0); // XPU side IP
                Ptr<NetDevice> dev = devices.Get(0); // XPU device
                Mac48Address mac = Mac48Address::ConvertFrom(dev->GetAddress());

                m_ipToMacMap[ip] = mac;

                NS_LOG_INFO("Connected XPU" << (xpuIdx + 1) << " Port" << (globalPortIdx + 1)
                           << " to Switch" << (switchIdx + 1)
                           << " (SUE" << (sueIdx + 1) << ", IP: " << xpuPortIp << ")");
            }
        }
    }

    // Set global IP-MAC mapping table to SueClient and PointToPointSueNetDevice
    SueClient::SetGlobalIpMacMap(m_ipToMacMap);
    PointToPointSueNetDevice::SetGlobalIpMacMap(m_ipToMacMap);
}

void
TopologyBuilder::BuildForwardingTables (const SueSimulationConfig& config)
{
    uint32_t nXpus = config.network.nXpus;
    uint32_t portsPerXpu = config.network.portsPerXpu;
    uint32_t portsPerSue = config.network.portsPerSue;
    uint32_t suesPerXpu = config.network.suesPerXpu;
    uint32_t totalSwitches = suesPerXpu;

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ================= Build Global XPU MAC Address Table =================
    // Create a 2D array to store MAC addresses of all XPU devices
    m_xpuMacAddresses.resize(nXpus, std::vector<Mac48Address>(portsPerXpu));

    std::cout << "\n=== XPU Devices ===" << std::endl;
    // Collect MAC addresses of all XPU devices during connection creation
    for (uint32_t portIdx = 0; portIdx < portsPerXpu; ++portIdx) {
        for (uint32_t xpuIdx = 0; xpuIdx < nXpus; ++xpuIdx) {
            // Get XPU device
            Ptr<NetDevice> xpuDev = m_xpuDevices[xpuIdx][portIdx];
            Mac48Address mac = Mac48Address::ConvertFrom(xpuDev->GetAddress());
            m_xpuMacAddresses[xpuIdx][portIdx] = mac;

            // Print collected MAC addresses
            std::ostringstream macStream;
            macStream << mac;
            std::string macStr = macStream.str();
            std::replace(macStr.begin(), macStr.end(), '-', ':');
            std::cout << "XPU" << xpuIdx << " Port" << portIdx << " MAC: " << macStr << std::endl;
        }
    }

    // ================= Build Switch Forwarding Tables =================
    std::cout << "\n=== Building Global Switch Forwarding Tables ===" << std::endl;

    // Create a global forwarding table, one for each switch
    std::vector<std::map<Mac48Address, uint32_t>> globalSwitchTables(totalSwitches);

    // Iterate through each switch
    for (uint32_t switchIdx = 0; switchIdx < totalSwitches; ++switchIdx) {
        Ptr<Node> switchNode = m_switchNodes.Get(switchIdx);
        std::cout << "Switch" << switchIdx + 1 << " (Node " << switchNode->GetId() + 1 << "):" << std::endl;

        // Build forwarding table for each switch
        // switchIdx corresponds to SUE index, this switch only connects corresponding SUE ports of each XPU
        for (uint32_t xpuIdx = 0; xpuIdx < nXpus; ++xpuIdx) {

            // Add forwarding table entries only for ports actually connected to this switch
            // switchIdx corresponds to SUE index, managing portsPerSue ports
            uint32_t sueManagedPortStart = switchIdx * portsPerSue;
            uint32_t sueManagedPortEnd = sueManagedPortStart + portsPerSue - 1;

            for (uint32_t xpuPortIdx = sueManagedPortStart; xpuPortIdx <= sueManagedPortEnd; ++xpuPortIdx) {
                // Get MAC address of target XPU port
                Mac48Address xpuMac = m_xpuMacAddresses[xpuIdx][xpuPortIdx];

                uint32_t switchPortIdx = portsPerSue * xpuIdx + (xpuPortIdx - sueManagedPortStart);

                Ptr<NetDevice> switchDev = m_switchDevices[switchIdx][switchPortIdx];

                // Add to forwarding table: target MAC -> outgoing port device's GetIfIndex()
                globalSwitchTables[switchIdx][xpuMac] = switchDev->GetIfIndex();

                // Print forwarding table entry
                std::ostringstream macStream;
                macStream << xpuMac;
                std::string macStr = macStream.str();
                std::replace(macStr.begin(), macStr.end(), '-', ':');
                std::cout << "  XPU" << xpuIdx << " Port" << xpuPortIdx + 1
                        << " -> DeviceIndex:" << switchDev->GetIfIndex()
                        << " MAC: " << macStr << std::endl;
            }
        }
        std::cout << std::endl;
    }

    // ================= Set Global Forwarding Tables to All Devices =================
    for (uint32_t switchIdx = 0; switchIdx < totalSwitches; ++switchIdx) {
        for (uint32_t devIdx = 0; devIdx < m_switchDevices[switchIdx].size(); ++devIdx) {
            Ptr<NetDevice> switchDev = m_switchDevices[switchIdx][devIdx];
            Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice>(switchDev);

            if (p2pDev) {
                // Set complete global forwarding table
                p2pDev->SetForwardingTable(globalSwitchTables[switchIdx]);

                // Print setting result
                std::cout << "Switch" << switchIdx + 1 << " Dev" << devIdx + 1
                        << " set global forwarding table with "
                        << globalSwitchTables[switchIdx].size() << " entries" << std::endl;
            }
        }
    }
}

void
TopologyBuilder::PrintTopologyInfo (const SueSimulationConfig& config) const
{
    // IP to MAC Mapping Table
    std::cout << "\nIP to MAC Mapping Table:" << std::endl;
    for (const auto& entry : m_ipToMacMap) {
        std::cout << "IP: " << entry.first << " -> MAC: " << entry.second << std::endl;
    }

    // Print all server information
    std::cout << "\nServer Information:" << std::endl;
    std::cout << "-------------------" << std::endl;
    for (const auto& server : m_serverInfos)
    {
        std::cout << "IP: " << server.first << ", Port: " << server.second << std::endl;
    }
    std::cout << "Total servers: " << m_serverInfos.size() << std::endl;

    uint32_t totalSwitches = config.network.suesPerXpu;

    // Print switch device information - Updated to SUE-based topology
    std::cout << "\n=== SwitchNode Devices (SUE-based topology) ===" << std::endl;
    for (uint32_t switchIdx = 0; switchIdx < totalSwitches; ++switchIdx) {

        std::cout << "Switch" << switchIdx << " has "
                  << m_switchDevices[switchIdx].size() << " devices:" << std::endl;

        for (uint32_t devIdx = 0; devIdx < m_switchDevices[switchIdx].size(); ++devIdx) {
            Ptr<NetDevice> dev = m_switchDevices[switchIdx][devIdx];
            Mac48Address mac = Mac48Address::ConvertFrom(dev->GetAddress());

            // Format as concise MAC address (00:00:00:00:00:00)
            std::ostringstream macStream;
            macStream << mac;
            std::string macStr = macStream.str();
            std::replace(macStr.begin(), macStr.end(), '-', ':');

            std::cout << "  Dev" << (devIdx+1)
                    << " Ptr: " << dev
                    << " MAC: " << macStr
                    << std::endl;
        }
    }

    // Print XPU device information
    std::cout << "\n=== XPU Devices ===" << std::endl;
    for (uint32_t xpuIdx = 0; xpuIdx < config.network.nXpus; ++xpuIdx) {
        for (uint32_t devIdx = 0; devIdx < m_xpuDevices[xpuIdx].size(); ++devIdx) {
            Ptr<NetDevice> dev = m_xpuDevices[xpuIdx][devIdx];
            Mac48Address mac = Mac48Address::ConvertFrom(dev->GetAddress());

            // Format as concise MAC address
            std::ostringstream macStream;
            macStream << mac;
            std::string macStr = macStream.str();
            std::replace(macStr.begin(), macStr.end(), '-', ':');

            std::cout << "XPU" << xpuIdx << " Dev" << (devIdx+1)
                    << " Ptr: " << dev
                    << " MAC: " << macStr
                    << std::endl;
        }
    }
}

NodeContainer*
TopologyBuilder::GetXpuNodes ()
{
    return &m_xpuNodes;
}

NodeContainer*
TopologyBuilder::GetSwitchNodes ()
{
    return &m_switchNodes;
}

std::vector<std::vector<Ptr<NetDevice>>>&
TopologyBuilder::GetXpuDevices ()
{
    return m_xpuDevices;
}

std::vector<std::vector<Ptr<NetDevice>>>&
TopologyBuilder::GetSwitchDevices ()
{
    return m_switchDevices;
}

std::vector<std::vector<Ipv4Address>>&
TopologyBuilder::GetXpuPortIps ()
{
    return m_xpuPortIps;
}

std::vector<std::pair<Ipv4Address, uint16_t>>&
TopologyBuilder::GetServerInfos ()
{
    return m_serverInfos;
}

std::vector<std::vector<Mac48Address>>&
TopologyBuilder::GetXpuMacAddresses ()
{
    return m_xpuMacAddresses;
}

} // namespace ns3