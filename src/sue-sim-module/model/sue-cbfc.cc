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

#include "sue-cbfc.h"
#include "sue-cbfc-header.h"
#include "point-to-point-sue-net-device.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/queue-size.h"
#include "ns3/mac48-address.h"
#include <unordered_map>
#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("CbfcManager");

NS_OBJECT_ENSURE_REGISTERED(CbfcManager);

TypeId
CbfcManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::CbfcManager")
                          .SetParent<Object> ()
                          .SetGroupName ("PointToPointSue")
                          .AddConstructor<CbfcManager> ()
                          .AddAttribute ("NumVcs", "The number of Virtual Channels.",
                                        UintegerValue (4),
                                        MakeUintegerAccessor (&CbfcManager::m_numVcs),
                                        MakeUintegerChecker<uint8_t> ())
                          .AddAttribute ("InitialCredits", "The initial credits for each VC.",
                                        UintegerValue (20),
                                        MakeUintegerAccessor (&CbfcManager::m_initialCredits),
                                        MakeUintegerChecker<uint32_t> ())
                          .AddAttribute ("EnableLinkCBFC", "If enable LINK CBFC.",
                                        BooleanValue (false),
                                        MakeBooleanAccessor (&CbfcManager::m_enableLinkCBFC),
                                        MakeBooleanChecker ())
                          .AddAttribute ("CreditBatchSize", "The credit batch size.",
                                        UintegerValue (1),
                                        MakeUintegerAccessor (&CbfcManager::m_creditBatchSize),
                                        MakeUintegerChecker<uint32_t> ())
                          .AddAttribute ("LinkCreditMode",
                                        "Link credit allocation mode: 0 = SHARED (all VCs share one pool), "
                                        "1 = EXCLUSIVE (each VC gets total/numVcs).",
                                        UintegerValue (static_cast<uint8_t> (LinkCreditMode::SHARED)),
                                        MakeUintegerAccessor (&CbfcManager::m_linkCreditModeRaw),
                                        MakeUintegerChecker<uint8_t> (0, 1));
  return tid;
}

CbfcManager::CbfcManager ()
  : m_initialized (false),
    m_enableLinkCBFC (false),
    m_linkPeerMac (),
    m_linkPeerMacSet (false),
    m_linkCreditMode (LinkCreditMode::SHARED),
    m_linkCreditModeRaw (static_cast<uint8_t> (LinkCreditMode::SHARED)),
    m_initialCredits (20),
    m_numVcs (4),
    m_creditBatchSize (1),
    m_enableDynamicCredits (true),
    m_baseCredit (1),
    m_transactionSize (256),
    m_headerSize (52),
    m_bytesPerCredit (256),
    m_getLocalMac (),
    m_getNode (),
    m_sendPacket (),
    m_creditGenerateDelay (Seconds (0.0)),
    m_protocolNum (0),
    m_callbacksSet (false)
{
  NS_LOG_FUNCTION (this);
}

CbfcManager::~CbfcManager ()
{
  NS_LOG_FUNCTION (this);
}

void
CbfcManager::Configure (uint8_t numVcs,
                       uint32_t initialCredits,
                       bool enableLinkCBFC,
                       uint32_t creditBatchSize)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (numVcs) << initialCredits
                   << enableLinkCBFC << creditBatchSize);

  m_numVcs = numVcs;
  m_initialCredits = initialCredits;
  m_enableLinkCBFC = enableLinkCBFC;
  m_creditBatchSize = creditBatchSize;
}

void
CbfcManager::InitializeBasic (void)
{
  NS_LOG_FUNCTION (this);

  if (m_initialized)
    {
      return;
    }

  // Sync credit mode from raw attribute value
  m_linkCreditMode = static_cast<LinkCreditMode> (m_linkCreditModeRaw);

  // Clear existing data structures
  m_txCreditsMap.clear ();
  m_rxCreditsToReturnMap.clear ();
  m_sharedTxCreditsMap.clear ();

  m_initialized = true;
  NS_LOG_INFO ("CbfcManager initialized with " << static_cast<uint32_t> (m_numVcs)
               << " VCs and " << m_initialCredits << " initial credits"
               << ", mode=" << (m_linkCreditMode == LinkCreditMode::SHARED ? "SHARED" : "EXCLUSIVE"));
}

