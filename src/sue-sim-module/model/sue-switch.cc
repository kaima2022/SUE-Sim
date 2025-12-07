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

#include "sue-switch.h"
#include "point-to-point-sue-net-device.h"
#include "sue-cbfc.h"
#include "sue-llr.h"
#include "sue-utils.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/net-device.h"
#include "ns3/data-rate.h"

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
  : m_forwardingBusy (false),
    m_llrNodeManager (nullptr),
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

  // Apply state machine: check if switch is busy forwarding
  if (m_forwardingBusy)
  {
    // Switch is busy, fail and let upper layer retry
    NS_LOG_DEBUG ("Switch busy, forwarding failed - upper layer will retry");
    return false;
  }
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

              // Check CBFC and credits if enabled
              Ptr<CbfcManager> cbfcManager = currentDevice->GetCbfcManager ();
              bool canForward = false;

              if (cbfcManager)
                {
                  if (cbfcManager->IsLinkCbfcEnabled ())
                    {
                      // CBFC enabled: check dynamic credits before forwarding
                      uint32_t packetSize = packet->GetSize ();
                      if (cbfcManager->HasEnoughCredits (mac, vcId, packetSize))
                        {
                          canForward = true;
                          if (cbfcManager->ConsumeDynamicCredits (mac, vcId, packetSize))
                            {
                              SueStatsUtils::ProcessCreditChangeStats(mac, vcId, cbfcManager->GetTxCredits(mac, vcId), currentDevice->GetNode()->GetId(), currentDevice->GetIfIndex() - 1);
                              NS_LOG_INFO ("Switch forwarding: consumed credits for packet size " << packetSize << " bytes to " << mac << " VC " << static_cast<uint32_t> (vcId));
                            }
                          else
                            {
                              canForward = false;
                              NS_LOG_INFO ("Switch forwarding: failed to consume credits for packet size " << packetSize << " bytes");
                            }
                        }
                      else
                        {
                          NS_LOG_INFO ("No enough credits for forwarding packet size " << packetSize << " bytes to " << mac);
                        }
                    }
                  else
                    {
                      // CBFC disabled: always allow forwarding
                      canForward = true;
                    }
                }
              else
                {
                  // No CBFC manager: always allow forwarding
                  canForward = true;
                }

              if (canForward)
                {
                  // Modify packet: replace source MAC with current device MAC
                  EthernetHeader ethTemp;
                  packet->RemoveHeader (ethTemp);
                  Mac48Address currentMac = Mac48Address::ConvertFrom (currentDevice->GetAddress ());
                  ethTemp.SetSource (currentMac);
                  packet->AddHeader (ethTemp);

                  // Calculate forwarding delay
                  Time forwardDelay = CalculateAdaptiveForwardDelay (currentDevice, packet->GetSize ());

                  NS_LOG_DEBUG ("Switch idle, starting immediate forwarding with delay: "
                                << forwardDelay.GetNanoSeconds () << "ns");

                  // Schedule forwarding completion after the delay
                  Simulator::Schedule (forwardDelay,
                                       &SueSwitch::ForwardingComplete,
                                       this,
                                       currentDevice, p2pDev, packet,
                                       ethHeader, vcId, ethHeader.GetSource ());

                  // Switch is idle, start forwarding immediately
                  m_forwardingBusy = true;
                }
              else
                {
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

Time
SueSwitch::CalculateAdaptiveForwardDelay (Ptr<PointToPointSueNetDevice> device, uint32_t packetSize)
{
  NS_LOG_FUNCTION (this << device << packetSize);

  // Get base forwarding delay from device configuration
  Time baseDelay = device->GetSwitchForwardDelay ();

  // Get actual data rate from device instead of hardcoding
  DataRate deviceRate = device->GetDataRate ();

  // Calculate size-based additional delay using device's actual data rate
  // This simulates the serialization delay in real switches
  // Use a fraction of actual transmission time to represent internal processing overhead
  Time sizeBasedDelay = deviceRate.CalculateBytesTxTime (packetSize * 1.0); // 10% of transmission time

  // Total adaptive delay = base delay + size-based component
  Time totalDelay = baseDelay + sizeBasedDelay;

  NS_LOG_DEBUG ("Calculated adaptive forward delay: " << totalDelay.GetNanoSeconds ()
                << "ns for packet size " << packetSize << " bytes (base: "
                << baseDelay.GetNanoSeconds () << "ns, size-based: "
                << sizeBasedDelay.GetNanoSeconds () << "ns) using device rate: "
                << deviceRate.GetBitRate () << "bps");

  return totalDelay;
}

void
SueSwitch::ForwardingComplete (Ptr<PointToPointSueNetDevice> originalDevice,
                               Ptr<PointToPointSueNetDevice> targetDevice,
                               Ptr<Packet> packet,
                               const EthernetHeader& ethHeader,
                               uint8_t vcId,
                               Mac48Address sourceMac)
{
  NS_LOG_FUNCTION (this);

  // Perform actual forwarding: enqueue to target device's VC queue
  originalDevice->SpecDevEnqueueToVcQueue (targetDevice, packet->Copy ());

  // Handle credit return with the same delay (if CBFC enabled)
  Ptr<CbfcManager> cbfcManager = originalDevice->GetCbfcManager ();
  if (cbfcManager && cbfcManager->IsLinkCbfcEnabled ())
    {
      Simulator::Schedule (NanoSeconds (0),  // Already delayed by forwarding delay
                         &CbfcManager::HandleCreditReturn,
                         cbfcManager, ethHeader, vcId, packet->GetSize ());
      Simulator::Schedule (NanoSeconds (0),
                         &CbfcManager::CreditReturn,
                         cbfcManager, sourceMac, vcId);
    }

  // Mark forwarding as complete
  m_forwardingBusy = false;

  NS_LOG_DEBUG ("Forwarding completed - switch is now idle");
}

} // namespace ns3