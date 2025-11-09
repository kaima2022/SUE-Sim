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

#include "traffic-generator-config.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/sue-utils.h"
#include "ns3/packet.h"
#include <algorithm>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("ConfigurableTrafficGenerator");

NS_OBJECT_ENSURE_REGISTERED (ConfigurableTrafficGenerator);

TypeId
ConfigurableTrafficGenerator::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::ConfigurableTrafficGenerator")
    .SetParent<Application> ()
    .SetGroupName ("Applications")
    .AddConstructor<ConfigurableTrafficGenerator> ()
    .AddAttribute ("TransactionSize",
                   "The size of each transaction in bytes",
                   UintegerValue (256),
                   MakeUintegerAccessor (&ConfigurableTrafficGenerator::m_transactionSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxBurstSize",
                   "Maximum burst size in bytes",
                   UintegerValue (2048),
                   MakeUintegerAccessor (&ConfigurableTrafficGenerator::m_maxBurstSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("EnableClientCBFC",
                   "Enable application-level CBFC",
                   BooleanValue (true),
                   MakeBooleanAccessor (&ConfigurableTrafficGenerator::m_enableClientCBFC),
                   MakeBooleanChecker ())
    .AddAttribute ("AppInitCredit",
                   "Application-level initial credit",
                   UintegerValue (100),
                   MakeUintegerAccessor (&ConfigurableTrafficGenerator::m_appInitCredit),
                   MakeUintegerChecker<uint32_t> ());
  return tid;
}

ConfigurableTrafficGenerator::ConfigurableTrafficGenerator ()
  : m_transactionSize (256),
    m_localXpuId (0),
    m_maxBurstSize (2048),
    m_enableClientCBFC (true),
    m_appInitCredit (100),
    m_transmissionComplete (false),
    m_psn (0),
    m_currentTime (0),
    m_generationPaused (false)
{
  NS_LOG_FUNCTION (this);
  m_rand = CreateObject<UniformRandomVariable> ();
}

ConfigurableTrafficGenerator::~ConfigurableTrafficGenerator ()
{
  NS_LOG_FUNCTION (this);
}

void
ConfigurableTrafficGenerator::SetLoadBalancer (Ptr<LoadBalancer> loadBalancer)
{
  NS_LOG_FUNCTION (this << loadBalancer);
  m_loadBalancer = loadBalancer;
}

void
ConfigurableTrafficGenerator::SetTransactionSize (uint32_t size)
{
  NS_LOG_FUNCTION (this << size);
  m_transactionSize = size;
}

void
ConfigurableTrafficGenerator::SetLocalXpuId (uint32_t localXpuId)
{
  NS_LOG_FUNCTION (this << localXpuId);
  m_localXpuId = localXpuId;
}

void
ConfigurableTrafficGenerator::SetFineGrainedFlows (const std::vector<FineGrainedTrafficFlow>& flows)
{
  NS_LOG_FUNCTION (this << flows.size ());
  m_fineGrainedFlows = flows;
}

bool
ConfigurableTrafficGenerator::CheckTransmissionComplete (void) const
{
  return m_transmissionComplete;
}

uint64_t
ConfigurableTrafficGenerator::GetRemainingBytes (void) const
{
  uint64_t totalRemaining = 0;
  for (const auto& state : m_flowStates)
    {
      if (state.isActive)
        {
          totalRemaining += m_transactionSize;
        }
    }
  return totalRemaining;
}

bool
ConfigurableTrafficGenerator::GetEnableClientCBFC (void) const
{
  return m_enableClientCBFC;
}

void
ConfigurableTrafficGenerator::PauseGeneration ()
{
  NS_LOG_FUNCTION (this);
  m_generationPaused = true;
  if (m_generateEvent.IsPending ())
    {
      Simulator::Cancel (m_generateEvent);
    }
}

void
ConfigurableTrafficGenerator::ResumeGeneration ()
{
  NS_LOG_FUNCTION (this);
  m_generationPaused = false;
  if (!m_transmissionComplete)
    {
      ScheduleNextTransaction ();
    }
}

bool
ConfigurableTrafficGenerator::IsGenerationPaused () const
{
  return m_generationPaused;
}

void
ConfigurableTrafficGenerator::StartApplication (void)
{
  NS_LOG_FUNCTION (this);

  m_transmissionComplete = false;
  m_currentTime = 0;
  m_psn = 0;
  m_generationPaused = false;

  // Initialize active flows for this XPU
  InitializeActiveFlows ();

  // Schedule first transaction if there are active flows
  if (!m_activeFlowIndices.empty ())
    {
      ScheduleNextTransaction ();
      NS_LOG_INFO ("ConfigurableTrafficGenerator started for XPU" << m_localXpuId
                  << " with " << m_activeFlowIndices.size () << " active flows");
    }
  else
    {
      NS_LOG_INFO ("ConfigurableTrafficGenerator started for XPU" << m_localXpuId
                  << " but no active flows found");
      m_transmissionComplete = true;
    }
}

void
ConfigurableTrafficGenerator::StopApplication (void)
{
  NS_LOG_FUNCTION (this);

  if (m_generateEvent.IsPending ())
    {
      Simulator::Cancel (m_generateEvent);
    }

  m_transmissionComplete = true;
  NS_LOG_INFO ("ConfigurableTrafficGenerator stopped for XPU" << m_localXpuId);
}

void
ConfigurableTrafficGenerator::InitializeActiveFlows ()
{
  NS_LOG_FUNCTION (this);

  m_flowStates.clear ();
  m_activeFlowIndices.clear ();

  // Initialize flow states and identify active flows for this XPU
  for (uint32_t i = 0; i < m_fineGrainedFlows.size (); ++i)
    {
      const auto& flow = m_fineGrainedFlows[i];
      FlowState state;

      state.bytesSent = 0;
      state.lastGenerationTime = 0;
      state.isActive = (flow.sourceXpuId == m_localXpuId);
      state.dataRate = DataRate (flow.dataRate);

      // Calculate packet interval based on data rate
      if (flow.dataRate > 0.0)
        {
          double bitsPerPacket = m_transactionSize * 8.0;
          double secondsPerPacket = bitsPerPacket / (flow.dataRate * 1e6);
          state.packetInterval = Seconds (secondsPerPacket);
        }
      else
        {
          state.packetInterval = Seconds (1.0); // Default to 1 second if rate is 0
        }

      m_flowStates.push_back (state);

      if (state.isActive)
        {
          m_activeFlowIndices.push_back (i);
          NS_LOG_INFO ("Active flow " << i << ": XPU" << flow.sourceXpuId
                      << " -> XPU" << flow.destXpuId
                      << " via SUE" << flow.sueId
                      << " at " << flow.dataRate << " Mbps"
                      << " on VC" << (uint32_t)flow.vcId);
        }
    }

  // If no active flows found, stop all SUE logging
  if (m_activeFlowIndices.empty ())
    {
      NS_LOG_INFO ("No active flows found for XPU" << m_localXpuId << ", stopping all SUE logging");

      // Disable SUE logging only when there are no active flows (don't cancel events)
      if (m_loadBalancer)
        {
          m_loadBalancer->DisableSueLoggingOnly ();
        }
    }
}

void
ConfigurableTrafficGenerator::GenerateTransactions ()
{
  NS_LOG_FUNCTION (this);

  if (m_generationPaused || m_transmissionComplete)
    {
      return;
    }

  // Check for flows that need to generate transactions
  for (uint32_t flowIdx : m_activeFlowIndices)
    {
      const auto& flow = m_fineGrainedFlows[flowIdx];
      auto& state = m_flowStates[flowIdx];

      if (!state.isActive)
        {
          continue;
        }

      // Check if this flow has completed its transmission
      if (state.bytesSent >= flow.totalBytes)
        {
          state.isActive = false;
          NS_LOG_INFO ("Flow " << flowIdx << " completed transmission");
          continue;
        }

      // Check if it's time to generate a transaction for this flow
      uint64_t currentTimeNs = Simulator::Now ().GetNanoSeconds ();
      uint64_t nextGenerationTime = state.lastGenerationTime + state.packetInterval.GetNanoSeconds ();

      if (currentTimeNs >= nextGenerationTime)
        {
          GenerateTransactionForFlow (flowIdx);
          state.lastGenerationTime = currentTimeNs;
        }
    }

  // Check if all flows are complete
  bool allFlowsComplete = true;
  for (uint32_t flowIdx : m_activeFlowIndices)
    {
      if (m_flowStates[flowIdx].isActive)
        {
          allFlowsComplete = false;
          break;
        }
    }

  if (allFlowsComplete)
    {
      m_transmissionComplete = true;
      NS_LOG_INFO ("All flows completed for XPU" << m_localXpuId);

      // Stop all SUE logging when all flows are complete
      if (m_loadBalancer)
        {
          m_loadBalancer->StopAllLogging ();
        }
    }
  else
    {
      ScheduleNextTransaction ();
    }
}

void
ConfigurableTrafficGenerator::GenerateTransactionForFlow (uint32_t flowIndex)
{
  NS_LOG_FUNCTION (this << flowIndex);

  const auto& flow = m_fineGrainedFlows[flowIndex];
  auto& state = m_flowStates[flowIndex];

  // Create transaction packet
  Ptr<Packet> packet = Create<Packet> (m_transactionSize);

  // Add SUE header
  AddSueHeader (packet, flow.destXpuId, flow.vcId);

  // Distribute through load balancer with specific SUE ID
  if (m_loadBalancer)
    {
      // Create a custom transaction that specifies the SUE ID
      // This requires extending the LoadBalancer interface to support SUE ID specification
      m_loadBalancer->DistributeTransaction (packet, flow.destXpuId, flow.vcId);
    }

  // Update flow state
  state.bytesSent += m_transactionSize;

  NS_LOG_DEBUG ("Generated transaction for flow " << flowIndex
               << ": XPU" << flow.sourceXpuId << " -> XPU" << flow.destXpuId
               << " via SUE" << flow.sueId << " (bytes sent: " << state.bytesSent << "/" << flow.totalBytes << ")");
}

void
ConfigurableTrafficGenerator::ScheduleNextTransaction ()
{
  NS_LOG_FUNCTION (this);

  if (m_generationPaused || m_transmissionComplete)
    {
      return;
    }

  Time nextEventTime = CalculateNextEventTime ();

  if (nextEventTime.IsPositive ())
    {
      m_generateEvent = Simulator::Schedule (nextEventTime,
                                           &ConfigurableTrafficGenerator::GenerateTransactions,
                                           this);
    }
  else
    {
      NS_LOG_INFO ("No more events to schedule for XPU" << m_localXpuId);
    }
}

Time
ConfigurableTrafficGenerator::CalculateNextEventTime ()
{
  Time minNextEvent = Time::Max ();
  uint64_t currentTimeNs = Simulator::Now ().GetNanoSeconds ();

  for (uint32_t flowIdx : m_activeFlowIndices)
    {
      const auto& state = m_flowStates[flowIdx];

      if (!state.isActive)
        {
          continue;
        }

      uint64_t nextGenerationTime = state.lastGenerationTime + state.packetInterval.GetNanoSeconds ();

      if (nextGenerationTime > currentTimeNs)
        {
          Time nextEvent = NanoSeconds (nextGenerationTime - currentTimeNs);
          if (nextEvent < minNextEvent)
            {
              minNextEvent = nextEvent;
            }
        }
      else
        {
          // Immediate generation needed
          return NanoSeconds (1);
        }
    }

  return (minNextEvent == Time::Max ()) ? Time () : minNextEvent;
}

void
ConfigurableTrafficGenerator::AddSueHeader (Ptr<Packet> packet, uint32_t destXpuId, uint8_t vcId)
{
  NS_LOG_FUNCTION (this << packet << destXpuId << (uint32_t)vcId);

  SueHeader sueHeader;
  sueHeader.SetPsn (m_psn++);
  sueHeader.SetXpuId (destXpuId);
  sueHeader.SetVc (vcId);
  sueHeader.SetOp (0); // Data packet

  packet->AddHeader (sueHeader);
}

} // namespace ns3