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
#include "sue-server.h"
#include "ns3/sue-header.h"
#include "sue-client.h"
#include "ns3/core-module.h"
#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "xpu-delay-tag.h"
#include "performance-logger.h"

namespace ns3 {
NS_LOG_COMPONENT_DEFINE("SueServerApplication");
NS_OBJECT_ENSURE_REGISTERED(SueServer);

TypeId SueServer::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::SueServer")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<SueServer>()
        .AddAttribute("Port", "The port on which to listen for incoming packets.",
                      UintegerValue(9), MakeUintegerAccessor(&SueServer::m_port),
                      MakeUintegerChecker<uint16_t>())
        .AddAttribute("TransactionSize", "The size of transaction.",
                      UintegerValue(256), MakeUintegerAccessor(&SueServer::m_transactionSize),
                      MakeUintegerChecker<uint16_t>());
    return tid;
}

SueServer::SueServer() : 
            m_packetsReceived(0), 
            m_transactionsUnpacked(0),
            m_nodeId(0),
            m_xpuId(0),
            m_portId(0),
            m_transactionSize(256)
            {}
SueServer::~SueServer() { m_socket = 0; }
void SueServer::DoDispose(void) { Application::DoDispose(); }

void SueServer::SetPortInfo(uint32_t xpuId, uint32_t portId) {
    m_xpuId = xpuId;
    m_portId = portId;
}

void SueServer::StartApplication(void) {
    m_nodeId = GetNode()->GetId();

    if (m_socket == nullptr) {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        if (m_socket->Bind(local) == -1) {
            NS_FATAL_ERROR("Failed to bind socket");
        }
    }
    m_socket->SetRecvCallback(MakeCallback(&SueServer::HandleRead, this));
}

void SueServer::StopApplication(void) {
    if (m_socket != nullptr) {
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket->Close();
    }
    NS_LOG_INFO(Simulator::Now().GetSeconds() << "s XPU" << m_xpuId 
               << "-Port" << m_portId << ": Received " << m_packetsReceived 
               << " packets, unpacked " << m_transactionsUnpacked << " transactions.");
    std::cout << Simulator::Now().GetSeconds() << "s XPU" << m_xpuId 
               << "-Port" << m_portId << ": Received " << m_packetsReceived 
               << " packets, unpacked " << m_transactionsUnpacked << " transactions." << std::endl;

    Ptr<Node> node =  GetNode();
    for (uint32_t i = 0; i < node->GetNDevices(); i++) {
        Ptr<NetDevice> dev = node->GetDevice(i);
        Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice>(dev);
        // Check if cast is successful
        if (p2pDev && p2pDev->GetIfIndex() == m_portId){
            std::cout << Simulator::Now().GetSeconds() << "s XPU" << m_xpuId 
            << "-Port" << m_portId << ": Dropped " << p2pDev->GetTotalPacketDropNum()
            << " packets" << std::endl;
        }
    }
}

// HandleRead function updated to handle packets containing multiple transactions
void SueServer::HandleRead(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() >= 8) {
            m_packetsReceived++;
            SueHeader requestHeader;
            packet->RemoveHeader(requestHeader);

            // Try to read XPU timestamp tag
            XpuDelayTag timestampTag;
            bool hasTimestamp = packet->PeekPacketTag(timestampTag);

            // Calculate end-to-end delay
            Time delay = Time(0);
            if (hasTimestamp) {
                Time sendTime = timestampTag.GetTimestamp();
                delay = Simulator::Now() - sendTime;

                // Record delay statistics
                uint64_t timeNs = Simulator::Now().GetNanoSeconds();
                double delayNs = delay.GetNanoSeconds();

                PerformanceLogger::GetInstance().LogXpuDelay(
                    timeNs, m_xpuId, m_portId, delayNs);

                NS_LOG_INFO(Simulator::Now().GetSeconds() << " [XPU" << m_xpuId
                            << "-Port" << m_portId << "] received packet with "
                            << requestHeader << ", end-to-end delay: " << delayNs << "ns");
            } else {
                NS_LOG_DEBUG("No XPU timestamp tag found in packet");
            }

            // Unpack transactions
            // Assume each transaction has fixed size (e.g., 128 bytes).
            // Calculate number of transactions by dividing total payload size by single transaction size.
            uint32_t payloadSize = packet->GetSize();

            uint32_t numTransactions = (payloadSize > 0) ? (payloadSize / m_transactionSize) : 1;
            //TODO Unpack transactions->schedule

            // TODO Server identifier
            m_transactionsUnpacked  += numTransactions;

            // Send response (ACK), logic remains unchanged
            // uint8_t vcId=0;
            // SueHeader responseHeader;
            // responseHeader.SetOp(1);
            // responseHeader.SetVc(vcId);
            // responseHeader.SetRpsn(requestHeader.GetPsn());
            // responseHeader.SetXpuId(requestHeader.GetXpuId());
            // Ptr<Packet> responsePacket = Create<Packet>(0);
            // responsePacket->AddHeader(responseHeader);

            // InetSocketAddress inetFrom = InetSocketAddress::ConvertFrom(from);
            // NS_LOG_INFO(Simulator::Now().GetSeconds() << " [Node "<< m_nodeId <<" Device "<< m_portId << "] received packet from "
            //         << inetFrom.GetIpv4() << ":" << inetFrom.GetPort() << " with "
            //         << requestHeader << ", containing " << numTransactions << " transaction(s).");


            // socket->SendTo(responsePacket, 0, from);
        }
    }
}
}