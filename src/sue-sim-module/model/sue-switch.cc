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
#include "sue-ppp-header.h"
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
  : m_egressOverflowPolicy (EgressOverflowPolicy::RETRY),
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

void
SueSwitch::SetEgressOverflowPolicy (EgressOverflowPolicy policy)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (policy));
  m_egressOverflowPolicy = policy;
}

SueSwitch::EgressOverflowPolicy
SueSwitch::GetEgressOverflowPolicy (void) const
{
  return m_egressOverflowPolicy;
}

bool
SueSwitch::LookupOutPortIndex (const Mac48Address &destination, uint32_t *outPortIndex) const
{
  NS_LOG_FUNCTION (this << destination);
  if (!outPortIndex)
    {
      return false;
    }
  auto it = m_forwardingTable.find (destination);
  if (it == m_forwardingTable.end ())
    {
      return false;
    }
  *outPortIndex = it->second;
  return true;
}

Ptr<PointToPointSueNetDevice>
SueSwitch::FindPortDevice (Ptr<Node> node, uint32_t outPortIndex) const
{
  NS_LOG_FUNCTION (this << node << outPortIndex);
  if (!node)
    {
      return nullptr;
    }
  for (uint32_t i = 0; i < node->GetNDevices (); i++)
    {
      Ptr<NetDevice> dev = node->GetDevice (i);
      Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice> (dev);
      if (p2pDev && p2pDev->GetIfIndex () == outPortIndex)
        {
          return p2pDev;
        }
    }
  return nullptr;
}

bool
SueSwitch::TryReserveEgressVcQueueBytes (uint32_t outPortIndex,
                                        uint8_t vcId,
                                        uint32_t bytes,
                                        Ptr<PointToPointSueNetDevice> targetDevice)
{
  NS_LOG_FUNCTION (this << outPortIndex << static_cast<uint32_t> (vcId) << bytes << targetDevice);

  if (m_egressOverflowPolicy != EgressOverflowPolicy::DROP)
    {
      return true;
    }
  if (!targetDevice)
    {
      return false;
    }

  // Target device's VC queue parameters are initialized as part of CBFC init.
  // In certain traffic patterns (e.g., incast), an egress port may be used for
  // forwarding before it has received any packets itself. Ensure the egress
  // port's queue manager has been initialized before admission checks.
  targetDevice->InitializeCbfc ();

  Ptr<SueQueueManager> qm = targetDevice->GetQueueManager ();
  if (!qm)
    {
      return false;
    }

  const uint32_t usedBytes = qm->GetVcQueueBytes (vcId);
  const uint32_t maxBytes = qm->GetVcQueueMaxBytes ();
  const uint64_t reserved = m_egressReservedBytes[outPortIndex][vcId];
  const uint64_t projected = static_cast<uint64_t> (usedBytes) + reserved + bytes;

  if (projected > maxBytes)
    {
      return false;
    }

  m_egressReservedBytes[outPortIndex][vcId] = reserved + bytes;
  return true;
}

void
SueSwitch::ReleaseEgressVcQueueBytes (uint32_t outPortIndex, uint8_t vcId, uint32_t bytes)
{
  NS_LOG_FUNCTION (this << outPortIndex << static_cast<uint32_t> (vcId) << bytes);

  auto portIt = m_egressReservedBytes.find (outPortIndex);
  if (portIt == m_egressReservedBytes.end ())
    {
      return;
    }
  auto &vcMap = portIt->second;
  auto vcIt = vcMap.find (vcId);
  if (vcIt == vcMap.end ())
    {
      return;
    }
  if (vcIt->second <= bytes)
    {
      vcIt->second = 0;
    }
  else
    {
      vcIt->second -= bytes;
    }
}

uint64_t
SueSwitch::GetReservedEgressVcQueueBytes (uint32_t outPortIndex, uint8_t vcId) const
{
  auto portIt = m_egressReservedBytes.find (outPortIndex);
  if (portIt == m_egressReservedBytes.end ())
    {
      return 0;
    }
  auto vcIt = portIt->second.find (vcId);
  if (vcIt == portIt->second.end ())
    {
      return 0;
    }
  return vcIt->second;
}


