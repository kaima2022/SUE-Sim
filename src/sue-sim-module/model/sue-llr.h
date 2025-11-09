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

#ifndef SUE_LLR_H
#define SUE_LLR_H

#include <map>
#include <functional>
#include "ns3/mac48-address.h"
#include "ns3/packet.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/event-id.h"
#include "ns3/node.h"
#include "ns3/ethernet-header.h"
#include "sue-ppp-header.h"
#include "sue-cbfc-header.h"
#include "sue-tag.h"
#include "sue-utils.h"
#include "sue-switch.h"

namespace ns3 {

// Forward declarations
class SueSwitch;

/**
 * \brief Callback types for LLR operations
 */
typedef std::function<Mac48Address()> GetLocalMacCallback;
typedef std::function<Ptr<Node>()> GetNodeCallback;
typedef std::function<void(Ptr<Packet>, Mac48Address, uint16_t)> SendPacketCallback;
typedef std::function<void()> TryTransmitCallback;
typedef std::function<Mac48Address()> GetRemoteMacCallback;
typedef std::function<Ptr<SueSwitch>()> GetSwitchCallback;

/**
 * \brief Structure holding info for a sent packet tracked by LLR
 * (Consistent with NetDevice implementation)
 */
struct LlrPacketInfo
{
  Ptr<Packet> packet;      //!< The packet data
  Time sendTime;           //!< Time when packet was sent
  uint32_t sequenceNumber; //!< Sequence number assigned to this packet
};

/**
 * \brief LLR Manager for End Nodes (NICs)
 *
 * Specialized LLR manager for end devices/nodes. Handles complete LLR communication
 * between NIC and switch, exactly matching the NetDevice implementation.
 *
 * Key responsibilities:
 * - Send data packets to switch MAC with sequence numbers
 * - Process ACK/NACK responses from switch
 * - Handle timeout-based retransmissions
 * - Maintain per-VC state for reliable delivery to switch port
 * - Support both regular and switch-internal communication paths
 */
class LlrNodeManager : public Object
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /**
   * Construct a LlrNodeManager
   */
  LlrNodeManager ();

  /**
   * Destroy a LlrNodeManager
   */
  virtual ~LlrNodeManager ();

  /**
   * \brief Initialize node LLR functionality (matching NetDevice variable names)
   *
   * \param llrEnabled Whether LLR is enabled (m_llrEnabled)
   * \param llrWindowSize LLR window size (m_llrWindowSize)
   * \param llrTimeout Retransmission timeout (m_llrTimeout)
   * \param ackAddHeaderDelay Delay for adding ACK headers (m_AckAddHeaderDelay)
   * \param ackProcessDelay Delay for processing ACK/NACK (m_AckProcessDelay)
   * \param protocolNum Protocol number for LLR packets
   * \param numVcs Number of virtual channels
   * \param getLocalMac Callback to get local MAC address
   * \param getNode Callback to get node pointer
   * \param getRemoteMac Callback to get remote MAC address
   * \param sendPacket Callback to send packets
   * \param tryTransmit Callback to trigger transmission attempt
   */
  void Initialize (bool llrEnabled,
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
                  TryTransmitCallback tryTransmit);

  // Core LLR operations (matching NetDevice function names)
  /**
   * \brief Send data packet with LLR processing (matching LlrSendPacket)
   *
   * \param packet Packet to send
   * \param vcId Virtual channel ID
   * \return Sequence number assigned to the packet
   */
  uint32_t LlrSendPacket (Ptr<Packet> packet, uint8_t vcId);

  /**
   * \brief Process received data packet (matching LlrReceivePacket)
   *
   * \param packet Received packet
   * \param vcId Virtual channel ID
   * \param source Source MAC address
   * \param seq_rev Received sequence number
   */
  void LlrReceivePacket (Ptr<Packet> packet, uint8_t vcId, Mac48Address source, uint32_t seq_rev);

  /**
   * \brief Send LLR ACK (matching SendLlrAck)
   *
   * \param vcId Virtual channel ID
   * \param seq Sequence number to acknowledge
   */
  void SendLlrAck (uint8_t vcId, uint32_t seq);

  /**
   * \brief Send LLR NACK (matching SendLlrNack)
   *
   * \param vcId Virtual channel ID
   * \param seq Sequence number that was lost
   */
  void SendLlrNack (uint8_t vcId, uint32_t seq);

  /**
   * \brief Process received ACK packet (matching ProcessLlrAck)
   *
   * \param p ACK packet to process
   */
  void ProcessLlrAck (Ptr<Packet> p);

  /**
   * \brief Process received NACK packet (matching ProcessLlrNack)
   *
   * \param p NACK packet to process
   */
  void ProcessLlrNack (Ptr<Packet> p);

  /**
   * \brief Timeout-based resend for regular devices (matching Resend)
   *
   * \param vcId Virtual channel ID
   */
  void Resend (uint8_t vcId);

