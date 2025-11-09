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

#include "sue-llr.h"
#include "sue-ppp-header.h"
#include "sue-tag.h"
#include "sue-cbfc-header.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/mac48-address.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include <sstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("LlrNodeManager");

// LlrNodeManager Implementation
NS_OBJECT_ENSURE_REGISTERED(LlrNodeManager);

TypeId
LlrNodeManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LlrNodeManager")
                          .SetParent<Object> ()
                          .SetGroupName ("PointToPointSue")
                          .AddConstructor<LlrNodeManager> ()
                          .AddAttribute ("LlrEnabled", "Whether LLR is enabled.",
                                        BooleanValue (false),
                                        MakeBooleanAccessor (&LlrNodeManager::m_llrEnabled),
                                        MakeBooleanChecker ())
                          .AddAttribute ("LlrWindowSize", "The LLR window size.",
                                        UintegerValue (10),
                                        MakeUintegerAccessor (&LlrNodeManager::m_llrWindowSize),
                                        MakeUintegerChecker<uint32_t> ())
                          .AddAttribute ("LlrTimeout", "The LLR timeout value.",
                                        TimeValue (NanoSeconds (10000)),
                                        MakeTimeAccessor (&LlrNodeManager::m_llrTimeout),
                                        MakeTimeChecker ());
  return tid;
}

LlrNodeManager::LlrNodeManager ()
  : m_llrEnabled (false),
    m_llrWindowSize (10),
    m_llrTimeout (NanoSeconds (10000)),
    m_AckAddHeaderDelay (Seconds (0.0)),
    m_AckProcessDelay (Seconds (0.0)),
    m_numVcs (4),
    m_protocolNum (0),
    m_llrResendseq (0)
{
  NS_LOG_FUNCTION (this);
}

LlrNodeManager::~LlrNodeManager ()
{
  NS_LOG_FUNCTION (this);
}

void
LlrNodeManager::Initialize (bool llrEnabled,
                           uint32_t llrWindowSize,
                           Time llrTimeout,
                           Time ackAddHeaderDelay,
                           Time ackProcessDelay,
                           uint16_t protocolNum,
                           uint8_t numVcs,
                           GetLocalMacCallback getLocalMac,
                           GetNodeCallback getNode,
                           GetRemoteMacCallback getRemoteMac,
                           SendPacketCallback sendPacket,
                           TryTransmitCallback tryTransmit)
{
  NS_LOG_FUNCTION (this << llrEnabled << llrWindowSize << llrTimeout << numVcs);

  m_llrEnabled = llrEnabled;
  m_llrWindowSize = llrWindowSize;
  m_llrTimeout = llrTimeout;
  m_AckAddHeaderDelay = ackAddHeaderDelay;
  m_AckProcessDelay = ackProcessDelay;
  m_protocolNum = protocolNum;
  m_numVcs = numVcs;

  // Set callbacks
  m_getLocalMac = getLocalMac;
  m_getNode = getNode;
  m_getRemoteMac = getRemoteMac;
  m_sendPacket = sendPacket;
  m_tryTransmit = tryTransmit;

  // Get and store remote MAC address
  if (m_getRemoteMac)
    {
      m_switchMac = m_getRemoteMac ();
      NS_LOG_DEBUG ("Remote MAC address: " << m_switchMac);
    }

  // Initialize LLR structures
  m_sendList.resize (m_numVcs);
  m_waitSeq.resize (m_numVcs, 0);
  m_sendSeq.resize (m_numVcs, 0);
  m_unack.resize (m_numVcs, 0);
  m_llrWait.resize (m_numVcs, false);
  m_llrResending.resize (m_numVcs, false);
  m_lastAckedTime.resize (m_numVcs);
  m_lastAcksend.resize (m_numVcs);
  m_resendPkt.resize (m_numVcs);

  // Clear all send lists
  for (uint8_t i = 0; i < m_numVcs; i++)
    {
      m_sendList[i].clear ();
    }

  NS_LOG_INFO ("LLR Node Manager initialized: enabled=" << m_llrEnabled
               << ", windowSize=" << m_llrWindowSize
               << ", timeout=" << m_llrTimeout
               << ", numVcs=" << static_cast<uint32_t> (m_numVcs));
}

