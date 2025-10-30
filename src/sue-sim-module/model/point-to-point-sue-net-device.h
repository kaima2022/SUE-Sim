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

#ifndef POINT_TO_POINT_SUE_NET_DEVICE_H
#define POINT_TO_POINT_SUE_NET_DEVICE_H

#include <cstring>
#include <map>
#include <queue>
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/net-device.h"
#include "ns3/callback.h"
#include "ns3/packet.h"
#include "ns3/traced-callback.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/ptr.h"
#include "ns3/mac48-address.h"
#include "sue-ppp-header.h"
#include "ns3/ethernet-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/queue.h"
#include "sue-switch.h"

namespace ns3 {

class PointToPointSueChannel;
class ErrorModel;
class SueCbfcHeader;
class SueSwitch;

/**
 * \defgroup point-to-point-sue Point-To-Point SUE Network Device
 * This section documents the API of the ns-3 point-to-point-sue module.
 * For a functional description, please refer to the ns-3 manual.
 */

/**
 * \ingroup point-to-point-sue
 * \class PointToPointSueNetDevice
 * \brief A Device for a Point to Point Network Link with SUE enhancements.
 *
 * This PointToPointSueNetDevice class specializes the NetDevice abstract
 * base class. Together with a PointToPointSueChannel (and a peer
 * PointToPointSueNetDevice), the class models, with some level of
 * abstraction, a generic point-to-point or serial link with SUE enhancements
 * including Credit-Based Flow Control (CBFC) and Virtual Channel support.
 * Key parameters or objects that can be specified for this device
 * include multiple VC queues, data rate, interframe transmission gap, and
 * CBFC credit management.
 */

/**
 * \brief Structure for items in the processing queue
 */
struct ProcessItem
{
  Ptr<Packet> originalPacket; //!< Original packet before processing
  Ptr<Packet> packet;         //!< Processed packet
  uint8_t vcId;              //!< Virtual Channel ID
  uint16_t protocol;         //!< Protocol number
};

class PointToPointSueNetDevice : public NetDevice
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /**
   * Construct a PointToPointSueNetDevice
   */
  PointToPointSueNetDevice ();

  /**
   * Destroy a PointToPointSueNetDevice
   */
  virtual ~PointToPointSueNetDevice ();

  /**
   * \brief Set the Data Rate used for transmission of packets.
   *
   * The data rate is set in the Attach () method from the corresponding field in the channel
   * to which the device is attached.  It can be overridden using this method.
   *
   * \param bps the data rate at which this object operates
   */
  void SetDataRate (DataRate bps);

  /**
   * \brief Set the interframe gap used to separate packets.
   *
   * The interframe gap defines the minimum space required between packets sent by this device.
   *
   * \param t the interframe gap time
   */
  void SetInterframeGap (Time t);

  /**
   * \brief Attach the device to a channel.
   *
   * \param ch Ptr to the channel to which this object is being attached.
   * \return true if the operation was successful (always true actually)
   */
  bool Attach (Ptr<PointToPointSueChannel> ch);

  /**
   * \brief Attach a queue to the PointToPointSueNetDevice.
   *
   * The PointToPointSueNetDevice "owns" a queue that implements a queueing
   * method such as DropTailQueue or RedQueue
   *
   * \param queue Ptr to the new queue.
   */
  void SetQueue (Ptr<Queue<Packet> > queue);

  /**
   * \brief Get a copy of the attached Queue.
   *
   * \returns Ptr to the queue.
   */
  Ptr<Queue<Packet> > GetQueue (void) const;

  /**
   * \brief Attach a receive ErrorModel to the PointToPointSueNetDevice.
   *
   * The PointToPointSueNetDevice may optionally include an ErrorModel in
   * the packet receive chain.
   *
   * \param em Ptr to the ErrorModel.
   */
  void SetReceiveErrorModel (Ptr<ErrorModel> em);

  /**
   * \brief Set the maximum VC queue size in bytes.
   *
   * \param maxBytes Maximum queue size in bytes
   */
  void SetVcQueueMaxBytes(uint32_t maxBytes);

  /**
   * \brief Get the maximum VC queue size in bytes.
   *
   * \return Maximum queue size in bytes
   */
  uint32_t GetVcQueueMaxBytes(void) const;

