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

bool
CbfcManager::DecrementTxCredits (Mac48Address mac, uint8_t vcId, uint32_t credits)
{
  NS_LOG_FUNCTION (this << mac << static_cast<uint32_t> (vcId) << credits);

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
      m_txCreditsMap[mac][vcId] += credits;
      NS_LOG_INFO ("Added " << credits << " credits for " << mac
                   << " VC " << static_cast<uint32_t> (vcId)
                   << ". Total now: " << m_txCreditsMap[mac][vcId]);
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