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

#ifndef CBFC_MANAGER_H
#define CBFC_MANAGER_H

#include <map>
#include <queue>
#include <functional>
#include "ns3/mac48-address.h"
#include "ns3/packet.h"
#include "ns3/queue.h"
#include "ns3/ptr.h"
#include "ns3/nstime.h"
#include "ns3/object.h"
#include "ns3/ethernet-header.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/node.h"
#include "ns3/net-device.h"

namespace ns3 {

// Forward declarations
class SueCbfcHeader;

/**
 * \brief Callback types for NetDevice operations
 */
typedef std::function<Mac48Address()> GetLocalMacCallback;
typedef std::function<Ptr<Node>()> GetNodeCallback;
typedef std::function<void(Ptr<Packet>, Mac48Address, uint16_t)> SendPacketCallback;

/**
 * \brief Credit-Based Flow Control (CBFC) Manager
 *
 * This class manages credit-based flow control for virtual channels (VCs),
 * including credit allocation, capacity reservation, and queue management.
 * It encapsulates all CBFC-related functionality from the original net device.
 */
class CbfcManager : public Object
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /**
   * Construct a CbfcManager
   */
  CbfcManager ();

  /**
   * Destroy a CbfcManager
   */
  virtual ~CbfcManager ();

  /**
   * \brief Initialize CBFC functionality (internal use)
   *
   * Sets up initial credits and CBFC state.
   * This method is for internal use by other initialization methods.
   */
  void InitializeBasic (void);

  /**
   * \brief Configure and initialize CBFC with peer device credits
   *
   * This method combines Configure, InitializeBasic, and InitializePeerDeviceCredits
   * for a streamlined initialization process.
   *
   * \param numVcs Number of virtual channels
   * \param initialCredits Initial credit count for each VC
   * \param enableLinkCBFC Whether link-level CBFC is enabled
   * \param creditBatchSize Credit batch size
   * \param getLocalMac Callback to get local MAC address
   * \param getNode Callback to get node pointer
   * \param sendPacket Callback to send packets
   * \param creditGenerateDelay Credit generation delay
   * \param protocolNum Protocol number for CBFC updates
   * \param getRemoteMac Callback to get remote MAC address
   * \param isSwitchDevice Callback to check if device is a switch
   * \param switchCredits Default credits for switch devices (default: 85)
   */
  void Initialize (uint8_t numVcs,
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
                  uint32_t switchCredits = 85);

  /**
   * \brief Configure CBFC parameters
   *
   * \param numVcs Number of virtual channels
   * \param initialCredits Initial credit count for each VC
   * \param enableLinkCBFC Whether link-level CBFC is enabled
   * \param creditBatchSize Credit batch size
   */
  void Configure (uint8_t numVcs,
                 uint32_t initialCredits,
                 bool enableLinkCBFC,
                 uint32_t creditBatchSize);

  /**
   * \brief Set callbacks for NetDevice operations
   *
   * \param getLocalMac Callback to get local MAC address
   * \param getNode Callback to get node pointer
   * \param sendPacket Callback to send packets
   * \param creditGenerateDelay Credit generation delay
   * \param protocolNum Protocol number for CBFC updates
   */
  void SetCallbacks (GetLocalMacCallback getLocalMac,
                    GetNodeCallback getNode,
                    SendPacketCallback sendPacket,
                    Time creditGenerateDelay,
                    uint16_t protocolNum);

  /**
   * \brief Handle credit return for a target device and VC (migrated from PointToPointSueNetDevice)
   *
   * \param targetMac MAC address of the target device
   * \param vcId Virtual channel ID
   */
  void CreditReturn (Mac48Address targetMac, uint8_t vcId);

  /**
   * \brief Add a peer device for credit management
   *
   * \param peerMac MAC address of the peer device
   * \param initialCredits Initial credits for this peer (0 to use default)
   */
  void AddPeerDevice (Mac48Address peerMac, uint32_t initialCredits = 0);

  /**
   * \brief Initialize credits for peer devices (migrated from PointToPointSueNetDevice)
   *
   * This function handles the credit initialization logic that was previously
   * in PointToPointSueNetDevice::InitializeCbfc(), including adding peer devices
   * with initial credits and handling switch device credit allocation.
   *
   * \param getRemoteMac Callback to get remote MAC address
   * \param isSwitchDevice Callback to check if device is a switch
   * \param switchCredits Default credits for switch devices (default: 85)
   */
  void InitializePeerDeviceCredits (std::function<Mac48Address()> getRemoteMac,
                                   std::function<bool()> isSwitchDevice,
                                   uint32_t switchCredits = 85);