void
CbfcManager::Initialize (uint8_t numVcs,
                        uint32_t initialCredits,
                        bool enableLinkCBFC,
                        uint32_t creditBatchSize,
                        GetLocalMacCallback getLocalMac,
                        GetNodeCallback getNode,
                        SendPacketCallback sendPacket,
                        Time creditGenerateDelay,
                        uint16_t protocolNum,
                        std::function<Mac48Address()> getRemoteMac,
                        std::function<bool()> isSwitchDevice,
                        uint32_t switchCredits,
                        uint32_t creditWindowPacketBytes)
{
  NS_LOG_FUNCTION (this);

  // Configure parameters
  Configure (numVcs, initialCredits, enableLinkCBFC,
            creditBatchSize);

  // Set callbacks first (required for InitializePeerDeviceCredits)
  SetCallbacks (getLocalMac, getNode, sendPacket,
               creditGenerateDelay, protocolNum);

  // Initialize CBFC manager
  InitializeBasic ();

  // Initialize peer device credits
  InitializePeerDeviceCredits (getRemoteMac, isSwitchDevice, switchCredits, creditWindowPacketBytes);

  NS_LOG_INFO ("CBFC initialized with " << static_cast<uint32_t> (numVcs)
               << " VCs and " << initialCredits << " initial credits");
}

void
CbfcManager::AddPeerDevice (Mac48Address peerMac, uint32_t initialCredits)
{
  NS_LOG_FUNCTION (this << peerMac << initialCredits);

  if (!m_initialized)
    {
      InitializeBasic ();
    }

  uint32_t credits = (initialCredits == 0) ? m_initialCredits : initialCredits;

  // Record link peer MAC only on the first call (the link peer).
  // Subsequent calls are for switch-internal peers and must not overwrite.
  const bool isLinkPeer = !m_linkPeerMacSet;
  if (isLinkPeer)
    {
      m_linkPeerMac = peerMac;
      m_linkPeerMacSet = true;
    }

  if (isLinkPeer && m_linkCreditMode == LinkCreditMode::SHARED)
    {
      // Shared mode: one credit pool for all VCs on the link
      m_sharedTxCreditsMap[peerMac] = credits;
      NS_LOG_INFO ("Added link peer " << peerMac << " with SHARED credit pool = " << credits);
    }
  else
    {
      // Switch-internal peers always use per-VC credits (regardless of link credit mode).
      // Link peer in EXCLUSIVE mode also uses per-VC credits.
      const uint32_t perVcCredits = credits / std::max (static_cast<uint8_t> (1), m_numVcs);
      for (uint8_t vc = 0; vc < m_numVcs; vc++)
        {
          m_txCreditsMap[peerMac][vc] = perVcCredits;
        }
      NS_LOG_INFO ("Added peer " << peerMac
                   << (isLinkPeer ? " (link, EXCLUSIVE)" : " (switch-internal)")
                   << " with " << perVcCredits << " credits per VC");
    }

  for (uint8_t vc = 0; vc < m_numVcs; vc++)
    {
      m_rxCreditsToReturnMap[peerMac][vc] = 0;
    }
}