uint32_t
LlrNodeManager::LlrSendPacket (Ptr<Packet> packet, uint8_t vcId)
{
  NS_LOG_FUNCTION (this << packet << static_cast<uint32_t> (vcId));

  if (!m_llrEnabled)
    {
      // Non-LLR mode: add PPP header and tag for delay measurement
      SuePppHeader ppp;
      ppp.SetProtocol (SuePacketUtils::EtherToPpp (0x0800));
      packet->AddHeader (ppp);

      // For NIC (first hop), add tag with seq=0 for delay measurement
      SueTag tag (Simulator::Now (), 0); // Default sequence for non-LLR
      tag.SetLinkType (0); // 0 = NIC (first hop)
      packet->AddPacketTag (tag);
      NS_LOG_DEBUG ("Non-LLR mode: added PPP header and tag with seq=0 for delay measurement");
      return 0;
    }

  if (vcId >= m_numVcs)
    {
      NS_LOG_WARN ("Invalid VC ID: " << static_cast<uint32_t> (vcId));
      return 0;
    }

  // Get next sequence number
  uint32_t sequenceNumber = m_sendSeq[vcId];
  m_sendSeq[vcId]++;

  // Store packet for potential retransmission
  m_sendList[vcId][sequenceNumber] = packet->Copy ();
  m_unack[vcId]++;

  // Add PPP Header (protocol number 0x0800 for IP)
  SuePppHeader ppp;
  ppp.SetProtocol (SuePacketUtils::EtherToPpp (0x0800));
  packet->AddHeader (ppp);

  // For NIC (first hop: NIC -> Switch), add tag with sequence number and link type
  SueTag tag (Simulator::Now (), sequenceNumber);
  tag.SetLinkType (0); // 0 = NIC (first hop)
  packet->AddPacketTag (tag);

  NS_LOG_DEBUG ("NIC sending data packet with seq " << sequenceNumber
               << " on VC " << static_cast<uint32_t> (vcId)
               << " to switch " << m_switchMac << " (first hop)");

  // Start retransmission timer for this VC if not already running
  if (!m_resendPkt[vcId].IsPending ())
    {
      m_resendPkt[vcId] = Simulator::Schedule (m_llrTimeout,
                                               &LlrNodeManager::Resend,
                                               this, vcId);
    }

  return sequenceNumber;
}

void
LlrNodeManager::LlrReceivePacket (Ptr<Packet> packet, uint8_t vcId, Mac48Address source, uint32_t seq_rev)
{
  NS_LOG_FUNCTION (this << packet << static_cast<uint32_t> (vcId) << source << seq_rev);

  if (!m_llrEnabled)
    {
      return; // If LLR is disabled, don't process
    }

  if (vcId >= m_numVcs)
    {
      NS_LOG_WARN ("Invalid VC ID: " << static_cast<uint32_t> (vcId));
      return;
    }

  uint32_t expectedSeq = m_waitSeq[vcId];

  if (seq_rev == expectedSeq)
    {
      // In-order packet, update wait sequence and send ACK
      m_waitSeq[vcId]++;
      SendLlrAck (vcId, seq_rev);

      NS_LOG_DEBUG ("Received in-order packet seq " << seq_rev
                   << " on VC " << static_cast<uint32_t> (vcId)
                   << ", next expected: " << m_waitSeq[vcId]);
    }
  else if (seq_rev > expectedSeq)
    {
      // Out-of-order packet, missing packets detected
      NS_LOG_INFO ("Received out-of-order packet seq " << seq_rev
                  << " on VC " << static_cast<uint32_t> (vcId)
                  << ", expected " << expectedSeq);

      // Send NACK for the missing sequence
      SendLlrNack (vcId, expectedSeq);
    }
  else
    {
      // Duplicate packet, just send ACK and discard
      NS_LOG_DEBUG ("Received duplicate packet seq " << seq_rev
                   << " on VC " << static_cast<uint32_t> (vcId));

      SendLlrAck (vcId, seq_rev);
    }
}

void
LlrNodeManager::SendLlrAck (uint8_t vcId, uint32_t seq)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (vcId) << seq);

  if (!m_llrEnabled)
    {
      return;
    }

  // Create ACK packet (exactly like NetDevice SendLlrAck)
  Ptr<Packet> ackPacket = Create<Packet> ();

  // Add Ethernet header, source MAC is local port, destination MAC is switch port
  EthernetHeader ethHeader;
  if (m_getLocalMac)
    {
      ethHeader.SetSource (m_getLocalMac ());
    }
  ethHeader.SetDestination (m_switchMac);
  ethHeader.SetLengthType (0x0800);
  ackPacket->AddHeader (ethHeader);

  // Add CBFC header to carry VC ID
  SueCbfcHeader ackHeader;
  ackHeader.SetVcId (vcId);
  ackHeader.SetCredits (0); // ACK packets have 0 credits
  ackPacket->AddHeader (ackHeader);

  // PPP Header
  SuePppHeader ppp;
  ppp.SetProtocol (SuePacketUtils::EtherToPpp (SuePacketUtils::ACK_REV));
  ackPacket->AddHeader (ppp);

  // Add SueTag to carry sequence number
  SueTag ackTag (Simulator::Now (), seq);
  ackPacket->AddPacketTag (ackTag);

  NS_LOG_DEBUG ("Sending ACK for seq " << seq
               << " to switch " << m_switchMac
               << " on VC " << static_cast<uint32_t> (vcId));

  // Schedule ACK transmission with delay
  Simulator::Schedule (m_AckAddHeaderDelay,
                      &LlrNodeManager::SendControlPacket,
                      this, ackPacket, SuePacketUtils::ACK_REV);
}