  /**
   * \brief Get the number of transmit credits for a specific peer and VC (migrated from PointToPointSueNetDevice)
   *
   * \param mac MAC address of the peer device
   * \param vcId Virtual channel ID
   * \return Number of available credits
   */
  uint32_t GetTxCredits (Mac48Address mac, uint8_t vcId) const;

  /**
   * \brief Decrement transmit credits for a specific peer and VC (migrated from PointToPointSueNetDevice)
   *
   * \param mac MAC address of the peer device
   * \param vcId Virtual channel ID
   * \return True if credits were successfully decremented, false if no credits available
   */
  bool DecrementTxCredits (Mac48Address mac, uint8_t vcId);

  /**
   * \brief Add transmit credits for a specific peer and VC
   *
   * \param mac MAC address of the peer device
   * \param vcId Virtual channel ID
   * \param credits Number of credits to add
   */
  void AddTxCredits (Mac48Address mac, uint8_t vcId, uint32_t credits);

  /**
   * \brief Handle credit return for a received packet (migrated from PointToPointSueNetDevice)
   *
   * \param ethHeader Ethernet header of the received packet
   * \param vcId Virtual channel ID
   */
  void HandleCreditReturn (const EthernetHeader& ethHeader, uint8_t vcId);

  /**
   * \brief Get the number of credits to return to a peer device
   *
   * \param peerMac MAC address of the peer device
   * \param vcId Virtual channel ID
   * \return Number of credits to return
   */
  uint32_t GetCreditsToReturn (Mac48Address peerMac, uint8_t vcId) const;

  /**
   * \brief Clear credits to return for a peer device and VC
   *
   * \param peerMac MAC address of the peer device
   * \param vcId Virtual channel ID
   * \return Number of credits that were cleared
   */
  uint32_t ClearCreditsToReturn (Mac48Address peerMac, uint8_t vcId);

  /**
   * \brief Check if CBFC is enabled
   *
   * \return True if CBFC is enabled
   */
  bool IsEnabled (void) const;

  /**
   * \brief Check if CBFC is initialized
   *
   * \return True if CBFC is initialized
   */
  bool IsInitialized (void) const;

  /**
   * \brief Get the number of virtual channels
   *
   * \return Number of virtual channels
   */
  uint8_t GetNumVcs (void) const;

  /**
   * \brief Get initial credit count
   *
   * \return Initial credit count
   */
  uint32_t GetInitialCredits (void) const;

  /**
   * \brief Get credit batch size
   *
   * \return Credit batch size
   */
  uint32_t GetCreditBatchSize (void) const;

  /**
   * \brief Get access to the internal TX credits map for logging purposes
   *
   * \return Reference to the internal TX credits map
   */
  const std::map<Mac48Address, std::map<uint8_t, uint32_t>>& GetTxCreditsMap (void) const;

  /**
   * \brief Check if link-level CBFC is enabled (migrated from PointToPointSueNetDevice)
   *
   * \return True if link-level CBFC is enabled
   */
  bool IsLinkCbfcEnabled (void) const;

private:
  /**
   * \brief Internal method to send credit packet via callback
   *
   * \param packet Credit packet to send
   * \param targetMac Target MAC address
   * \param protocolNum Protocol number
   */
  void SendCreditPacket (Ptr<Packet> packet, Mac48Address targetMac, uint16_t protocolNum);

  bool m_initialized;                    //!< CBFC initialization flag
  bool m_enableLinkCBFC;                 //!< CBFC enable flag

  // Credit management maps - replicated from original logic
  std::map<Mac48Address, std::map<uint8_t, uint32_t>> m_txCreditsMap;        //!< TX credits: MAC -> VC -> credits
  std::map<Mac48Address, std::map<uint8_t, uint32_t>> m_rxCreditsToReturnMap; //!< RX credits to return: MAC -> VC -> credits

  // Configuration parameters
  uint32_t m_initialCredits;          //!< Initial credit count
  uint8_t m_numVcs;                   //!< Number of virtual channels
  uint32_t m_creditBatchSize;         //!< Credit batch size

  // Callbacks for NetDevice operations
  GetLocalMacCallback m_getLocalMac;  //!< Callback to get local MAC address
  GetNodeCallback m_getNode;          //!< Callback to get node pointer
  SendPacketCallback m_sendPacket;    //!< Callback to send packets
  Time m_creditGenerateDelay;         //!< Credit generation delay
  uint16_t m_protocolNum;             //!< Protocol number for CBFC updates
  bool m_callbacksSet;                //!< Flag indicating if callbacks are set
};

} // namespace ns3

#endif /* CBFC_MANAGER_H */