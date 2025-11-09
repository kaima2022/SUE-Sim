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
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/queue-size.h"
#include "ns3/mac48-address.h"
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
                                        MakeUintegerChecker<uint32_t> ());
  return tid;
}

CbfcManager::CbfcManager ()
  : m_initialized (false),
    m_enableLinkCBFC (false),
    m_initialCredits (20),
    m_numVcs (4),
    m_creditBatchSize (1),
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

  // Clear existing data structures
  m_txCreditsMap.clear ();
  m_rxCreditsToReturnMap.clear ();

  m_initialized = true;
  NS_LOG_INFO ("CbfcManager initialized with " << static_cast<uint32_t> (m_numVcs)
               << " VCs and " << m_initialCredits << " initial credits.");
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
                        uint32_t switchCredits)
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
  InitializePeerDeviceCredits (getRemoteMac, isSwitchDevice, switchCredits);

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

  for (uint8_t vc = 0; vc < m_numVcs; vc++)
    {
      m_txCreditsMap[peerMac][vc] = credits;
      m_rxCreditsToReturnMap[peerMac][vc] = 0;
    }

  NS_LOG_INFO ("Added peer device " << peerMac << " with " << credits << " initial credits per VC");
}

void
CbfcManager::InitializePeerDeviceCredits (std::function<Mac48Address()> getRemoteMac,
                                         std::function<bool()> isSwitchDevice,
                                         uint32_t switchCredits)
{
  NS_LOG_FUNCTION (this);

  if (!m_initialized)
    {
      InitializeBasic ();
    }

  // Add peer device with initial credits
  Mac48Address peerMac = getRemoteMac ();
  AddPeerDevice (peerMac, m_initialCredits);

  // If switch device, initialize credit allocation for other devices on the switch
  if (isSwitchDevice ())
    {
      NS_LOG_INFO ("Switch device detected: initializing credits for all peer devices on all ports");

      // Switch device: initialize credits for all peer devices on all ports
      Ptr<Node> node = m_getNode ();
      if (node)
        {
          Mac48Address localMac = m_getLocalMac ();
          for (uint32_t i = 0; i < node->GetNDevices (); i++)
            {
              Ptr<NetDevice> dev = node->GetDevice (i);
              // Skip if device is null or is this device (compare by MAC address)
              if (!dev)
                {
                  continue;
                }

              Mac48Address mac = Mac48Address::ConvertFrom (dev->GetAddress ());
              if (mac == localMac)
                {
                  continue; // Skip this device
                }

              // Add peer device with switch default credits
              AddPeerDevice (mac, switchCredits);

              NS_LOG_INFO ("Switch: Added peer device " << mac << " with " << switchCredits << " switch default credits");
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

void
CbfcManager::AddTxCredits (Mac48Address mac, uint8_t vcId, uint32_t credits)
{
  NS_LOG_FUNCTION (this << mac << static_cast<uint32_t> (vcId) << credits);

  if (credits > 0)
    {
      m_txCreditsMap[mac][vcId] += credits;
      NS_LOG_INFO ("Added " << credits << " credits for " << mac
                   << " VC " << static_cast<uint32_t> (vcId)
                   << ". Total now: " << m_txCreditsMap[mac][vcId]);
    }
}

void
CbfcManager::HandleCreditReturn (const EthernetHeader& ethHeader, uint8_t vcId)
{
  NS_LOG_FUNCTION (this << ethHeader.GetSource () << static_cast<uint32_t> (vcId));

  if (m_enableLinkCBFC)
    {
      // Increase credit count for corresponding source address and VC
      Mac48Address source = ethHeader.GetSource ();

      m_rxCreditsToReturnMap[source][vcId]++;
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

  // Clear credits from manager after successful transmission
  ClearCreditsToReturn (targetMac, vcId);
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

} // namespace ns3