void
CbfcManager::InitializePeerDeviceCredits (std::function<Mac48Address()> getRemoteMac,
                                         std::function<bool()> isSwitchDevice,
                                         uint32_t switchCredits,
                                         uint32_t creditWindowPacketBytes)
{
  NS_LOG_FUNCTION (this);

  if (!m_initialized)
    {
      InitializeBasic ();
    }

  // Add peer device with initial credits
  Mac48Address peerMac = getRemoteMac ();
  // Add peer device with initial credits.
  // Link credit mode (SHARED / EXCLUSIVE) determines allocation strategy:
  //   SHARED    — single credit pool (total = m_initialCredits) shared by all VCs
  //   EXCLUSIVE — each VC gets m_initialCredits / m_numVcs independently
  AddPeerDevice (peerMac, 0 /*use m_initialCredits*/);

  if (m_enableLinkCBFC && creditWindowPacketBytes > 0)
    {
      // Ensure the link peer window is large enough to send at least one
      // max-sized data packet (otherwise CBFC can deadlock at the first send).
      const uint32_t need = CalculateCreditsForPacket (creditWindowPacketBytes);
      const uint32_t have = m_initialCredits;
      if (have < need)
        {
          NS_ABORT_MSG ("LinkCredits(InitialCredits)=" << have
                                                       << " is too small for creditWindowPacketBytes="
                                                       << creditWindowPacketBytes
                                                       << " (BytesPerCredit=" << m_bytesPerCredit
                                                       << " => needCredits=" << need << "). "
                                                       << "Increase ProcessingQueueMaxKB/LinkCredits, "
                                                       << "or reduce MaxBurstSize/packet size.");
        }
    }

  // If switch device, initialize credit allocation for other devices on the switch.
  //
  // Switch-internal credits are only meaningful when link-level CBFC is enabled.
  // When CBFC is disabled, we must not enforce/abort on SwitchCredits, since no
  // credit checks/consumption will be performed in the forwarding path.
  if (isSwitchDevice () && m_enableLinkCBFC)
    {
      NS_LOG_INFO ("Switch device detected: initializing credits for all peer devices on all ports");

      // Switch device: initialize credits for all peer devices on all ports
      Ptr<Node> node = m_getNode ();
      if (node)
        {
          Mac48Address localMac = m_getLocalMac ();

          // Interpret `switchCredits` as the *total* egress credit window (across all VCs)
          // that an egress port can distribute across ingress senders for that egress.
          //
          // Important nuance (SUE-Sim multi-rail topology):
          // - In the default pipeline, SueClient sends packets from local port `k` to remote port `k`
          //   (lane-preserving). Therefore, each egress port `k` is primarily contended by the set of
          //   ingress ports with the same `portId=k` across other XPUs, i.e., (~nXpus-1) contenders,
          //   not by all switch ports (~nXpus*portsPerXpu-1).
          //
          // To reflect that, when TopologyBuilder has registered per-port metadata (xpuId, portId),
          // we distribute `switchCredits` across *contenders per portId group*:
          //   per_pair_credits(portId=k) = floor(switchCredits / (count(portId=k) - 1))
          //
          // If metadata is unavailable, we fall back to distributing across all peer switch ports.
          std::vector<Mac48Address> peerMacs;
          std::unordered_map<uint32_t, uint32_t> portIdCounts;
          bool havePortIdMeta = false;
          for (uint32_t i = 0; i < node->GetNDevices (); i++)
            {
              Ptr<NetDevice> dev = node->GetDevice (i);
              Ptr<PointToPointSueNetDevice> p2pDev = DynamicCast<PointToPointSueNetDevice> (dev);
              if (!p2pDev)
                {
                  continue;
                }

              Mac48Address mac = Mac48Address::ConvertFrom (p2pDev->GetAddress ());
              bool isSwitch = false;
              uint32_t portId = 0;
              if (PointToPointSueNetDevice::LookupRegisteredDeviceMeta (mac,
                                                                       &isSwitch,
                                                                       nullptr,
                                                                       &portId) &&
                  isSwitch)
                {
                  havePortIdMeta = true;
                  portIdCounts[portId] += 1;
                }

              if (mac != localMac)
                {
                  peerMacs.push_back (mac);
                }
            }

          const uint32_t peerCount = static_cast<uint32_t> (peerMacs.size ());
          if (peerCount > 0)
            {
              for (uint32_t idx = 0; idx < peerCount; ++idx)
                {
                  const Mac48Address peer = peerMacs[idx];

                  uint32_t competitorCount = peerCount; // fallback: all other switch ports
                  if (havePortIdMeta)
                    {
                      bool peerIsSwitch = false;
                      uint32_t peerPortId = 0;
                      if (PointToPointSueNetDevice::LookupRegisteredDeviceMeta (
                              peer, &peerIsSwitch, nullptr, &peerPortId) &&
                          peerIsSwitch)
                        {
                          auto itCnt = portIdCounts.find (peerPortId);
                          if (itCnt != portIdCounts.end () && itCnt->second > 1)
                            {
                              competitorCount = itCnt->second - 1; // exclude egress itself
                            }
                        }
                    }

                  if (competitorCount == 0)
                    {
                      competitorCount = 1;
                    }
                  const uint32_t vcFactor =
                      std::max(1u, static_cast<uint32_t>(m_numVcs));
                  const uint32_t minTotal = competitorCount * vcFactor;
                  if (switchCredits < minTotal)
                    {
                      NS_ABORT_MSG ("SwitchCredits=" << switchCredits
                                                     << " is too small for competitorCount="
                                                     << competitorCount
                                                     << " * numVcs=" << vcFactor
                                                     << " (per portId group). "
                                                     << "Increase VcQueueMaxKB/SwitchCredits, "
                                                     << "or reduce nXpus/contenders or numVcs.");
                    }

                  const uint32_t perPeerTotal = switchCredits / competitorCount;
                  const uint32_t perPeerPerVc = perPeerTotal / vcFactor;
                  NS_ASSERT_MSG (perPeerPerVc > 0, "per-peer switch credits must be > 0");

                  if (m_enableLinkCBFC && creditWindowPacketBytes > 0)
                    {
                      // Ensure each ingress->egress share can carry at least one max-sized packet.
                      // Otherwise, internal CBFC would block forever (no packet can be admitted).
                      const uint32_t need = CalculateCreditsForPacket (creditWindowPacketBytes);
                      if (perPeerPerVc < need)
                        {
                          NS_ABORT_MSG ("SwitchCredits=" << switchCredits
                                                         << " competitorCount=" << competitorCount
                                                         << " numVcs=" << vcFactor
                                                         << " => perPeerPerVc=" << perPeerPerVc
                                                         << " is too small for creditWindowPacketBytes="
                                                         << creditWindowPacketBytes
                                                         << " (BytesPerCredit=" << m_bytesPerCredit
                                                         << " => needCredits=" << need << "). "
                                                         << "Suggested fixes: increase VcQueueMaxKB/SwitchCredits "
                                                         << "(e.g., 32KB => 1024 credits at 32B/credit), "
                                                         << "reduce contenders (nXpus), reduce MaxBurstSize, "
                                                         << "reduce numVcs, or increase BytesPerCredit granularity.");
                        }
                    }

                  // AddPeerDevice() expects a TOTAL credit budget for switch-internal peers and
                  // performs the per-VC split itself. Passing the already-divided per-VC share
                  // here would divide by numVcs twice and under-provision internal CBFC windows.
                  AddPeerDevice (peer, perPeerTotal);
                  NS_LOG_INFO ("Switch: Added peer device " << peer
                                                           << " with perPeerTotal=" << perPeerTotal
                                                           << " perPeerPerVc=" << perPeerPerVc
                                                           << " switchCredits=" << switchCredits
                                                           << " competitorCount=" << competitorCount
                                                           << " numVcs=" << vcFactor);
                }
            }
        }
      else
        {
          NS_LOG_WARN ("Switch device: Cannot access node for peer device initialization");
        }
    }

  NS_LOG_INFO ("Credit initialization completed for peer device " << peerMac
               << " (switch: " << (isSwitchDevice () ? "yes" : "no") << ")");
}