void
LlrNodeManager::SendLlrNack (uint8_t vcId, uint32_t seq)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (vcId) << seq);

  if (!m_llrEnabled)
    {
      return;
    }

  // Create NACK packet (exactly like NetDevice SendLlrNack)
  Ptr<Packet> nackPacket = Create<Packet> ();

  // Add Ethernet header, source MAC is local port, destination MAC is switch port
  EthernetHeader ethHeader;
  if (m_getLocalMac)
    {
      ethHeader.SetSource (m_getLocalMac ());
    }
  ethHeader.SetDestination (m_switchMac);
  ethHeader.SetLengthType (0x0800);
  nackPacket->AddHeader (ethHeader);

  // Add CBFC header to carry VC ID
  SueCbfcHeader nackHeader;
  nackHeader.SetVcId (vcId);
  nackHeader.SetCredits (0); // NACK packets have 0 credits
  nackPacket->AddHeader (nackHeader);

  // PPP Header
  SuePppHeader ppp;
  ppp.SetProtocol (SuePacketUtils::EtherToPpp (SuePacketUtils::NACK_REV));
  nackPacket->AddHeader (ppp);

  // Add SueTag to carry sequence number
  SueTag nackTag (Simulator::Now (), seq);
  nackPacket->AddPacketTag (nackTag);

  NS_LOG_DEBUG ("Sending NACK for seq " << seq
               << " to switch " << m_switchMac
               << " on VC " << static_cast<uint32_t> (vcId));

  // Schedule NACK transmission with delay
  Simulator::Schedule (m_AckAddHeaderDelay,
                      &LlrNodeManager::SendControlPacket,
                      this, nackPacket, SuePacketUtils::NACK_REV);
}

void
LlrNodeManager::ProcessLlrAck (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);

  if (!m_llrEnabled)
    {
      return;
    }

  // Extract sequence number from tag
  SueTag tag;
  bool found = p->PeekPacketTag (tag);
  if (!found)
    {
      NS_LOG_WARN ("ProcessLlrAck: no tag found, cannot process ACK");
      return;
    }

  uint32_t seq = tag.GetSequence ();

  SuePppHeader ppp;
  p->RemoveHeader (ppp);
  SueCbfcHeader ackHeader;
  p->RemoveHeader (ackHeader);
  uint8_t vcId = ackHeader.GetVcId ();

  if (vcId >= m_numVcs)
    {
      NS_LOG_WARN ("ProcessLlrAck: invalid VC ID " << static_cast<uint32_t> (vcId));
      return;
    }

  NS_LOG_DEBUG ("ProcessLlrAck: read seq " << seq << " for VC " << static_cast<uint32_t> (vcId));

  // Start processing ACK sequence number (exactly like NetDevice)
  auto it1 = m_sendList[vcId].begin ();
  auto it2 = m_sendList[vcId].find (seq);
  if (seq < m_waitSeq[vcId])
    { // Received duplicate or old ACK
      NS_LOG_INFO ("Duplicate or old ACK received for VC " << static_cast<uint32_t> (vcId) << " seq " << seq << ", expected " << m_waitSeq[vcId]);
      return;
    }
  else if (it2 == m_sendList[vcId].end ())
    { // Cannot find corresponding seq
      NS_LOG_INFO ("ACK received for VC " << static_cast<uint32_t> (vcId) << " seq " << seq << " which is not in send list, possible duplicate ACK or out-of-order ACK.");
      return;
    }
  else
    {
      // Found corresponding seq, delete this and previous packets
      m_sendList[vcId].erase (it1, it2);
      m_sendList[vcId].erase (it2);
      m_waitSeq[vcId] = seq + 1;
      NS_LOG_INFO ("Updated waitSeq for VC " << static_cast<uint32_t> (vcId) << " to " << m_waitSeq[vcId]);
      m_llrResending[vcId] = false; // Stop retransmission
      m_lastAckedTime[vcId] = Simulator::Now ();
      // Set retransmission timer
      if (m_resendPkt[vcId].IsPending ())
        {
          m_resendPkt[vcId].Cancel ();
        }
    }
}

void
LlrNodeManager::ProcessLlrNack (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);

  if (!m_llrEnabled)
    {
      return;
    }

  // Extract sequence number from tag
  SueTag tag;
  bool found = p->PeekPacketTag (tag);
  if (!found)
    {
      NS_LOG_WARN ("ProcessLlrNack: no tag found, cannot process NACK");
      return;
    }

  uint32_t seq = tag.GetSequence ();

  SuePppHeader ppp;
  p->RemoveHeader (ppp);
  SueCbfcHeader nackHeader;
  p->RemoveHeader (nackHeader);
  uint8_t vcId = nackHeader.GetVcId ();

  if (vcId >= m_numVcs)
    {
      NS_LOG_WARN ("ProcessLlrNack: invalid VC ID " << static_cast<uint32_t> (vcId));
      return;
    }

  NS_LOG_DEBUG ("ProcessLlrNack: read seq " << seq << " for VC " << static_cast<uint32_t> (vcId));

  // Start processing NACK sequence number (exactly like NetDevice)
  auto it1 = m_sendList[vcId].begin ();
  auto it2 = m_sendList[vcId].find (seq);
  if (seq < m_waitSeq[vcId])
    {
      NS_LOG_INFO ("Duplicate or old NACK received for VC " << static_cast<uint32_t> (vcId) << " seq " << seq << ", expected " << m_waitSeq[vcId]);
      return;
    }
  else if (it2 == m_sendList[vcId].end ())
    {
      NS_LOG_INFO ("NACK received for VC " << static_cast<uint32_t> (vcId) << " seq " << seq << " which is not in send list, possible duplicate NACK or out-of-order NACK.");
      return;
    }
  else
    {
      // Received NACK for seq, which means all packets before seq have been received, but seq packet is lost, retransmit seq and subsequent packets
      m_sendList[vcId].erase (it1, it2);
      m_waitSeq[vcId] = seq;
      m_llrResendseq = seq;
      m_llrResending[vcId] = true;
      NS_LOG_INFO ("NACK received, will resend from seq " << seq << " for VC " << static_cast<uint32_t> (vcId));
      // Set retransmission timer
      if (m_resendPkt[vcId].IsPending ())
        {
          m_resendPkt[vcId].Cancel ();
        }
      // Schedule retransmission
      m_resendPkt[vcId] = Simulator::Schedule (m_llrTimeout,
                                                &LlrNodeManager::Resend,
                                                this, vcId);
    }
}