  /**
   * \brief Enable or disable logging for statistics.
   *
   * \param enabled True to enable logging, false to disable
   */
  void SetLoggingEnabled(bool enabled);

  
  /**
   * \brief Get the total number of dropped packets.
   *
   * \return Total dropped packet count
   */
  uint32_t GetTotalPacketDropNum();

  /**
   * \brief Set the global IP to MAC address mapping.
   *
   * \param map Map of IP addresses to MAC addresses
   */
  static void SetGlobalIpMacMap(const std::map<Ipv4Address, Mac48Address>& map);

  /**
   * \brief Get MAC address for a given IP address.
   *
   * \param IP address to lookup
   * \return Corresponding MAC address
   */
  static Mac48Address GetMacForIp(Ipv4Address ip);

  // Switch support methods
  /**
   * \brief Extract VC ID from a packet
   *
   * \param packet Packet to extract from
   * \return VC ID
   */
  uint8_t ExtractVcIdFromPacket(Ptr<const Packet> packet);

  /**
   * \brief Get the switch module
   *
   * \return Pointer to the switch module
   */
  Ptr<SueSwitch> GetSwitch() const;

  /**
   * \brief Set the switch module
   *
   * \param switchModule Pointer to the switch module
   */
  void SetSwitch(Ptr<SueSwitch> switchModule);

  // LLR support methods for switch
  /**
   * \brief Check if LLR is enabled
   *
   * \return true if LLR is enabled
   */
  bool GetLlrEnabled() const;

  /**
   * \brief Check if a MAC/VC is currently resending
   *
   * \param mac MAC address
   * \param vcId Virtual Channel ID
   * \return true if currently resending
   */
  bool IsLlrResending(Mac48Address mac, uint8_t vcId) const;

  
  /**
   * \brief Get TX credits for a specific MAC/VC
   *
   * \param mac MAC address
   * \param vcId Virtual Channel ID
   * \return Available credits
   */
  uint32_t GetTxCredits(Mac48Address mac, uint8_t vcId) const;

  /**
   * \brief Decrement TX credits for a specific MAC/VC
   *
   * \param mac MAC address
   * \param vcId Virtual Channel ID
   */
  void DecrementTxCredits(Mac48Address mac, uint8_t vcId);

  /**
   * \brief Check if CBFC is enabled
   *
   * \return true if CBFC is enabled
   */
  bool IsLinkCbfcEnabled() const;

  /**
   * \brief Get switch forwarding delay
   *
   * \return Switch forwarding delay
   */
  Time GetSwitchForwardDelay() const;

  /**
   * \brief Handle credit return
   *
   * \param ethHeader Ethernet header
   * \param vcId Virtual Channel ID
   */
  void HandleCreditReturn(const EthernetHeader& ethHeader, uint8_t vcId);

  
  // Backward compatibility methods - delegate to SueSwitch
  /**
   * \brief Check if this device is a switch device.
   *
   * \return true if this is a switch device
   */
  bool IsSwitchDevice() const;

  /**
   * \brief Check if a MAC address belongs to a switch device.
   *
   * \param mac MAC address to check
   * \return true if MAC belongs to a switch device
   */
  bool IsMacSwitchDevice(Mac48Address mac) const;

  /**
   * \brief Set the forwarding table for switch devices.
   *
   * \param table Map of destination MAC addresses to output port indices
   */
  void SetForwardingTable(const std::map<Mac48Address, uint32_t>& table);

  /**
   * \brief Clear the forwarding table.
   */
  void ClearForwardingTable();

  /**
   * \brief Receive a packet from a connected PointToPointSueChannel.
   *
   * The PointToPointSueNetDevice receives packets from its connected channel
   * and forwards them up the protocol stack.  This is the public method
   * used by the channel to indicate that the last bit of a packet has
   * arrived at the device.
   *
   * \param p Ptr to the received packet.
   */
  void Receive (Ptr<Packet> p);