  /**
   * \brief Perform resend of pending sequences (matching LlrResendPacket)
   *
   * \param vcId Virtual channel ID
   */
  void LlrResendPacket (uint8_t vcId);

  // State query methods (matching NetDevice function names)
  bool GetLlrEnabled (void) const;
  bool IsLlrResending (uint8_t vcId) const;

  
private:
  // Configuration (matching NetDevice variable names)
  bool m_llrEnabled;                  //!< Whether LLR is enabled
  uint32_t m_llrWindowSize;           //!< LLR window size (max outstanding packets per VC)
  Time m_llrTimeout;                  //!< Retransmission timeout
  Time m_AckAddHeaderDelay;           //!< Delay to add ACK/NACK header
  Time m_AckProcessDelay;             //!< Delay to process received ACK/NACK
  uint8_t m_numVcs;                   //!< Number of virtual channels
  uint16_t m_protocolNum;             //!< Protocol number for LLR packets

  // Peer MAC addresses
  Mac48Address m_switchMac;           //!< Switch MAC address for communication

  // LLR state data structures (simplified for single switch communication)
  std::vector<std::map<uint32_t, Ptr<Packet>>> m_sendList;              //!< Per VC map seq->packet
  std::vector<uint32_t> m_waitSeq;                                     //!< Per VC: expected next receive sequence
  std::vector<uint32_t> m_sendSeq;                                     //!< Per VC: next sequence to send
  std::vector<uint32_t> m_unack;                                       //!< Per VC: outstanding unacknowledged sequences
  uint32_t m_llrResendseq;                                             //!< Next sequence to resend

  // State flags (matching NetDevice implementation)
  std::vector<bool> m_llrWait;                                         //!< Per VC: waiting for ACK
  std::vector<bool> m_llrResending;                                    //!< Per VC: currently resending

  // Timing information (matching NetDevice implementation)
  std::vector<Time> m_lastAckedTime;                                   //!< Per VC: last ACK receive time
  std::vector<Time> m_lastAcksend;                                      //!< Per VC: last ACK send time

  // Retransmission events (matching NetDevice implementation)
  std::vector<EventId> m_resendPkt;                                     //!< Per VC: scheduled resend events

  // Callbacks
  GetLocalMacCallback m_getLocalMac;
  GetNodeCallback m_getNode;
  GetRemoteMacCallback m_getRemoteMac;
  SendPacketCallback m_sendPacket;
  TryTransmitCallback m_tryTransmit;

  // Internal helper methods
  void SendControlPacket (Ptr<Packet> packet, uint16_t protocolNum);
};

/**
 * \brief LLR Manager for Switch Ports
 *
 * Specialized LLR manager for switch ports. Handles LLR communication for
 * each switch port exactly matching the NetDevice implementation for switch devices.
 *
 * Key responsibilities:
 * - Send data packets to peer MAC with sequence numbers
 * - Process ACK/NACK responses from peer
 * - Handle timeout-based retransmissions
 * - Support switch-internal forwarding with optimized retransmission path
 * - Maintain per-VC state for reliable delivery to connected peers
 */