uint32_t
CbfcManager::GetTxCredits (Mac48Address mac, uint8_t vcId) const
{
  NS_LOG_FUNCTION (this << mac << static_cast<uint32_t> (vcId));

  // Shared mode: link peer uses shared pool
  if (m_linkCreditMode == LinkCreditMode::SHARED && mac == m_linkPeerMac)
    {
      auto it = m_sharedTxCreditsMap.find (mac);
      return (it != m_sharedTxCreditsMap.end ()) ? it->second : 0;
    }

  auto it = m_txCreditsMap.find (mac);
  if (it != m_txCreditsMap.end ())
    {
      auto vcIt = it->second.find (vcId);
      if (vcIt != it->second.end ())
        {
          return vcIt->second;
        }
    }
  return 0;
}

bool
CbfcManager::DecrementTxCredits (Mac48Address mac, uint8_t vcId)
{
  NS_LOG_FUNCTION (this << mac << static_cast<uint32_t> (vcId));

  // Shared mode: link peer uses shared pool
  if (m_linkCreditMode == LinkCreditMode::SHARED && mac == m_linkPeerMac)
    {
      auto it = m_sharedTxCreditsMap.find (mac);
      if (it != m_sharedTxCreditsMap.end () && it->second > 0)
        {
          it->second--;
          return true;
        }
      return false;
    }

  auto it = m_txCreditsMap.find (mac);
  if (it != m_txCreditsMap.end ())
    {
      auto vcIt = it->second.find (vcId);
      if (vcIt != it->second.end () && vcIt->second > 0)
        {
          vcIt->second--;
          return true;
        }
    }
  return false;
}