bool
SueSwitch::ProcessSwitchForwarding (Ptr<Packet> packet,
                                    const EthernetHeader& ethHeader,
                                    Ptr<PointToPointSueNetDevice> currentDevice,
                                    uint16_t protocol,
                                    uint8_t vcId,
                                    bool skipSwitchInternalCbfcCredits)
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

		              // Check switch-internal CBFC credits (ingress -> egress) if enabled.
		              // NOTE: In switch DROP overflow policy, we may pre-consume these credits
		              // during egress admission to avoid reserving egress bytes for packets that
		              // are later blocked by internal CBFC.
		              Ptr<CbfcManager> cbfcManager = currentDevice->GetCbfcManager ();
		              bool canForward = false;

		              if (cbfcManager)
		                {
		                  if (cbfcManager->IsLinkCbfcEnabled ())
		                    {
		                      if (skipSwitchInternalCbfcCredits)
		                        {
		                          canForward = true;
		                        }
		                      else
		                        {
		                          // CBFC enabled: check dynamic credits before forwarding.
		                          // Use on-wire size (include PPP header) to match credit return.
		                          SuePppHeader ppp;
		                          const uint32_t onWireSize = packet->GetSize () + ppp.GetSerializedSize ();
		                          if (cbfcManager->HasEnoughCredits (mac, vcId, onWireSize))
		                            {
		                              canForward = true;
		                              if (cbfcManager->ConsumeDynamicCredits (mac, vcId, onWireSize))
		                                {
		                                  SueStatsUtils::ProcessCreditChangeStats (mac, vcId, cbfcManager->GetTxCredits (mac, vcId),
		                                                                          currentDevice->GetNode ()->GetId (),
		                                                                          currentDevice->GetIfIndex () - 1);
		                                  NS_LOG_INFO ("Switch forwarding: consumed credits for packet size " << onWireSize
		                                                                                                     << " bytes to " << mac
		                                                                                                     << " VC " << static_cast<uint32_t> (vcId));
		                                }
		                              else
		                                {
		                                  canForward = false;
		                                  NS_LOG_INFO ("Switch forwarding: failed to consume credits for packet size " << onWireSize << " bytes");
		                                }
		                            }
		                          else
		                            {
		                              canForward = false;
		                              NS_LOG_INFO ("No enough credits for forwarding packet size " << onWireSize << " bytes to " << mac);
		                            }
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

                  ForwardingRequest req;
                  req.originalDevice = currentDevice;
                  req.targetDevice = p2pDev;
                  req.packet = packet;
                  req.ethHeader = ethHeader;
                  req.vcId = vcId;
                  req.sourceMac = ethHeader.GetSource ();

                  // One pipeline per egress port: if busy, enqueue; otherwise start now.
                  if (m_egressBusy[outPortIndex])
                    {
                      m_egressQueues[outPortIndex].push_back (req);
                      return true;
                    }

                  m_egressBusy[outPortIndex] = true;
                  m_egressQueues[outPortIndex].push_back (req);
                  StartNextOnEgressPort (outPortIndex);
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

  bool isSwitch = false;
  if (PointToPointSueNetDevice::LookupRegisteredDeviceRole (mac, &isSwitch))
    {
      return isSwitch;
    }

  uint8_t buffer[6];
  mac.CopyTo (buffer);
  uint8_t lastByte = buffer[5]; // Last byte of MAC address
  // Fallback heuristic for legacy setups: switch devices get even MAC addresses.
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
  const Time txTime = deviceRate.CalculateBytesTxTime (packetSize);
  const Time sizeBasedDelay = NanoSeconds (txTime.GetNanoSeconds () / 10); // 10% of serialization time

  // Total adaptive delay = base delay
  Time totalDelay = baseDelay;

  NS_LOG_DEBUG ("Calculated adaptive forward delay: " << totalDelay.GetNanoSeconds ()
                << "ns for packet size " << packetSize << " bytes (base: "
                << baseDelay.GetNanoSeconds () << "ns, size-based: "
                << sizeBasedDelay.GetNanoSeconds () << "ns) using device rate: "
                << deviceRate.GetBitRate () << "bps");

  return totalDelay;
}

void
SueSwitch::StartNextOnEgressPort (uint32_t outPortIndex)
{
  auto& q = m_egressQueues[outPortIndex];
  if (q.empty ())
    {
      m_egressBusy[outPortIndex] = false;
      return;
    }

  ForwardingRequest req = q.front ();
  q.pop_front ();

  // Calculate forwarding delay
  Time forwardDelay = CalculateAdaptiveForwardDelay (req.originalDevice, req.packet->GetSize ());

  NS_LOG_DEBUG ("Switch egress " << outPortIndex << " forwarding with delay: "
                << forwardDelay.GetNanoSeconds () << "ns (pending=" << q.size () << ")");

  Simulator::Schedule (forwardDelay,
                       &SueSwitch::ForwardingComplete,
                       this,
                       req.originalDevice, req.targetDevice, req.packet,
                       req.ethHeader, req.vcId, req.sourceMac, outPortIndex);
}

void
SueSwitch::ForwardingComplete (Ptr<PointToPointSueNetDevice> originalDevice,
                               Ptr<PointToPointSueNetDevice> targetDevice,
                               Ptr<Packet> packet,
                               const EthernetHeader& ethHeader,
                               uint8_t vcId,
                               Mac48Address sourceMac,
                               uint32_t outPortIndex)
{
  NS_LOG_FUNCTION (this);

  // Perform actual forwarding: enqueue to target device's VC queue
  const bool enqueued =
      originalDevice->SpecDevEnqueueToVcQueue (targetDevice, packet->Copy ());
  if (!enqueued)
    {
      if (m_egressOverflowPolicy == EgressOverflowPolicy::RETRY)
        {
          // Egress VC queue is full. Do not drop: keep the request on this egress
          // pipeline and retry later. Also do not return credits yet so that
          // upstream backpressure is preserved under CBFC.
          //
          // NOTE: Avoid extremely tight busy-retry loops (1ns) which can explode
          // the event count under heavy contention (e.g., large packed bursts).
          // Use a conservative retry delay tied to the link serialization time
          // of the packet on the egress device.
          Time retryDelay = NanoSeconds (1);
          if (targetDevice)
            {
              retryDelay = targetDevice->GetDataRate ().CalculateBytesTxTime (packet->GetSize ());
              if (retryDelay.IsZero ())
                {
                  retryDelay = NanoSeconds (1);
                }
            }
          Simulator::Schedule (retryDelay,
                               &SueSwitch::ForwardingComplete,
                               this,
                               originalDevice,
                               targetDevice,
                               packet,
                               ethHeader,
                               vcId,
                               sourceMac,
                               outPortIndex);
          return;
        }

      // DROP mode: model lossy egress. Do not return credits (credit leak is
      // intentional; periodic credit sync + LLR should recover).
      SuePppHeader ppp;
      const uint32_t projectedBytes = packet->GetSize () + ppp.GetSerializedSize ();
      ReleaseEgressVcQueueBytes (outPortIndex, vcId, projectedBytes);

      NS_LOG_INFO ("Switch egress drop: outPortIndex=" << outPortIndex
                                                      << " vc=" << static_cast<uint32_t> (vcId)
                                                      << " bytes=" << projectedBytes);

      // Advance egress pipeline even on drop.
      StartNextOnEgressPort (outPortIndex);
      return;
    }

  if (m_egressOverflowPolicy == EgressOverflowPolicy::DROP)
    {
      SuePppHeader ppp;
      const uint32_t projectedBytes = packet->GetSize () + ppp.GetSerializedSize ();
      ReleaseEgressVcQueueBytes (outPortIndex, vcId, projectedBytes);
    }

  // Switch-internal CBFC credit return is handled by the switch egress port when the
  // packet is actually transmitted on the outgoing link (PointToPointSueNetDevice::TransmitStart()).
  //
  // Returning credits here (at enqueue-time) would artificially inflate the internal
  // window and can re-introduce egress over-admission under contention.

  // Advance egress pipeline.
  StartNextOnEgressPort (outPortIndex);
}

} // namespace ns3
