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
#include "ns3/sue-sim-module-module.h"
#include "ns3/sue-client.h"
#include "../sue-utils.h"
#include "../point-to-point-sue-net-device.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TopologyBuilder");

namespace {
void
SetReceiverErrorRate (Ptr<RateErrorModel> errorModel, double errorRate)
{
    if (errorModel)
    {
        errorModel->SetAttribute ("ErrorRate", DoubleValue (errorRate));
    }
}

bool
IsTopologyVerboseEnabled ()
{
    const char* raw = std::getenv ("SUE_TOPOLOGY_VERBOSE");
    if (raw == nullptr || *raw == '\0')
    {
        return false;
    }
    std::string s (raw);
    std::transform (s.begin (), s.end (), s.begin (),
                    [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
    return s == "1" || s == "true" || s == "yes" || s == "on";
}
} // namespace

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

    // Ensure role mapping (MAC -> switch/XPU) starts clean for each topology.
    PointToPointSueNetDevice::ClearRegisteredDeviceRoles ();

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

    if (IsTopologyVerboseEnabled ())
    {
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
    m_p2p.SetDeviceAttribute("SwitchCredits", UintegerValue(config.cbfc.SwitchCredits));
    m_p2p.SetDeviceAttribute("HeaderSize", UintegerValue(config.cbfc.HeaderSize));
    m_p2p.SetDeviceAttribute("TransactionSize", UintegerValue(config.traffic.transactionSize));
    // Validate CBFC credit windows against the worst-case data packet size.
    // In pipeline mode, TrafficConfig::maxBurstSize corresponds to the packed payload bytes.
    // `HeaderSize` captures network+link headers accounted in credit calculations.
    m_p2p.SetDeviceAttribute("CbfcCreditWindowPacketBytes",
                             UintegerValue(config.traffic.maxBurstSize + config.cbfc.HeaderSize));
    m_p2p.SetDeviceAttribute("BytesPerCredit", UintegerValue(config.cbfc.BytesPerCredit));
    m_p2p.SetDeviceAttribute("VcQueueMaxBytes", UintegerValue(config.queue.vcQueueMaxBytes));
    m_p2p.SetDeviceAttribute("ProcessingQueueMaxBytes", UintegerValue(config.queue.processingQueueMaxBytes));
    m_p2p.SetDeviceAttribute("ProcessingDelayPerPacket", StringValue(config.link.processingDelay));
    m_p2p.SetChannelAttribute("Delay", StringValue(config.link.LinkDelay));
    m_p2p.SetDeviceAttribute("EnableLinkCBFC", BooleanValue(config.cbfc.EnableLinkCBFC));
    m_p2p.SetDeviceAttribute("EnableCreditSync", BooleanValue(config.cbfc.EnableCreditSync));
    m_p2p.SetDeviceAttribute("CreditSyncInterval", StringValue(config.cbfc.CreditSyncInterval));
    m_p2p.SetDeviceAttribute("LinkCreditMode", UintegerValue(config.cbfc.LinkCreditMode));
    m_p2p.SetDeviceAttribute("ErrorModelApplyToControlPackets", BooleanValue(config.link.errorModelApplyToControlPackets));
    m_p2p.SetDeviceAttribute("ErrorModelApplyToSyncPackets", BooleanValue(config.link.errorModelApplyToSyncPackets));
    m_p2p.SetDeviceAttribute("CreUpdateAddHeadDelay", StringValue(config.delay.CreUpdateAddHeadDelay));
    m_p2p.SetDeviceAttribute("StatLoggingEnabled", BooleanValue(config.trace.statLoggingEnabled));
    m_p2p.SetDeviceAttribute("CreditGenerateDelay", StringValue(config.delay.creditGenerateDelay));
    m_p2p.SetDeviceAttribute("SwitchForwardDelay", StringValue(config.delay.switchForwardDelay));
    m_p2p.SetDeviceAttribute("ProcessingQueueScheduleDelay", StringValue(config.delay.processingQueueScheduleDelay));
    m_p2p.SetDeviceAttribute("AdditionalHeaderSize", UintegerValue(config.delay.additionalHeaderSize));

    // LLR configuration
    m_p2p.SetDeviceAttribute("EnableLLR", BooleanValue(config.llr.m_llrEnabled));
    m_p2p.SetDeviceAttribute("LlrProtectCbfcUpdates", BooleanValue(config.llr.LlrProtectCbfcUpdates));
    m_p2p.SetDeviceAttribute("LlrTimeout", StringValue(config.llr.LlrTimeout));
    m_p2p.SetDeviceAttribute("LlrWindowSize", UintegerValue(config.llr.LlrWindowSize));
    m_p2p.SetDeviceAttribute("AckAddHeaderDelay", StringValue(config.llr.AckAddHeaderDelay));
    m_p2p.SetDeviceAttribute("AckProcessDelay", StringValue(config.llr.AckProcessDelay));

    // Link layer delay parameter configuration - Activate queue scheduling and transmission only
    m_p2p.SetDeviceAttribute("VcSchedulingDelay", StringValue(config.delay.vcSchedulingDelay));

    // Error rate model
    m_errorModel = CreateObject<RateErrorModel>();
    m_errorModel->SetAttribute("ErrorRate", DoubleValue(config.link.errorRate));
    m_errorModel->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    // Optional: disable the receiver error model after a short duration since clientStart,
    // to emulate a burst of impairments followed by a clean link (helps observe recovery).
    if (config.link.errorRate > 0.0 && !config.link.errorRateStopAfter.empty())
    {
        Time stopAfter = Time (config.link.errorRateStopAfter);
        if (stopAfter.IsNegative ())
        {
            NS_LOG_WARN ("ErrorRateStopAfter is negative (" << config.link.errorRateStopAfter
                                                           << "), ignore.");
        }
        else
        {
            Time stopAt = Seconds (config.timing.clientStart) + stopAfter;
            Simulator::Schedule (stopAt, &SetReceiverErrorRate, m_errorModel, 0.0);
        }
    }
}

void
TopologyBuilder::CreateConnections (const SueSimulationConfig& config)
{
    uint32_t nXpus = config.network.nXpus;
    uint32_t portsPerSue = config.network.portsPerSue;
    uint32_t suesPerXpu = config.network.suesPerXpu;
    uint32_t totalSwitches = suesPerXpu;

    // IP address allocation:
    // Use a monotonic /30 subnet allocator to avoid octet overflow when nXpus is large.
    // (/30 gives exactly two host addresses per link: XPU and switch interface.)
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.252");
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

                // Assign one unique /30 subnet per physical link.
                Ipv4InterfaceContainer interfaces = address.Assign(devices);
                address.NewNetwork();

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

                // Register explicit switch/XPU roles + topology metadata for robust link-layer behaviors.
                // portId remains 1-based globalPortIdx+1.
                const uint32_t portId = globalPortIdx + 1;
                PointToPointSueNetDevice::RegisterDeviceMeta (mac,
                                                              false /*isSwitchDevice*/,
                                                              xpuIdx,
                                                              portId);
                PointToPointSueNetDevice::RegisterDeviceMeta (
                    Mac48Address::ConvertFrom (switchDev->GetAddress ()),
                    true /*isSwitchDevice*/,
                    xpuIdx /*connected XPU id*/,
                    portId);

                NS_LOG_INFO("Connected XPU" << (xpuIdx + 1) << " Port" << (globalPortIdx + 1)
                           << " to Switch" << (switchIdx + 1)
                           << " (SUE" << (sueIdx + 1) << ", IP: " << xpuPortIp << ")");
            }
        }
    }

    // Set global IP-MAC mapping table to SueClient and PointToPointSueNetDevice
    SueClient::SetGlobalIpMacMap(m_ipToMacMap);
    SuePacketUtils::SetGlobalIpMacMap(m_ipToMacMap);
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

    const bool verbose = IsTopologyVerboseEnabled ();
    if (verbose) {
        std::cout << "\n=== XPU Devices ===" << std::endl;
    }
    // Collect MAC addresses of all XPU devices during connection creation
    for (uint32_t portIdx = 0; portIdx < portsPerXpu; ++portIdx) {
        for (uint32_t xpuIdx = 0; xpuIdx < nXpus; ++xpuIdx) {
            // Get XPU device
            Ptr<NetDevice> xpuDev = m_xpuDevices[xpuIdx][portIdx];
            Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice>(xpuDev);
            if(p2pDev){
                p2pDev->InitializeCbfc();
                Mac48Address mac = Mac48Address::ConvertFrom(p2pDev->GetAddress());
                m_xpuMacAddresses[xpuIdx][portIdx] = mac;

                if (verbose) {
                    // Print collected MAC addresses
                    std::ostringstream macStream;
                    macStream << mac;
                    std::string macStr = macStream.str();
                    std::replace(macStr.begin(), macStr.end(), '-', ':');
                    std::cout << "XPU" << xpuIdx << " Port" << portIdx
                              << " MAC: " << macStr << std::endl;
                }
            }
        }
    }

    // ================= Build Switch Forwarding Tables =================
    if (verbose) {
        std::cout << "\n=== Building Global Switch Forwarding Tables ===" << std::endl;
    }

    // Create a global forwarding table, one for each switch
    std::vector<std::map<Mac48Address, uint32_t>> globalSwitchTables(totalSwitches);

    // Iterate through each switch
    for (uint32_t switchIdx = 0; switchIdx < totalSwitches; ++switchIdx) {
        Ptr<Node> switchNode = m_switchNodes.Get(switchIdx);
        if (verbose) {
            std::cout << "Switch" << switchIdx + 1 << " (Node " << switchNode->GetId() + 1 << "):" << std::endl;
        }

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

                if (verbose) {
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
        }
        if (verbose) {
            std::cout << std::endl;
        }
    }

    // ================= Set Global Forwarding Tables to All Devices =================
    for (uint32_t switchIdx = 0; switchIdx < totalSwitches; ++switchIdx) {
        // Share one SueSwitch instance across all ports on the same switch node so that
        // egress pipeline state and (optional) egress drop modeling are consistent.
        Ptr<SueSwitch> sharedSwitchModule = CreateObject<SueSwitch>();
        sharedSwitchModule->SetForwardingTable(globalSwitchTables[switchIdx]);
        sharedSwitchModule->SetEgressOverflowPolicy(
            (config.queue.switchEgressOverflowPolicy == 1) ? SueSwitch::EgressOverflowPolicy::DROP
                                                           : SueSwitch::EgressOverflowPolicy::RETRY);

        for (uint32_t devIdx = 0; devIdx < m_switchDevices[switchIdx].size(); ++devIdx) {
            Ptr<NetDevice> switchDev = m_switchDevices[switchIdx][devIdx];
            Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice>(switchDev);

            if (p2pDev) {
                // Point all switch ports at the shared switch module.
                p2pDev->SetSwitch(sharedSwitchModule);

                // Set complete global forwarding table
                auto switchModule = p2pDev->GetSwitch();
                if (switchModule) {
                    switchModule->SetForwardingTable(globalSwitchTables[switchIdx]);
                }

                if (verbose) {
                    // Print setting result
                    std::cout << "Switch" << switchIdx + 1 << " Dev" << devIdx + 1
                              << " set global forwarding table with "
                              << globalSwitchTables[switchIdx].size() << " entries" << std::endl;
                }

                // Initialize CBFC functionality
                p2pDev->InitializeCbfc();
            }
        }
    }
}

void
TopologyBuilder::PrintTopologyInfo (const SueSimulationConfig& config) const
{
    if (!IsTopologyVerboseEnabled ())
    {
        std::cout << "Topology summary: xpus=" << config.network.nXpus
                  << " ports_per_xpu=" << config.network.portsPerXpu
                  << " sues_per_xpu=" << config.network.suesPerXpu
                  << " links=" << m_serverInfos.size ()
                  << std::endl;
        return;
    }

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