bool
CbfcManager::DecrementTxCredits (Mac48Address mac, uint8_t vcId, uint32_t credits)
{
  NS_LOG_FUNCTION (this << mac << static_cast<uint32_t> (vcId) << credits);

  // Shared mode: link peer uses shared pool
  if (m_linkCreditMode == LinkCreditMode::SHARED && mac == m_linkPeerMac)
    {
      auto it = m_sharedTxCreditsMap.find (mac);
      if (it != m_sharedTxCreditsMap.end () && it->second >= credits)
        {
          it->second -= credits;
          return true;
        }
      return false;
    }

  auto it = m_txCreditsMap.find (mac);
  if (it != m_txCreditsMap.end ())
    {
      auto vcIt = it->second.find (vcId);
      if (vcIt != it->second.end () && vcIt->second >= credits)
        {
          vcIt->second -= credits;
          return true;
        }
    }
  return false;
}

void
CbfcManager::AddTxCredits (Mac48Address mac, uint8_t vcId, uint32_t credits)
{
  NS_LOG_FUNCTION (this << mac << static_cast<uint32_t> (vcId) << credits);

  if (credits > 0)
    {
      // Shared mode: link peer returns go to the shared pool
      if (m_linkCreditMode == LinkCreditMode::SHARED && mac == m_linkPeerMac)
        {
          m_sharedTxCreditsMap[mac] += credits;
          NS_LOG_INFO ("Added " << credits << " credits to SHARED pool for " << mac
                       << " (VC " << static_cast<uint32_t> (vcId) << " triggered)"
                       << ". Total now: " << m_sharedTxCreditsMap[mac]);
        }
      else
        {
          m_txCreditsMap[mac][vcId] += credits;
          NS_LOG_INFO ("Added " << credits << " credits for " << mac
                       << " VC " << static_cast<uint32_t> (vcId)
                       << ". Total now: " << m_txCreditsMap[mac][vcId]);
        }
    }
}

void
CbfcManager::SetTxCredits (Mac48Address mac, uint8_t vcId, uint32_t credits)
{
  NS_LOG_FUNCTION (this << mac << static_cast<uint32_t> (vcId) << credits);

  if (!m_initialized)
    {
      InitializeBasic ();
    }

  // Shared mode: link peer sync overwrites the shared pool
  if (m_linkCreditMode == LinkCreditMode::SHARED && mac == m_linkPeerMac)
    {
      m_sharedTxCreditsMap[mac] = credits;
      NS_LOG_INFO ("Set SHARED pool for " << mac << " to " << credits
                   << " (VC " << static_cast<uint32_t> (vcId) << " triggered)");
    }
  else
    {
      m_txCreditsMap[mac][vcId] = credits;
      NS_LOG_INFO ("Set credits for " << mac << " VC " << static_cast<uint32_t> (vcId)
                                      << " to " << credits);
    }
}

void
CbfcManager::HandleCreditReturn (const EthernetHeader& ethHeader, uint8_t vcId, uint32_t packetSize)
{
  NS_LOG_FUNCTION (this << ethHeader.GetSource () << static_cast<uint32_t> (vcId) << packetSize);

  if (m_enableLinkCBFC)
    {
      // Calculate credits to return based on packet size (same as consumption logic)
      uint32_t creditsToReturn = CalculateCreditsForPacket (packetSize);

      // Increase credit count for corresponding source address and VC
      Mac48Address source = ethHeader.GetSource ();

      m_rxCreditsToReturnMap[source][vcId] += creditsToReturn;

      NS_LOG_DEBUG ("Added " << creditsToReturn << " credits to return for " << source
                   << " VC " << static_cast<uint32_t> (vcId)
                   << " (packet size: " << packetSize << " bytes)");
    }
}

