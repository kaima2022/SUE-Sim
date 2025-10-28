/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2008 University of Washington
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

#ifndef POINT_TO_POINT_SUE_CHANNEL_H
#define POINT_TO_POINT_SUE_CHANNEL_H

#include "ns3/channel.h"
#include "ns3/ptr.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/traced-callback.h"

namespace ns3 {

class PointToPointSueNetDevice;
class NetDevice;
class Packet;

/**
 * \ingroup point-to-point-sue
 * \brief Simple Point To Point Channel for SUE enhancements
 *
 * This class represents a simple point-to-point channel that can be used
 * with PointToPointSueNetDevice. It maintains compatibility with the
 * original PointToPointChannel while supporting SUE enhancements.
 */
class PointToPointSueChannel : public Channel
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Construct a PointToPointSueChannel
   */
  PointToPointSueChannel ();

  /**
   * \brief Destroy a PointToPointSueChannel
   */
  virtual ~PointToPointSueChannel ();

  /**
   * \brief Attach a given netdevice to this channel
   * \param device Pointer to the netdevice to attach to the channel
   */
  void Attach (Ptr<PointToPointSueNetDevice> device);

  /**
   * \brief Transmit a packet over this channel
   * \param p Packet to transmit
   * \param src Source PointToPointSueNetDevice
   * \param txTime Transmit time to apply
   * \returns true if successful, false on failure
   */
  bool TransmitStart (Ptr<const Packet> p, Ptr<PointToPointSueNetDevice> src, Time txTime);

  /**
   * \brief Get the net device at the other end of the channel
   * \param device The device at one end of the channel
   * \returns The net device at the other end of the channel
   */
  Ptr<PointToPointSueNetDevice> GetSource (Ptr<PointToPointSueNetDevice> device);

  /**
   * \brief Get the net device at the other end of the channel
   * \param device The device at one end of the channel
   * \returns The net device at the other end of the channel
   */
  Ptr<PointToPointSueNetDevice> GetDestination (Ptr<PointToPointSueNetDevice> device);

  /**
   * \brief Get the delay associated with this channel
   * \returns Time delay
   */
  Time GetDelay (void) const;

  /**
   * \brief Set the delay associated with this channel
   * \param delay Time delay to set
   */
  void SetDelay (Time delay);

  // Channel interface implementation
  virtual std::size_t GetNDevices (void) const;
  virtual Ptr<NetDevice> GetDevice (std::size_t i) const;

  /**
   * \brief Get PointToPointSueNetDevice corresponding to index i on this channel
   * \param i Index number of the device requested
   * \returns Ptr to PointToPointSueNetDevice requested
   */
  Ptr<PointToPointSueNetDevice> GetPointToPointDevice (std::size_t i) const;

protected:

  /**
   * \brief Check to make sure the link is initialized
   * \returns true if initialized, asserts otherwise
   */
  bool IsInitialized (void) const;

  /**
   * \brief Get the net-device source
   * \param i the link requested
   * \returns Ptr to PointToPointSueNetDevice source for the specified link
   */
  Ptr<PointToPointSueNetDevice> GetSource (uint32_t i) const;

  /**
   * \brief Get the net-device destination
   * \param i the link requested
   * \returns Ptr to PointToPointSueNetDevice destination for the specified link
   */
  Ptr<PointToPointSueNetDevice> GetDestination (uint32_t i) const;

  /**
   * \brief TracedCallback signature for transmit and receive events
   *
   * \param p Packet that was transmitted/received
   * \param src Source PointToPointSueNetDevice
   * \param dst Destination PointToPointSueNetDevice
   * \param txTime Transmit time
   * \param rxTime Receive time
   */
  typedef void (* TxRxAnimationCallback)(Ptr<const Packet> p,
                                         Ptr<const NetDevice> src,
                                         Ptr<const NetDevice> dst,
                                         Time txTime,
                                         Time rxTime);

private:
  /** Each point to point link has exactly two net devices. */
  static const std::size_t N_DEVICES = 2;

  /** \brief Wire states */
  enum WireState
  {
    INITIALIZING,
    IDLE,
    TRANSMITTING,
    PROPAGATING
  };

  /**
   * \brief Wire model for the PointToPointSueChannel
   */
  class Link
  {
  public:
    Link() : m_state (INITIALIZING), m_src (0), m_dst (0) {}
    WireState                  m_state; //!< State of the link
    Ptr<PointToPointSueNetDevice> m_src; //!< First net device
    Ptr<PointToPointSueNetDevice> m_dst; //!< Second net device
  };

  /**
   * \brief The trace source for the transmit and receive events
   */
  TracedCallback<Ptr<const Packet>, Ptr<const NetDevice>, Ptr<const NetDevice>, Time, Time> m_txrxPointToPoint;

  Time m_delay;    //!< Propagation delay
  std::size_t m_nDevices; //!< Devices connected to the channel
  Link m_link[N_DEVICES]; //!< Link model

  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse
   */
  PointToPointSueChannel (const PointToPointSueChannel &o);
  /**
   * \brief Assignment operator
   *
   * Defined and unimplemented to avoid misuse
   */
  PointToPointSueChannel &operator = (const PointToPointSueChannel &o);

  // Link model already defined as m_link[N_DEVICES] above
};

} // namespace ns3

#endif /* POINT_TO_POINT_SUE_CHANNEL_H */