void
LlrNodeManager::Resend (uint8_t vcId)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (vcId));

  if (!m_llrEnabled || vcId >= m_numVcs)
    {
      return;
    }

  m_llrResending[vcId] = false;

  if (m_sendList[vcId].empty ())
    {
      NS_LOG_DEBUG ("No packets to retransmit for VC " << static_cast<uint32_t> (vcId));
      return;
    }

  m_llrResending[vcId] = true;
  m_llrResendseq = m_sendList[vcId].begin ()->first;

  NS_LOG_DEBUG ("Starting retransmission for VC " << static_cast<uint32_t> (vcId)
               << " from seq " << m_llrResendseq);

  // Trigger transmission attempt
  if (m_tryTransmit)
    {
      m_tryTransmit ();
    }
}

void
LlrNodeManager::LlrResendPacket (uint8_t vcId)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (vcId));

  if (!m_llrEnabled || vcId >= m_numVcs)
    {
      return;
    }

  if (!m_llrResending[vcId] || m_sendList[vcId].empty ())
    {
      return;
    }

  auto it = m_sendList[vcId].find (m_llrResendseq);
  if (it == m_sendList[vcId].end ())
    {
      NS_LOG_WARN ("Resend sequence not found in send list");
      m_llrResending[vcId] = false;
      return;
    }

  Ptr<Packet> packet = it->second->Copy ();

  // Add sequence tag
  SueTag tag (Simulator::Now (), m_llrResendseq);
  packet->AddPacketTag (tag);

  NS_LOG_DEBUG ("Resending packet seq " << m_llrResendseq
               << " on VC " << static_cast<uint32_t> (vcId));

  // Send packet
  if (m_sendPacket && !m_switchMac.IsGroup ())
    {
      m_sendPacket (packet, m_switchMac, m_protocolNum);
    }

  m_llrResendseq++;
}

bool
LlrNodeManager::GetLlrEnabled (void) const
{
  return m_llrEnabled;
}

bool
LlrNodeManager::IsLlrResending (uint8_t vcId) const
{
  if (vcId >= m_numVcs)
    {
      return false;
    }
  return m_llrResending[vcId];
}

void
LlrNodeManager::SendControlPacket (Ptr<Packet> packet, uint16_t protocolNum)
{
  NS_LOG_FUNCTION (this << packet << protocolNum);

  if (m_sendPacket && !m_switchMac.IsGroup ())
    {
      m_sendPacket (packet, m_switchMac, protocolNum);
    }
  else
    {
      NS_LOG_WARN ("SendControlPacket: callback not set or switch MAC not configured");
    }
}

// LlrSwitchPortManager Implementation
NS_OBJECT_ENSURE_REGISTERED(LlrSwitchPortManager);

TypeId
LlrSwitchPortManager::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::LlrSwitchPortManager")
                          .SetParent<Object> ()
                          .SetGroupName ("PointToPointSue")
                          .AddConstructor<LlrSwitchPortManager> ()
                          .AddAttribute ("LlrEnabled", "Whether LLR is enabled.",
                                        BooleanValue (false),
                                        MakeBooleanAccessor (&LlrSwitchPortManager::m_llrEnabled),
                                        MakeBooleanChecker ())
                          .AddAttribute ("LlrWindowSize", "The LLR window size.",
                                        UintegerValue (10),
                                        MakeUintegerAccessor (&LlrSwitchPortManager::m_llrWindowSize),
                                        MakeUintegerChecker<uint32_t> ())
                          .AddAttribute ("LlrTimeout", "The LLR timeout value.",
                                        TimeValue (NanoSeconds (10000)),
                                        MakeTimeAccessor (&LlrSwitchPortManager::m_llrTimeout),
                                        MakeTimeChecker ());
  return tid;
}

LlrSwitchPortManager::LlrSwitchPortManager ()
  : m_llrEnabled (false),
    m_llrWindowSize (10),
    m_llrTimeout (NanoSeconds (10000)),
    m_AckAddHeaderDelay (Seconds (0.0)),
    m_AckProcessDelay (Seconds (0.0)),
    m_numVcs (4),
    m_protocolNum (0)
{
  NS_LOG_FUNCTION (this);
}

LlrSwitchPortManager::~LlrSwitchPortManager ()
{
  NS_LOG_FUNCTION (this);
}