uint32_t
CbfcManager::GetCreditsToReturn (Mac48Address peerMac, uint8_t vcId) const
{
  NS_LOG_FUNCTION (this << peerMac << static_cast<uint32_t> (vcId));

  auto macIt = m_rxCreditsToReturnMap.find (peerMac);
  if (macIt != m_rxCreditsToReturnMap.end ())
    {
      auto vcIt = macIt->second.find (vcId);
      if (vcIt != macIt->second.end ())
        {
          return vcIt->second;
        }
    }
  return 0;
}

uint32_t
CbfcManager::ClearCreditsToReturn (Mac48Address peerMac, uint8_t vcId)
{
  NS_LOG_FUNCTION (this << peerMac << static_cast<uint32_t> (vcId));

  auto macIt = m_rxCreditsToReturnMap.find (peerMac);
  if (macIt != m_rxCreditsToReturnMap.end ())
    {
      auto vcIt = macIt->second.find (vcId);
      if (vcIt != macIt->second.end ())
        {
          uint32_t credits = vcIt->second;
          vcIt->second = 0;
          return credits;
        }
    }
  return 0;
}

bool
CbfcManager::IsEnabled (void) const
{
  return m_enableLinkCBFC;
}

bool
CbfcManager::IsInitialized (void) const
{
  return m_initialized;
}

uint8_t
CbfcManager::GetNumVcs (void) const
{
  return m_numVcs;
}

uint32_t
CbfcManager::GetInitialCredits (void) const
{
  return m_initialCredits;
}

uint32_t
CbfcManager::GetCreditBatchSize (void) const
{
  return m_creditBatchSize;
}

const std::map<Mac48Address, std::map<uint8_t, uint32_t>>&
CbfcManager::GetTxCreditsMap (void) const
{
  return m_txCreditsMap;
}

bool
CbfcManager::IsLinkCbfcEnabled (void) const
{
  return m_enableLinkCBFC;
}

LinkCreditMode
CbfcManager::GetLinkCreditMode (void) const
{
  return m_linkCreditMode;
}

void
CbfcManager::SetLinkCreditMode (LinkCreditMode mode)
{
  m_linkCreditMode = mode;
  m_linkCreditModeRaw = static_cast<uint8_t> (mode);
}

void
CbfcManager::SetCallbacks (GetLocalMacCallback getLocalMac,
                          GetNodeCallback getNode,
                          SendPacketCallback sendPacket,
                          Time creditGenerateDelay,
                          uint16_t protocolNum)
{
  NS_LOG_FUNCTION (this);

  m_getLocalMac = getLocalMac;
  m_getNode = getNode;
  m_sendPacket = sendPacket;
  m_creditGenerateDelay = creditGenerateDelay;
  m_protocolNum = protocolNum;
  m_callbacksSet = true;
}

void
CbfcManager::CreditReturn (Mac48Address targetMac, uint8_t vcId)
{
  NS_LOG_FUNCTION (this << targetMac << static_cast<uint32_t>(vcId));

  if (!m_enableLinkCBFC || !m_callbacksSet)
  {
    NS_LOG_LOGIC ("CBFC not enabled or callbacks not set");
    return;
  }

  uint32_t creditsToSend = GetCreditsToReturn (targetMac, vcId);

  // Check if batch sending conditions are met
  if (creditsToSend < m_creditBatchSize)
  {
    NS_LOG_LOGIC ("Credits for VC " << static_cast<uint32_t>(vcId)
                                   << " are less than batch size (" << m_creditBatchSize << ")");
    return;
  }

  // Create credit packet
  EthernetHeader ethHeader;
  ethHeader.SetSource (m_getLocalMac ());
  ethHeader.SetDestination (targetMac);
  ethHeader.SetLengthType (0x0800);

  SueCbfcHeader creditHeader;
  creditHeader.SetVcId (vcId);
  creditHeader.SetCredits (creditsToSend);
  Ptr<Packet> creditPacket = Create<Packet> ();

  creditPacket->AddHeader (ethHeader);
  creditPacket->AddHeader (creditHeader);

  NS_LOG_INFO ("Node " << m_getNode ()->GetId () << " sending "
                      << creditsToSend << " credits to " << targetMac
                      << " for VC " << static_cast<uint32_t>(vcId));

  // Schedule the packet sending using the callback
  Simulator::Schedule (m_creditGenerateDelay, &CbfcManager::SendCreditPacket,
                      this, creditPacket, targetMac, m_protocolNum);
}