  // The remaining methods are documented in ns3::NetDevice
  virtual void SetIfIndex (const uint32_t index);
  virtual uint32_t GetIfIndex (void) const;
  virtual Ptr<Channel> GetChannel (void) const;
  virtual void SetAddress (Address address);
  virtual Address GetAddress (void) const;
  virtual bool SetMtu (const uint16_t mtu);
  virtual uint16_t GetMtu (void) const;
  virtual bool IsLinkUp (void) const;
  virtual void AddLinkChangeCallback (Callback<void> callback);
  virtual bool IsBroadcast (void) const;
  virtual Address GetBroadcast (void) const;
  virtual bool IsMulticast (void) const;
  virtual Address GetMulticast (Ipv4Address multicastGroup) const;
  virtual bool IsPointToPoint (void) const;
  virtual bool IsBridge (void) const;
  virtual bool Send (Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber);
  virtual bool SendFrom (Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber);
  virtual Ptr<Node> GetNode (void) const;
  virtual void SetNode (Ptr<Node> node);
  virtual bool NeedsArp (void) const;
  virtual void SetReceiveCallback (NetDevice::ReceiveCallback cb);
  virtual Address GetMulticast (Ipv6Address addr) const;
  virtual void SetPromiscReceiveCallback (PromiscReceiveCallback cb);
  virtual bool SupportsSendFrom (void) const;

  /**
   * \brief CBFC protocol number
   */
  static const uint16_t PROT_CBFC_UPDATE = 0xCBFC;

protected:
  /**
   * \brief Dispose of the object
   */
  virtual void DoDispose (void);

  /**
   * \brief Handler for MPI receive event
   *
   * \param p Packet received
   */
  void DoMpiReceive (Ptr<Packet> p);

private:
  /**
   * \brief Copy constructor
   *
   * The method is private, so it is DISABLED.
   *
   * \param o Other NetDevice
   */
  PointToPointSueNetDevice (const PointToPointSueNetDevice &o);

  /**
   * \brief Assignment operator
   *
   * The method is private, so it is DISABLED.
   *
   * \param o Other NetDevice
   * \return Reference to this NetDevice
   */
  PointToPointSueNetDevice& operator = (const PointToPointSueNetDevice &o);

  /**
   * \returns the address of the remote device connected to this device
   * through the point to point channel.
   */
  Address GetRemote (void) const;

  /**
   * Adds the necessary headers and trailers to a packet of data in order to
   * respect the protocol implemented by the agent.
   * \param p packet
   * \param protocolNumber protocol number
   */
  void AddHeader (Ptr<Packet> p, uint16_t protocolNumber,uint32_t seq);

  /**
   * Removes, from a packet of data, all headers and trailers that
   * relate to the protocol implemented by the agent
   * \param p Packet whose headers need to be processed
   * \param param An integer parameter that can be set by the function
   * \return Returns true if the packet should be forwarded up the
   * protocol stack.
   */
  bool ProcessHeader (Ptr<Packet> p, uint16_t& param,uint32_t& seq);

  /**
   * Start Sending a Packet Down the Wire.
   *
   * The TransmitStart method is the method that is used internally in the
   * PointToPointSueNetDevice to begin the process of sending a packet out on
   * the channel.
   *
   * \see PointToPointSueChannel::TransmitStart ()
   * \see TransmitComplete()
   * \param p a reference to the packet to send
   * \returns true if success, false on failure
   */
  bool TransmitStart (Ptr<Packet> p);

  /**
   * Stop Sending a Packet Down the Wire and Begin the Interframe Gap.
   *
   * The TransmitComplete method is used internally to finish the process
   * of sending a packet out on the channel.
   */
  void TransmitComplete (void);

  /**
   * \brief Make the link up and running
   *
   * It calls also the linkChange callback.
   */
  void NotifyLinkUp (void);

  
  /**
   * \brief Log device statistics
   */
  void LogStatistics();

  /**
   * \brief Initialize CBFC credits
   */
  void InitializeCbfc();

  public:

  /**
   * \brief Return credits to a target MAC for a specific VC
   *
   * \param targetMac Target MAC address
   * \param vcId Virtual Channel ID
   */
  void CreditReturn(Mac48Address targetMac, uint8_t vcId);

  /**
   * \brief Extract destination IP from a packet
   *
   * \param packet Packet to extract from
   * \return Destination IP address
   */
  Ipv4Address ExtractDestIpFromPacket(Ptr<Packet> packet);

  /**
   * \brief Add Ethernet header to a packet
   *
   * \param packet Packet to add header to
   * \param destMac Destination MAC address
   */
  void AddEthernetHeader(Ptr<Packet> packet, Mac48Address destMac);

  /**
   * \brief Try to transmit packets from VC queues
   */
  void TryTransmit();

  /**
   * \brief Handle dropped packets
   *
   * \param droppedPacket The dropped packet
   */
  void HandlePacketDrop(Ptr<const Packet> droppedPacket);