void
LlrSwitchPortManager::Initialize (bool llrEnabled,
                                 uint32_t llrWindowSize,
                                 Time llrTimeout,
                                 Time ackAddHeaderDelay,
                                 Time ackProcessDelay,
                                 uint16_t protocolNum,
                                 uint8_t numVcs,
                                 GetLocalMacCallback getLocalMac,
                                 GetNodeCallback getNode,
                                 GetSwitchCallback getSwitch,
                                 SendPacketCallback sendPacket,
                                 TryTransmitCallback tryTransmit,
                                 Mac48Address peerMac)
{
  NS_LOG_FUNCTION (this << llrEnabled << llrWindowSize << llrTimeout << numVcs << peerMac);

  m_llrEnabled = llrEnabled;
  m_llrWindowSize = llrWindowSize;
  m_llrTimeout = llrTimeout;
  m_AckAddHeaderDelay = ackAddHeaderDelay;
  m_AckProcessDelay = ackProcessDelay;
  m_protocolNum = protocolNum;
  m_numVcs = numVcs;
  m_peerMac = peerMac;

  // Set callbacks
  m_getLocalMac = getLocalMac;
  m_getNode = getNode;
  m_getSwitch = getSwitch;
  m_sendPacket = sendPacket;
  m_tryTransmit = tryTransmit;

  // Initialize LLR structures for connected peer (direct connection)
  m_sendList[m_peerMac].resize (m_numVcs);
  m_waitSeq[m_peerMac].resize (m_numVcs, 0);
  m_sendSeq[m_peerMac].resize (m_numVcs, 0);
  m_unack[m_peerMac].resize (m_numVcs, 0);
  m_llrResendseq[m_peerMac].resize (m_numVcs, 0);
  m_llrWait[m_peerMac].resize (m_numVcs, false);
  m_llrResending[m_peerMac].resize (m_numVcs, false);
  m_lastAckedTime[m_peerMac].resize (m_numVcs);
  m_lastAcksend[m_peerMac].resize (m_numVcs);
  m_resendPkt[m_peerMac].resize (m_numVcs);

  // Clear all send lists for peer
  for (uint8_t i = 0; i < m_numVcs; i++)
    {
      m_sendList[m_peerMac][i].clear ();
    }

  // If switch device, initialize LLR for all other switch ports
  if (m_getSwitch && m_getSwitch ())
    {
      Ptr<Node> node = m_getNode ();
      if (node)
        {
          for (uint32_t i = 0; i < node->GetNDevices (); i++)
            {
              Ptr<NetDevice> dev = node->GetDevice (i);
              if (dev)
                {
                  Mac48Address mac = Mac48Address::ConvertFrom (dev->GetAddress ());
                  // Skip our own address and already initialized peer
                  if (mac != m_getLocalMac () && mac != m_peerMac)
                    {
                      // Initialize LLR structures for this switch port
                      m_sendList[mac].resize (m_numVcs);
                      m_waitSeq[mac].resize (m_numVcs, 0);
                      m_sendSeq[mac].resize (m_numVcs, 0);
                      m_unack[mac].resize (m_numVcs, 0);
                      m_llrResendseq[mac].resize (m_numVcs, 0);
                      m_llrWait[mac].resize (m_numVcs, false);
                      m_llrResending[mac].resize (m_numVcs, false);
                      m_lastAckedTime[mac].resize (m_numVcs);
                      m_lastAcksend[mac].resize (m_numVcs);
                      m_resendPkt[mac].resize (m_numVcs);

                      // Clear send lists for this MAC
                      for (uint8_t j = 0; j < m_numVcs; j++)
                        {
                          m_sendList[mac][j].clear ();
                        }

                      NS_LOG_DEBUG ("Initialized LLR structures for switch port MAC: " << mac);
                    }
                }
            }
        }
    }

  NS_LOG_INFO ("LLR Switch Port Manager initialized: enabled=" << m_llrEnabled
               << ", windowSize=" << m_llrWindowSize
               << ", timeout=" << m_llrTimeout
               << ", numVcs=" << static_cast<uint32_t> (m_numVcs)
               << ", peerMac=" << peerMac);
}