void
CbfcManager::SendCreditPacket (Ptr<Packet> packet, Mac48Address targetMac, uint16_t protocolNum)
{
  NS_LOG_FUNCTION (this << packet << targetMac << protocolNum);

  if (m_callbacksSet && m_sendPacket)
  {
    m_sendPacket (packet, targetMac, protocolNum);
  }
  else
  {
    NS_LOG_WARN ("Send packet callback not set, credit packet dropped");
  }
}

// Dynamic credit consumption methods

void
CbfcManager::SetDynamicCreditMode (bool enable, uint32_t baseCredit, uint32_t transactionSize, uint32_t headerSize)
{
  NS_LOG_FUNCTION (this << enable << baseCredit << transactionSize << headerSize);

  m_enableDynamicCredits = enable;
  m_baseCredit = baseCredit;
  m_transactionSize = transactionSize;
  m_headerSize = headerSize;

  NS_LOG_INFO ("Dynamic credit mode " << (enable ? "enabled" : "disabled")
               << ", base credit: " << baseCredit
               << ", transaction size: " << transactionSize
               << " bytes, header size: " << headerSize << " bytes");
}

void
CbfcManager::SetAdvancedCreditCalculation (uint32_t bytesPerCredit)
{
  NS_LOG_FUNCTION (this << bytesPerCredit);

  m_bytesPerCredit = bytesPerCredit;

  NS_LOG_INFO ("Credit calculation enabled"
               << ", bytes per credit: " << bytesPerCredit << " bytes");
}

uint32_t
CbfcManager::CalculateCreditsForPacket (uint32_t packetSize) const
{
  NS_LOG_FUNCTION (this << packetSize);

  if (!m_enableDynamicCredits)
    {
      return m_baseCredit;
    }

  // Simple linear mapping: packet bytes / bytes per credit, round up
  uint32_t credits = (packetSize + m_bytesPerCredit - 1) / m_bytesPerCredit;

  // Ensure at least minimum credits
  uint32_t totalCredits = std::max (credits, m_baseCredit);

  NS_LOG_DEBUG ("Packet size " << packetSize << " bytes, bytes per credit: " << m_bytesPerCredit
                << ", requires " << totalCredits << " credits");

  return totalCredits;
}

bool
CbfcManager::HasEnoughCredits (Mac48Address mac, uint8_t vcId, uint32_t packetSize) const
{
  NS_LOG_FUNCTION (this << mac << static_cast<uint32_t> (vcId) << packetSize);

  uint32_t creditsNeeded = CalculateCreditsForPacket (packetSize);

  // Shared mode: link peer checks shared pool
  if (m_linkCreditMode == LinkCreditMode::SHARED && mac == m_linkPeerMac)
    {
      auto it = m_sharedTxCreditsMap.find (mac);
      return (it != m_sharedTxCreditsMap.end () && it->second >= creditsNeeded);
    }

  auto it = m_txCreditsMap.find (mac);
  if (it != m_txCreditsMap.end ())
    {
      auto vcIt = it->second.find (vcId);
      if (vcIt != it->second.end ())
        {
          return vcIt->second >= creditsNeeded;
        }
    }

  return false;
}

bool
CbfcManager::ConsumeDynamicCredits (Mac48Address mac, uint8_t vcId, uint32_t packetSize)
{
  NS_LOG_FUNCTION (this << mac << static_cast<uint32_t> (vcId) << packetSize);

  uint32_t creditsNeeded = CalculateCreditsForPacket (packetSize);

  if (DecrementTxCredits (mac, vcId, creditsNeeded))
    {
      NS_LOG_INFO ("Consumed " << creditsNeeded << " credits for packet size "
                   << packetSize << " bytes to " << mac << " VC " << static_cast<uint32_t> (vcId));
      return true;
    }
  else
    {
      NS_LOG_INFO ("Failed to consume " << creditsNeeded << " credits for packet size "
                   << packetSize << " bytes to " << mac << " VC " << static_cast<uint32_t> (vcId)
                   << " - insufficient credits");
      return false;
    }
}

} // namespace ns3
