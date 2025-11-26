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

#ifndef SUE_SERVER_H
#define SUE_SERVER_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/ptr.h"

namespace ns3 {

/**
 * \defgroup sue-server SUE Server Application
 * This section documents the API of the ns-3 sue-server module.
 * For a functional description, please refer to the ns-3 manual.
 */

/**
 * \ingroup sue-server
 * \class SueServer
 * \brief A server application for SUE protocol with packed transaction handling.
 *
 * The SueServer class implements a server application that can receive and process
 * packed transactions from SUE clients. It handles unpacking of bundled transactions,
 * generates acknowledgments, and maintains comprehensive statistics about
 * received packets and unpacked transactions.
 */
class SueServer : public Application
{
public:
  /**
   * \brief Get the TypeId
   *
   * \return The TypeId for this class
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Construct a SueServer
   */
  SueServer ();

  /**
   * \brief Destroy a SueServer
   */
  virtual ~SueServer ();

  /**
   * \brief Set port information for the server
   *
   * \param xpuId XPU identifier
   * \param portId Port identifier
   */
  void SetPortInfo (uint32_t xpuId, uint32_t portId);

protected:
  /**
   * \brief Dispose of the object
   */
  void DoDispose (void) override;

private:
  /**
   * \brief Copy constructor (disabled)
   *
   * \param other Other SueServer instance
   */
  SueServer (const SueServer &other);

  /**
   * \brief Assignment operator (disabled)
   *
   * \param other Other SueServer instance
   * \return Reference to this instance
   */
  SueServer& operator= (const SueServer &other);

  /**
   * \brief Application specific startup method
   */
  void StartApplication (void) override;

  /**
   * \brief Application specific shutdown method
   */
  void StopApplication (void) override;

  /**
   * \brief Handle received packets
   *
   * \param socket Socket on which data was received
   */
  void HandleRead (Ptr<Socket> socket);

  uint16_t m_port;                    //!< Server port number
  Ptr<Socket> m_socket;               //!< Server socket
  uint32_t m_packetsReceived;         //!< Number of packets received
  uint32_t m_transactionsUnpacked;    //!< Number of unpacked transactions
  uint32_t m_nodeId;                  //!< Node identifier
  uint32_t m_xpuId;                   //!< XPU identifier
  uint32_t m_portId;                  //!< Port identifier
  uint32_t m_transactionSize;         //!< Transaction size in bytes
};

} // namespace ns3

#endif /* SUE_SERVER_H */