uint32_t
LlrSwitchPortManager::LlrSendPacket (Ptr<Packet> packet, uint8_t vcId, Mac48Address mac)
{
  NS_LOG_FUNCTION (this << packet << static_cast<uint32_t> (vcId));

  if (!m_llrEnabled)
    {
      // Non-LLR mode: add PPP header
      SuePppHeader ppp;
      ppp.SetProtocol (SuePacketUtils::EtherToPpp (0x0800));
      packet->AddHeader (ppp);

      // For switch ports in non-LLR mode, no tag operations needed
      // (second and third hops don't need tag in non-LLR mode)
      NS_LOG_DEBUG ("Non-LLR mode: added PPP header for switch port");
      return 0;
    }

  if (vcId >= m_numVcs)
    {
      NS_LOG_WARN ("Invalid VC ID: " << static_cast<uint32_t> (vcId));
      return 0;
    }

  // Get next sequence number
  uint32_t sequenceNumber = m_sendSeq[mac][vcId];
  m_sendSeq[mac][vcId]++;

  // Store packet for potential retransmission
  m_sendList[mac][vcId][sequenceNumber] = packet->Copy ();
  m_unack[mac][vcId]++;

  // Add PPP Header (protocol number 0x0800 for IP)
  SuePppHeader ppp;
  ppp.SetProtocol (SuePacketUtils::EtherToPpp (0x0800));
  packet->AddHeader (ppp);

  // Handle tag based on whether this is internal forwarding or egress
  SueTag tag;
  bool hasExistingTag = packet->PeekPacketTag (tag);

  if (hasExistingTag) {
    // Switch internal forwarding or egress: update existing tag
    uint8_t newLinkType;

    // Check if peer is switch or NIC to determine link type
    bool peerIsSwitch = m_getSwitch && m_getSwitch () && m_getSwitch ()->IsSwitchDevice (mac);

    if (peerIsSwitch) {
      // Second hop: Switch Internal forwarding
      newLinkType = 1; // 1 = Switch Ingress
      NS_LOG_DEBUG ("Switch internal forwarding: updating tag for VC " << static_cast<uint32_t> (vcId));
    } else {
      // Third hop: Switch Egress -> NIC
      newLinkType = 2; // 2 = Switch Egress
      NS_LOG_DEBUG ("Switch egress: updating tag for VC " << static_cast<uint32_t> (vcId));
    }

    // Update sequence and link type in the packet's tag
    SueTag::UpdateSequenceAndLinkTypeInPacket (packet, sequenceNumber, newLinkType);
  } else {
    // This should not happen in normal operation, but handle gracefully
    NS_LOG_WARN ("Switch port sending packet without existing tag, adding new tag");
    SueTag newTag (Simulator::Now (), sequenceNumber);
    newTag.SetLinkType (1); // Default to switch ingress
    packet->AddPacketTag (newTag);
  }

  NS_LOG_DEBUG ("Switch port sending data packet with seq " << sequenceNumber
               << " on VC " << static_cast<uint32_t> (vcId)
               << " to peer " << mac);

  // Start retransmission timer for this VC if not already running
  if (!m_resendPkt[mac][vcId].IsPending ())
    {
      m_resendPkt[mac][vcId] = Simulator::Schedule (m_llrTimeout,
                                               &LlrSwitchPortManager::Resend,
                                               this, vcId, mac);
    }

  return sequenceNumber;
}

bool
LlrSwitchPortManager::LlrReceivePacket (Ptr<Packet> packet, uint8_t vcId, uint32_t seq_rev, Mac48Address source)
{
  NS_LOG_FUNCTION (this << packet << static_cast<uint32_t> (vcId) << seq_rev);

  if (!m_llrEnabled)
    {
      return true; // If LLR is disabled, process all packets
    }

  if (vcId >= m_numVcs)
    {
      NS_LOG_WARN ("Invalid VC ID: " << static_cast<uint32_t> (vcId));
      return false;
    }

  uint32_t expectedSeq = m_waitSeq[source][vcId];

  if (seq_rev == expectedSeq)
    {
      // In-order packet, update wait sequence and send ACK
      m_waitSeq[source][vcId]++;
      SendLlrAck (vcId, seq_rev, source);

      NS_LOG_DEBUG ("Switch port received in-order packet seq " << seq_rev
                   << " on VC " << static_cast<uint32_t> (vcId)
                   << ", next expected: " << m_waitSeq[source][vcId]);

      return true; // Process the packet (forward it)
    }
  else if (seq_rev > expectedSeq)
    {
      // Out-of-order packet, missing packets detected
      NS_LOG_INFO ("Switch port received out-of-order packet seq " << seq_rev
                  << " on VC " << static_cast<uint32_t> (vcId)
                  << ", expected " << expectedSeq);

      // Send NACK for the missing sequence
      SendLlrNack (vcId, expectedSeq, source);

      return false; // Don't process the packet yet
    }
  else
    {
      // Duplicate packet, just send ACK and discard
      NS_LOG_DEBUG ("Switch port received duplicate packet seq " << seq_rev
                   << " on VC " << static_cast<uint32_t> (vcId));

      SendLlrAck (vcId, seq_rev, source);
      return false; // Discard duplicate
    }
}

void
LlrSwitchPortManager::SendLlrAck (uint8_t vcId, uint32_t seq, Mac48Address mac)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (vcId) << seq);

  if (!m_llrEnabled)
    {
      return;
    }

  // Create ACK packet (exactly like NetDevice SendLlrAck)
  Ptr<Packet> ackPacket = Create<Packet> ();

  // Add Ethernet header, source MAC is local port, destination MAC is peer port
  EthernetHeader ethHeader;
  if (m_getLocalMac)
    {
      ethHeader.SetSource (m_getLocalMac ());
    }
  ethHeader.SetDestination (mac);
  ethHeader.SetLengthType (0x0800);
  ackPacket->AddHeader (ethHeader);

  // Add CBFC header to carry VC ID
  SueCbfcHeader ackHeader;
  ackHeader.SetVcId (vcId);
  ackHeader.SetCredits (0); // ACK packets have 0 credits
  ackPacket->AddHeader (ackHeader);

  // PPP Header
  SuePppHeader ppp;
  ppp.SetProtocol (SuePacketUtils::EtherToPpp (SuePacketUtils::ACK_REV));
  ackPacket->AddHeader (ppp);

  // Add SueTag to carry sequence number
  SueTag ackTag (Simulator::Now (), seq);
  ackPacket->AddPacketTag (ackTag);

  NS_LOG_DEBUG ("Switch port sending ACK for seq " << seq
               << " to peer " << m_peerMac
               << " on VC " << static_cast<uint32_t> (vcId));

  // Schedule ACK transmission with delay
  Simulator::Schedule (m_AckAddHeaderDelay,
                      &LlrSwitchPortManager::SendControlPacket,
                      this, ackPacket, SuePacketUtils::ACK_REV);
}