  /**
   * \brief Start processing packets in the processing queue
   */
  void StartProcessing();

  /**
   * \brief Complete processing of a packet item
   *
   * \param item The packet item to complete processing
   */
  void CompleteProcessing(ProcessItem item);

  /**
   * \brief Log device queue usage statistics
   */
  void LogDeviceQueueUsage();

  /**
   * \brief Get remote MAC address
   *
   * \return Remote MAC address
   */
  Mac48Address GetRemoteMac();

  /**
   * \brief Get local MAC address
   *
   * \return Local MAC address
   */
  Mac48Address GetLocalMac();

  /**
   * \brief Find device and send packet to target MAC
   *
   * \param packet Packet to send
   * \param targetMac Target MAC address
   * \param protocolNum Protocol number
   */
  void FindDeviceAndSend (Ptr<Packet> packet, Mac48Address targetMac, uint16_t protocolNum);

  /**
   * \brief Handle credit return packet
   *
   * \param ethHeader Ethernet header
   * \param item Processing item containing the credit packet
   */
  void HandleCreditReturn(const EthernetHeader& ethHeader, const ProcessItem& item);

  /**
   * \brief Get source MAC address from packet
   *
   * \param tempPacket Packet to extract from
   * \param ChangeHead Whether to modify the packet header
   * \return Source MAC address
   */
  Mac48Address GetSourceMac(Ptr<Packet> tempPacket, bool ChangeHead = false);

public:

  /**
   * \brief Enqueue packet to VC queue of specific device
   *
   * \param p2pDev Target device
   * \param packet Packet to enqueue
   */
  void SpecDevEnqueueToVcQueue(Ptr<PointToPointSueNetDevice> p2pDev, Ptr<Packet> packet);

  /**
   * \brief Enqueue packet to appropriate VC queue
   *
   * \param packet Packet to enqueue
   * \return true if successful, false otherwise
   */
  bool EnqueueToVcQueue(Ptr<Packet> packet);
  /**
   * \brief Get the available capacity for a specific VC queue
   *
   * \param vcId Virtual Channel ID
   * \return Available capacity in bytes
   */
  uint32_t GetVcQueueAvailableCapacity(uint8_t vcId);

  /**
   * \brief Reserve capacity for a specific VC queue
   *
   * This method reserves capacity for a pending send operation to prevent
   * race conditions between capacity query and actual send.
   *
   * \param vcId Virtual Channel ID
   * \param amount Amount of capacity to reserve in bytes
   * \return true if reservation successful, false otherwise
   */
  bool ReserveVcCapacity(uint8_t vcId, uint32_t amount);

  /**
   * \brief Release reserved capacity for a specific VC queue
   *
   * This method releases previously reserved capacity after the send
   * operation completes (either successfully or fails).
   *
   * \param vcId Virtual Channel ID
   * \param amount Amount of capacity to release in bytes
   */
  void ReleaseVcCapacity(uint8_t vcId, uint32_t amount);

  /**
   * \brief Update send packet statistics
   *
   * \param packet Packet being sent
   */
  void SendPacketStatistic(Ptr<Packet> packet);

  /**
   * \brief Update receive packet statistics
   *
   * \param packet Packet being received
   */
  void ReceivePacketStatistic(Ptr<Packet> packet);

  /**
   * \brief PPP to Ethernet protocol number mapping
   * \param protocol A PPP protocol number
   * \return The corresponding Ethernet protocol number
   */
  static uint16_t PppToEther (uint16_t protocol);

  /**
   * \brief Ethernet to PPP protocol number mapping
   * \param protocol An Ethernet protocol number
   * \return The corresponding PPP protocol number
   */
  static uint16_t EtherToPpp (uint16_t protocol);

  /**
   * \brief Enumeration of the states of the transmit machine of the net device.
   */
  enum TxMachineState
  {
    READY,   /**< The transmitter is ready to begin transmission of a packet */
    BUSY     /**< The transmitter is busy transmitting a packet */
  };

  /**
   * \brief The Maximum Transmission Unit
   *
   * This corresponds to the maximum
   * number of bytes that can be transmitted as seen from higher layers.
   * This corresponds to the 1500 byte MTU size often seen on IP over
   * Ethernet.
   */
  static const uint16_t DEFAULT_MTU = 1500;

