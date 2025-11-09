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

#include "sue-switch.h"
#include "point-to-point-sue-net-device.h"
#include "sue-cbfc.h"
#include "sue-llr.h"
#include "sue-utils.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/net-device.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SueSwitch");

NS_OBJECT_ENSURE_REGISTERED (SueSwitch);

TypeId
SueSwitch::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SueSwitch")
    .SetParent<Object> ()
    .SetGroupName ("SueSim")
    .AddConstructor<SueSwitch> ();
  return tid;
}

SueSwitch::SueSwitch ()
  : m_llrNodeManager (nullptr),
    m_llrSwitchPortManager (nullptr)
{
  NS_LOG_FUNCTION (this);
}

SueSwitch::~SueSwitch ()
{
  NS_LOG_FUNCTION (this);
}

void
SueSwitch::SetForwardingTable (const std::map<Mac48Address, uint32_t>& table)
{
  NS_LOG_FUNCTION (this);
  m_forwardingTable = table;
}

void
SueSwitch::ClearForwardingTable (void)
{
  NS_LOG_FUNCTION (this);
  m_forwardingTable.clear ();
}


bool
SueSwitch::ProcessSwitchForwarding (Ptr<Packet> packet,
                                    const EthernetHeader& ethHeader,
                                    Ptr<PointToPointSueNetDevice> currentDevice,
                                    uint16_t protocol,
                                    uint8_t vcId)
{
  NS_LOG_FUNCTION (this << packet << currentDevice << protocol << static_cast<uint32_t> (vcId));

  // Extract destination MAC address from packet
  Mac48Address destination = ethHeader.GetDestination ();

  // Lookup in forwarding table
  auto it = m_forwardingTable.find (destination);
  if (it == m_forwardingTable.end ())
    {
      NS_ASSERT_MSG (false, "No forwarding entry for destination " << destination);
      return false;
    }

  uint32_t outPortIndex = it->second;
  Ptr<Node> node = currentDevice->GetNode ();

  // Find device corresponding to the port on the node
  for (uint32_t i = 0; i < node->GetNDevices (); i++)
    {
      Ptr<NetDevice> dev = node->GetDevice (i);
      Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice> (dev);

      // Check if conversion is successful
      if (p2pDev && p2pDev->GetIfIndex () == outPortIndex)
        {
          // If current port is the egress port, directly enter VC queue
          // Switch egress port: replace SourceDestination MAC with current device MAC only during TransmitStart
          // If replaced with local MAC first, it's difficult to find the previous device's MAC later
          if (currentDevice->GetIfIndex () == outPortIndex)
            {
              // This won't actually execute here, because ingress port directly puts data into egress port's VC queue
              currentDevice->Send (packet->Copy (), destination, protocol);
              return true;
            }
          else
            {
              Mac48Address mac = Mac48Address::ConvertFrom (p2pDev->GetAddress ());

              // Switch internal LLR processing
              Ptr<Packet> packetCopy = packet->Copy ();

              // Apply LLR processing if enabled
              if (currentDevice->GetLlrEnabled () && m_llrSwitchPortManager)
                {
                  // LLR send processing for switch internal forwarding
                  m_llrSwitchPortManager->LlrSendPacket (packetCopy, vcId, mac);
                  NS_LOG_DEBUG ("LLR processing applied for switch internal forwarding");
                }

              // Check credits and forward if available
              Ptr<CbfcManager> cbfcManager = currentDevice->GetCbfcManager ();
              if (cbfcManager && cbfcManager->GetTxCredits (mac, vcId) > 0)
                {
                  // If current port is ingress port, hand over to egress port's receive queue
                  // When switch ingress port forwards, replace SourceDestination MAC with current device MAC
                  // to enable universal credit calculation based on SourceMac.
                  EthernetHeader ethTemp;
                  packet->RemoveHeader (ethTemp);
                  Mac48Address currentMac = Mac48Address::ConvertFrom (currentDevice->GetAddress ());
                  ethTemp.SetSource (currentMac);
                  packet->AddHeader (ethTemp);
                  
                  if (cbfcManager->IsLinkCbfcEnabled ())
                    {
                      cbfcManager->DecrementTxCredits (mac, vcId);
                      SueStatsUtils::ProcessCreditChangeStats(mac, vcId, cbfcManager->GetTxCredits(mac, vcId), currentDevice->GetNode()->GetId(), currentDevice->GetIfIndex() - 1);
                    }

                  // Switch forwarding delay - schedule the packet to be enqueued after delay
                  Simulator::Schedule (currentDevice->GetSwitchForwardDelay (),
                                     &PointToPointSueNetDevice::SpecDevEnqueueToVcQueue,
                                     currentDevice, p2pDev, packet->Copy ());

                  // Handle credit return - also delay by the same amount as forwarding
                  Simulator::Schedule (currentDevice->GetSwitchForwardDelay (),
                                     &CbfcManager::HandleCreditReturn,
                                     cbfcManager, ethHeader, vcId);
                  Simulator::Schedule (currentDevice->GetSwitchForwardDelay (),
                                     &CbfcManager::CreditReturn,
                                     cbfcManager, ethHeader.GetSource (), vcId);
                }
              else
                {
                  NS_LOG_INFO ("No credits available for forwarding to " << mac);
                  return false;
                }

              return true;
            }
        }
    }

  NS_LOG_INFO ("No output device found for port index " << outPortIndex);
  return false;
}

// LLR Manager Methods
void
SueSwitch::SetLlrNodeManager (Ptr<LlrNodeManager> llrNodeManager)
{
  NS_LOG_FUNCTION (this << llrNodeManager);
  m_llrNodeManager = llrNodeManager;
}

void
SueSwitch::SetLlrSwitchPortManager (Ptr<LlrSwitchPortManager> llrSwitchPortManager)
{
  NS_LOG_FUNCTION (this << llrSwitchPortManager);
  m_llrSwitchPortManager = llrSwitchPortManager;
}

Ptr<LlrNodeManager>
SueSwitch::GetLlrNodeManager (void) const
{
  return m_llrNodeManager;
}

Ptr<LlrSwitchPortManager>
SueSwitch::GetLlrSwitchPortManager (void) const
{
  return m_llrSwitchPortManager;
}

bool
SueSwitch::IsSwitchDevice (Mac48Address mac) const
{
  NS_LOG_FUNCTION (this << mac);

  uint8_t buffer[6];
  mac.CopyTo (buffer);
  uint8_t lastByte = buffer[5]; // Last byte of MAC address
  // TODO: Simplistic logic; needs modification for proper XPU/switch identification
  return (lastByte % 2 == 0); // Even numbers are switch devices
}

} // namespace ns3