void
LlrSwitchPortManager::SendLlrNack (uint8_t vcId, uint32_t seq, Mac48Address mac)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (vcId) << seq);

  if (!m_llrEnabled)
    {
      return;
    }

  // Create NACK packet (exactly like NetDevice SendLlrNack)
  Ptr<Packet> nackPacket = Create<Packet> ();

  // Add Ethernet header, source MAC is local port, destination MAC is peer port
  EthernetHeader ethHeader;
  if (m_getLocalMac)
    {
      ethHeader.SetSource (m_getLocalMac ());
    }
  ethHeader.SetDestination (mac);
  ethHeader.SetLengthType (0x0800);
  nackPacket->AddHeader (ethHeader);

  // Add CBFC header to carry VC ID
  SueCbfcHeader nackHeader;
  nackHeader.SetVcId (vcId);
  nackHeader.SetCredits (0); // NACK packets have 0 credits
  nackPacket->AddHeader (nackHeader);

  // PPP Header
  SuePppHeader ppp;
  ppp.SetProtocol (SuePacketUtils::EtherToPpp (SuePacketUtils::NACK_REV));
  nackPacket->AddHeader (ppp);

  // Add SueTag to carry sequence number
  SueTag nackTag (Simulator::Now (), seq);
  nackPacket->AddPacketTag (nackTag);

  NS_LOG_DEBUG ("Switch port sending NACK for seq " << seq
               << " to peer " << m_peerMac
               << " on VC " << static_cast<uint32_t> (vcId));

  // Schedule NACK transmission with delay
  Simulator::Schedule (m_AckAddHeaderDelay,
                      &LlrSwitchPortManager::SendControlPacket,
                      this, nackPacket, SuePacketUtils::NACK_REV);
}

void
LlrSwitchPortManager::ProcessLlrAck (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);

  if (!m_llrEnabled)
    {
      return;
    }

  // Extract sequence number from tag
  SueTag tag;
  bool found = p->PeekPacketTag (tag);
  if (!found)
    {
      NS_LOG_WARN ("ProcessLlrAck: no tag found, cannot process ACK");
      return;
    }

  uint32_t seq = tag.GetSequence ();

  SuePppHeader ppp;
  p->RemoveHeader (ppp);
  SueCbfcHeader ackHeader;
  p->RemoveHeader (ackHeader);
  uint8_t vcId = ackHeader.GetVcId ();

  // Key: use source MAC in packet header (exactly like NetDevice)
  EthernetHeader ethHeader;
  p->RemoveHeader (ethHeader);
  Mac48Address mac = ethHeader.GetSource ();

  if (vcId >= m_numVcs)
    {
      NS_LOG_WARN ("ProcessLlrAck: invalid VC ID " << static_cast<uint32_t> (vcId));
      return;
    }

  NS_LOG_DEBUG ("Switch port ProcessLlrAck: read seq " << seq << " for VC " << static_cast<uint32_t> (vcId) << " from MAC " << mac);

  // Start processing ACK sequence number (exactly like NetDevice)
  auto it1 = m_sendList[mac][vcId].begin ();
  auto it2 = m_sendList[mac][vcId].find (seq);
  if (seq < m_waitSeq[mac][vcId])
    { // Received duplicate or old ACK
      NS_LOG_INFO ("Switch port: Duplicate or old ACK received for VC " << static_cast<uint32_t> (vcId) << " seq " << seq << ", expected " << m_waitSeq[mac][vcId]);
      return;
    }
  else if (it2 == m_sendList[mac][vcId].end ())
    { // Cannot find corresponding seq
      NS_LOG_INFO ("Switch port: ACK received for VC " << static_cast<uint32_t> (vcId) << " seq " << seq << " which is not in send list, possible duplicate ACK or out-of-order ACK.");
      return;
    }
  else
    {
      // Found corresponding seq, delete this and previous packets
      m_sendList[mac][vcId].erase (it1, it2);
      m_sendList[mac][vcId].erase (it2);
      m_waitSeq[mac][vcId] = seq + 1;
      NS_LOG_INFO ("Switch port: Updated waitSeq for VC " << static_cast<uint32_t> (vcId) << " to " << m_waitSeq[mac][vcId]);
      m_llrResending[mac][vcId] = false; // Stop retransmission
      // Set retransmission timer
      if (m_resendPkt[mac][vcId].IsPending ())
        {
          m_resendPkt[mac][vcId].Cancel ();
        }
    }
}