  // Base NetDevice members
  TxMachineState m_txMachineState; //!< Transmit machine state
  DataRate       m_bps;            //!< Data rate
  Time           m_tInterframeGap; //!< Interframe gap
  Ptr<PointToPointSueChannel> m_channel; //!< Attached channel
  Ptr<Queue<Packet> > m_queue;            //!< Transmit queue
  Ptr<ErrorModel> m_receiveErrorModel;    //!< Receive error model
  Ptr<Node> m_node;                       //!< Node owning this NetDevice
  Mac48Address m_address;                 //!< MAC address
  NetDevice::ReceiveCallback m_rxCallback; //!< Receive callback
  NetDevice::PromiscReceiveCallback m_promiscCallback; //!< Promiscuous receive callback
  uint32_t m_ifIndex;                     //!< Interface index
  bool m_linkUp;                          //!< Link up status
  TracedCallback<> m_linkChangeCallbacks; //!< Link change callbacks
  uint32_t m_mtu;                         //!< MTU size
  Ptr<Packet> m_currentPkt;               //!< Current packet being transmitted

  // Trace callbacks
  TracedCallback<Ptr<const Packet> > m_macTxTrace;         //!< Transmit trace
  TracedCallback<Ptr<const Packet> > m_macTxDropTrace;     //!< Transmit drop trace
  TracedCallback<Ptr<const Packet> > m_macPromiscRxTrace;  //!< Promiscuous receive trace
  TracedCallback<Ptr<const Packet> > m_macRxTrace;         //!< Receive trace
  TracedCallback<Ptr<const Packet> > m_macRxDropTrace;     //!< Receive drop trace
  TracedCallback<Ptr<const Packet> > m_phyTxBeginTrace;    //!< PHY transmit begin trace
  TracedCallback<Ptr<const Packet> > m_phyTxEndTrace;      //!< PHY transmit end trace
  TracedCallback<Ptr<const Packet> > m_phyTxDropTrace;     //!< PHY transmit drop trace
  TracedCallback<Ptr<const Packet> > m_phyRxBeginTrace;    //!< PHY receive begin trace
  TracedCallback<Ptr<const Packet> > m_phyRxEndTrace;      //!< PHY receive end trace
  TracedCallback<Ptr<const Packet> > m_phyRxDropTrace;     //!< PHY receive drop trace
  TracedCallback<Ptr<const Packet> > m_snifferTrace;       //!< Sniffer trace
  TracedCallback<Ptr<const Packet> > m_promiscSnifferTrace; //!< Promiscuous sniffer trace

  // SUE-specific members
  bool m_cbfcInitialized; //!< CBFC initialization flag
  std::map<Mac48Address, std::map<uint8_t, uint32_t>> m_txCreditsMap; //!< TX credits: MAC -> VC -> credits
  std::map<Mac48Address, std::map<uint8_t, uint32_t>> m_rxCreditsToReturnMap; //!< RX credits to return: MAC -> VC -> credits
  std::map<uint8_t, Ptr<Queue<Packet>>> m_vcQueues; //!< Virtual channel queues
  std::map<uint8_t, uint32_t> m_vcReservedCapacity; //!< VC reserved capacity for pending sends
  uint32_t m_initialCredits;        //!< Initial credit count
  uint8_t m_numVcs;                 //!< Number of virtual channels
  uint32_t m_creditBatchSize;       //!< Credit batch size
  uint32_t m_vcQueueMaxBytes;       //!< VC queue maximum bytes
  uint32_t m_additionalHeaderSize;  //!< Additional header size for capacity reservation

  // Processing queue
  std::queue<ProcessItem> m_processingQueue; //!< Processing queue
  uint32_t m_currentProcessingQueueSize;     //!< Current processing queue size (packets)
  uint32_t m_currentProcessingQueueBytes;    //!< Current processing queue size (bytes)
  bool m_isProcessing;                       //!< Processing flag
  Time m_processingDelay;                    //!< Processing delay
  uint32_t m_processingQueueMaxBytes;        //!< Processing queue maximum bytes

  // Statistics
  std::map<uint8_t, uint64_t> m_vcBytesSent;      //!< VC bytes sent
  std::map<uint8_t, uint64_t> m_vcBytesReceived;  //!< VC bytes received
  Time m_lastStatTime;                             //!< Last statistics time
  Time m_linkStatInterval;                         //!< Statistics interval
  bool m_enableLinkCBFC;                           //!< CBFC enable flag
  std::map<uint8_t, uint32_t> m_vcDropCounts;      //!< VC drop counts
  std::map<uint8_t, uint32_t> m_vcDropCounts_SendQ; //!< VC send queue drop counts
  uint32_t m_totalPacketDropNum;                   //!< Total packet drop count