class LlrSwitchPortManager : public Object
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /**
   * Construct a LlrSwitchPortManager
   */
  LlrSwitchPortManager ();

  /**
   * Destroy a LlrSwitchPortManager
   */
  virtual ~LlrSwitchPortManager ();

  /**
   * \brief Initialize switch port LLR functionality (matching NetDevice variable names)
   *
   * \param llrEnabled Whether LLR is enabled (m_llrEnabled)
   * \param llrWindowSize LLR window size (m_llrWindowSize)
   * \param llrTimeout Retransmission timeout (m_llrTimeout)
   * \param ackAddHeaderDelay Delay for adding ACK headers (m_AckAddHeaderDelay)
   * \param ackProcessDelay Delay for processing ACK/NACK (m_AckProcessDelay)
   * \param protocolNum Protocol number for LLR packets
   * \param numVcs Number of virtual channels
   * \param getLocalMac Callback to get local MAC address
   * \param getNode Callback to get node pointer
   * \param getSwitch Callback to get switch module
   * \param sendPacket Callback to send packets
   * \param tryTransmit Callback to trigger transmission attempt
   * \param peerMac MAC address of connected peer device
   */
  void Initialize (bool llrEnabled,
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
                  Mac48Address peerMac);

  // Core LLR operations (matching NetDevice function names for switch ports)
  /**
   * \brief Send data packet with LLR processing (matching LlrSendPacket for switch ports)
   *
   * \param packet Packet to send
   * \param vcId Virtual channel ID
   * \param mac Target MAC address
   * \return Sequence number assigned to the packet
   */
  uint32_t LlrSendPacket (Ptr<Packet> packet, uint8_t vcId, Mac48Address mac);

  /**
   * \brief Process received data packet (matching LlrReceivePacket for switch ports)
   *
   * \param packet Received packet
   * \param vcId Virtual channel ID
   * \param seq_rev Received sequence number
   * \param source Source MAC address
   * \return True if packet should be processed, false if duplicate/out-of-order
   */
  bool LlrReceivePacket (Ptr<Packet> packet, uint8_t vcId, uint32_t seq_rev, Mac48Address source);

  /**
   * \brief Send LLR ACK for switch port (matching SendLlrAck)
   *
   * \param vcId Virtual channel ID
   * \param seq Sequence number to acknowledge
   * \param mac Target MAC address
   */
  void SendLlrAck (uint8_t vcId, uint32_t seq, Mac48Address mac);

  /**
   * \brief Send LLR NACK for switch port (matching SendLlrNack)
   *
   * \param vcId Virtual channel ID
   * \param seq Sequence number that was lost
   * \param mac Target MAC address
   */
  void SendLlrNack (uint8_t vcId, uint32_t seq, Mac48Address mac);

  /**
   * \brief Process received ACK packet (matching ProcessLlrAck for switch ports)
   *
   * \param p ACK packet to process
   */
  void ProcessLlrAck (Ptr<Packet> p);

  /**
   * \brief Process received NACK packet (matching ProcessLlrNack for switch ports)
   *
   * \param p NACK packet to process
   */
  void ProcessLlrNack (Ptr<Packet> p);

  /**
   * \brief Switch internal optimized resend (matching ResendInSwitch)
   *
   * \param vcId Virtual channel ID
   * \param mac Target MAC address
   */
  void ResendInSwitch (uint8_t vcId, Mac48Address mac);

  /**
   * \brief Regular resend for switch ports (matching Resend)
   *
   * \param vcId Virtual channel ID
   * \param mac Target MAC address
   */
  void Resend (uint8_t vcId, Mac48Address mac);

  /**
   * \brief Perform resend for switch ports (matching LlrResendPacket)
   *
   * \param vcId Virtual channel ID
   * \param mac Target MAC address
   */
  void LlrResendPacket (uint8_t vcId, Mac48Address mac);

  // State query methods (matching NetDevice function names)
  bool GetLlrEnabled (void) const;
  Mac48Address GetPeerMac (void) const;

  
private:
  // Configuration (matching NetDevice variable names)
  bool m_llrEnabled;                  //!< Whether LLR is enabled
  uint32_t m_llrWindowSize;           //!< LLR window size (max outstanding packets per VC)
  Time m_llrTimeout;                  //!< Retransmission timeout
  Time m_AckAddHeaderDelay;           //!< Delay to add ACK/NACK header
  Time m_AckProcessDelay;             //!< Delay to process received ACK/NACK
  uint8_t m_numVcs;                   //!< Number of virtual channels
  uint16_t m_protocolNum;             //!< Protocol number for LLR packets
  Mac48Address m_peerMac;             //!< MAC address of connected peer device

  // LLR state data structures (exactly matching NetDevice implementation for multiple peers)
  std::map<Mac48Address, std::vector<std::map<uint32_t, Ptr<Packet>>>> m_sendList;   //!< Per MAC, per VC map seq->packet
  std::map<Mac48Address, std::vector<uint32_t>> m_waitSeq;                           //!< Per MAC, per VC: expected next receive sequence
  std::map<Mac48Address, std::vector<uint32_t>> m_sendSeq;                           //!< Per MAC, per VC: next sequence to send
  std::map<Mac48Address, std::vector<uint32_t>> m_unack;                             //!< Per MAC, per VC: outstanding unacknowledged sequences
  std::map<Mac48Address, std::vector<uint32_t>> m_llrResendseq;                      //!< Per MAC, per VC: next sequence to resend

  // State flags (exactly matching NetDevice implementation)
  std::map<Mac48Address, std::vector<bool>> m_llrWait;                               //!< Per MAC, per VC: waiting for ACK
  std::map<Mac48Address, std::vector<bool>> m_llrResending;                          //!< Per MAC, per VC: currently resending

  // Timing information (exactly matching NetDevice implementation)
  std::map<Mac48Address, std::vector<Time>> m_lastAckedTime;                         //!< Per MAC, per VC: last ACK receive time
  std::map<Mac48Address, std::vector<Time>> m_lastAcksend;                            //!< Per MAC, per VC: last ACK send time

  // Retransmission events (exactly matching NetDevice implementation)
  std::map<Mac48Address, std::vector<EventId>> m_resendPkt;                           //!< Per MAC, per VC: scheduled resend events

  // Callbacks
  GetLocalMacCallback m_getLocalMac;
  GetNodeCallback m_getNode;
  GetSwitchCallback m_getSwitch;
  SendPacketCallback m_sendPacket;
  TryTransmitCallback m_tryTransmit;

  // Internal helper methods
  void SendControlPacket (Ptr<Packet> packet, uint16_t protocolNum);
};

} // namespace ns3

#endif /* SUE_LLR_H */