void
LlrSwitchPortManager::ProcessLlrNack (Ptr<Packet> p)
{
  NS_LOG_FUNCTION (this << p);

  if (!m_llrEnabled)
    {
      return;
    }

  // Extract sequence number from tag
  SueTag tag;
  bool found = p->PeekPacketTag (tag);
  if (!found)
    {
      NS_LOG_WARN ("ProcessLlrNack: no tag found, cannot process NACK");
      return;
    }

  uint32_t seq = tag.GetSequence ();

  SuePppHeader ppp;
  p->RemoveHeader (ppp);
  SueCbfcHeader nackHeader;
  p->RemoveHeader (nackHeader);
  uint8_t vcId = nackHeader.GetVcId ();

  // Key: use source MAC in packet header (exactly like NetDevice)
  EthernetHeader ethHeader;
  p->RemoveHeader (ethHeader);
  Mac48Address mac = ethHeader.GetSource ();

  if (vcId >= m_numVcs)
    {
      NS_LOG_WARN ("ProcessLlrNack: invalid VC ID " << static_cast<uint32_t> (vcId));
      return;
    }

  NS_LOG_DEBUG ("Switch port ProcessLlrNack: read seq " << seq << " for VC " << static_cast<uint32_t> (vcId) << " from MAC " << mac);

  // Start processing NACK sequence number (exactly like NetDevice)
  auto it1 = m_sendList[mac][vcId].begin ();
  auto it2 = m_sendList[mac][vcId].find (seq);
  if (seq < m_waitSeq[mac][vcId])
    {
      NS_LOG_INFO ("Switch port: Duplicate or old NACK received for VC " << static_cast<uint32_t> (vcId) << " seq " << seq << ", expected " << m_waitSeq[mac][vcId]);
      return;
    }
  else if (it2 == m_sendList[mac][vcId].end ())
    {
      NS_LOG_INFO ("Switch port: NACK received for VC " << static_cast<uint32_t> (vcId) << " seq " << seq << " which is not in send list, possible duplicate NACK or out-of-order NACK.");
      return;
    }
  else
    {
      // Received NACK for seq, which means all packets before seq have been received, but seq packet is lost, retransmit seq and subsequent packets
      m_sendList[mac][vcId].erase (it1, it2);
      m_waitSeq[mac][vcId] = seq;
      m_llrResendseq[mac][vcId] = seq;
      m_llrResending[mac][vcId] = true;
      NS_LOG_INFO ("Switch port: NACK received, will resend from seq " << seq << " for VC " << static_cast<uint32_t> (vcId));
      // Set retransmission timer
      if (m_resendPkt[mac][vcId].IsPending ())
        {
          m_resendPkt[mac][vcId].Cancel ();
        }
      // Schedule retransmission
      m_resendPkt[mac][vcId] = Simulator::Schedule (m_llrTimeout,
                                                &LlrSwitchPortManager::Resend,
                                                this, vcId, mac);
    }
}

void
LlrSwitchPortManager::ResendInSwitch (uint8_t vcId, Mac48Address mac)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (vcId));

  if (!m_llrEnabled)
    {
      return;
    }

  // Switch internal optimized resend - same logic as regular resend but optimized for internal switch communication
  Resend (vcId, mac);
}

void
LlrSwitchPortManager::Resend (uint8_t vcId, Mac48Address mac)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (vcId));

  if (!m_llrEnabled || vcId >= m_numVcs)
    {
      return;
    }

  m_llrResending[mac][vcId] = false;

  if (m_sendList[mac][vcId].empty ())
    {
      NS_LOG_DEBUG ("Switch port: No packets to retransmit for VC " << static_cast<uint32_t> (vcId));
      return;
    }

  m_llrResending[mac][vcId] = true;
  m_llrResendseq[mac][vcId] = m_sendList[mac][vcId].begin ()->first;

  NS_LOG_DEBUG ("Switch port starting retransmission for VC " << static_cast<uint32_t> (vcId)
               << " from seq " << m_llrResendseq[mac][vcId]);

  // Trigger transmission attempt
  if (m_tryTransmit)
    {
      m_tryTransmit ();
    }
}

void
LlrSwitchPortManager::LlrResendPacket (uint8_t vcId, Mac48Address mac)
{
  NS_LOG_FUNCTION (this << static_cast<uint32_t> (vcId));

  if (!m_llrEnabled || vcId >= m_numVcs)
    {
      return;
    }

  if (!m_llrResending[mac][vcId] || m_sendList[mac][vcId].empty ())
    {
      return;
    }

  auto it = m_sendList[mac][vcId].find (m_llrResendseq[mac][vcId]);
  if (it == m_sendList[mac][vcId].end ())
    {
      NS_LOG_WARN ("Resend sequence not found in send list");
      m_llrResending[mac][vcId] = false;
      return;
    }

  Ptr<Packet> packet = it->second->Copy ();

  // Add sequence tag
  SueTag tag (Simulator::Now (), m_llrResendseq[mac][vcId]);
  packet->AddPacketTag (tag);

  NS_LOG_DEBUG ("Switch port resending packet seq " << m_llrResendseq[mac][vcId]
               << " on VC " << static_cast<uint32_t> (vcId));

  // Send packet
  if (m_sendPacket && !mac.IsGroup ())
    {
      m_sendPacket (packet, mac, m_protocolNum);
    }

  m_llrResendseq[mac][vcId]++;
}

bool
LlrSwitchPortManager::GetLlrEnabled (void) const
{
  return m_llrEnabled;
}

Mac48Address
LlrSwitchPortManager::GetPeerMac (void) const
{
  return m_peerMac;
}


void
LlrSwitchPortManager::SendControlPacket (Ptr<Packet> packet, uint16_t protocolNum)
{
  NS_LOG_FUNCTION (this << packet << protocolNum);

  if (m_sendPacket && !m_peerMac.IsGroup ())
    {
      m_sendPacket (packet, m_peerMac, protocolNum);
    }
  else
    {
      NS_LOG_WARN ("SendControlPacket: callback not set or peer MAC not configured");
    }
}

} // namespace ns3