  // Timing parameters
  Time m_creUpdateAddHeadDelay;  //!< Credit update header addition delay
  Time m_dataAddHeadDelay;       //!< Data packet header addition delay
  Time m_creditGenerateDelay;    //!< Credit generation delay
  Time m_switchForwardDelay;     //!< Switch forward delay
  Time m_vcSchedulingDelay;      //!< VC scheduling delay

  // Event and logging
  EventId m_logStatisticsEvent;  //!< Statistics logging event
  bool m_loggingEnabled;         //!< Logging enabled flag
  DataRate m_processingRate;     //!< Processing rate
  std::string m_processingRateString; //!< Processing rate string for compatibility
  std::string m_linkStatIntervalString; //!< Link statistic interval string for compatibility

  // Switch functionality moved to SueSwitch module
  Ptr<SueSwitch> m_switch; //!< Switch module for Layer 2 forwarding

  // Global mappings
  static std::map<Ipv4Address, Mac48Address> s_ipToMacMap; //!< Global IP to MAC mapping

  /// ---- LLR members ----
  static const uint16_t ACK_REV = 0x1111;  //!< LLR ACK protocol number
  static const uint16_t NACK_REV = 0x2222; //!< LLR NACK protocol number

  /**
   * \brief Structure holding info for a sent packet tracked by LLR
   */

  bool m_llrEnabled;                 //!< Whether LLR is enabled
  uint32_t m_llrWindowSize;          //!< LLR window size (max outstanding packets per VC)
  std::map<Mac48Address,
           std::vector<std::map<uint32_t, Ptr<Packet>>>> m_sendList; //!< Per peer MAC: per VC map seq->packet
  Time m_llrTimeout;                 //!< Retransmission timeout
  Time m_AckAddHeaderDelay;          //!< Delay to add ACK/NACK header
  Time m_AckProcessDelay;            //!< Delay to process received ACK/NACK

  // Sequence tracking
  std::map<Mac48Address,std::vector<uint32_t>> waitSeq;      //!< Per peer MAC: expected next receive sequence per VC
  std::map<Mac48Address,std::vector<uint32_t>> sendSeq;      //!< Per peer MAC: next sequence to send per VC
  std::map<Mac48Address,std::vector<uint32_t>> unack;        //!< Per peer MAC: outstanding unacknowledged sequences per VC
  std::map<Mac48Address,std::vector<uint32_t>> llrResendseq; //!< Per peer MAC: sequences scheduled for resend per VC

  // State flags
  std::map<Mac48Address,std::vector<bool>> llrWait;      //!< Per peer MAC: VC waiting for ACK
  std::map<Mac48Address,std::vector<bool>> llrResending; //!< Per peer MAC: VC currently resending

  // Timing
  std::map<Mac48Address,std::vector<Time>> lastAckedTime; //!< Per peer MAC: last ACK receive time per VC
  std::map<Mac48Address,std::vector<Time>> lastAcksend;   //!< Per peer MAC: last ACK send time per VC

  // Retransmission events
  std::map<Mac48Address,std::vector<EventId>> resendPkt;  //!< Per peer MAC: scheduled resend events per VC

  // LLR operations
  void SendLlrAck(uint8_t vcId, uint32_t seq, Mac48Address dest);
  void SendLlrNack(uint8_t vcId, uint32_t seq, Mac48Address dest);
  void ProcessLlrAck(Ptr<Packet> p);
  void ProcessLlrNack(Ptr<Packet> p);
  void Resend(uint8_t vcId, Mac48Address dest);          //!< Timeout-based resend
  void ResendInSwitch(uint8_t vcId, Mac48Address dest);  //!< Switch internal resend
  void LlrSendPacket(Ptr<Packet> packet, uint8_t vcId, Mac48Address dest); //!< Initial send with sequencing
  void LlrReceivePacket(Ptr<Packet> packet, uint8_t vcId, Mac48Address source, uint32_t seq_rev); //!< Handle received data
  void LlrResendPacket(uint8_t vcId, Mac48Address dest); //!< Perform resend of pending sequences
};

} // namespace ns3

#endif /* POINT_TO_POINT_SUE_NET_DEVICE_H */