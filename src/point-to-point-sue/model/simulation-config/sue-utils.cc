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

#include "sue-utils.h"
#include "ns3/performance-logger.h"
#include <map>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("SueUtils");

// Static member initialization
std::map<std::string, SueUtils::TimingSession> SueUtils::m_activeSessions;

std::string
SueUtils::StartTiming ()
{
    // Generate unique session ID
    static uint32_t sessionCounter = 0;
    std::string sessionId = "session_" + std::to_string(++sessionCounter);

    // Create timing session
    TimingSession session;
    session.sessionId = sessionId;
    session.startRealTime = std::chrono::high_resolution_clock::now();
    session.startSystemTime = std::chrono::system_clock::now();

    // Store session
    m_activeSessions[sessionId] = session;

    // Display start time
    std::time_t startTime = std::chrono::system_clock::to_time_t(session.startSystemTime);
    std::cout << "Simulation START at: "
              << std::put_time(std::localtime(&startTime), "%Y-%m-%d %H:%M:%S")
              << " [Session: " << sessionId << "]" << std::endl;

    return sessionId;
}

void
SueUtils::EndTiming (const std::string& sessionId)
{
    auto it = m_activeSessions.find(sessionId);
    if (it == m_activeSessions.end())
    {
        NS_LOG_WARN("Timing session not found: " << sessionId);
        return;
    }

    const TimingSession& session = it->second;

    // Record end times
    auto endRealTime = std::chrono::high_resolution_clock::now();
    auto endSystemTime = std::chrono::system_clock::now();
    std::time_t endTime = std::chrono::system_clock::to_time_t(endSystemTime);

    // Calculate duration
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endRealTime - session.startRealTime);

    // Display results
    std::cout << "Simulation completed" << std::endl;
    std::cout << "Simulation END at real time: "
              << std::put_time(std::localtime(&endTime), "%Y-%m-%d %H:%M:%S")
              << " [Session: " << sessionId << "]" << std::endl;
    std::cout << "Total real time consumed: " << MillisecondsToSeconds(duration.count())
              << " s" << std::endl;

    // Remove session from active sessions
    m_activeSessions.erase(it);
}

void
SueUtils::InitializePerformanceLogger (const std::string& filename)
{
    PerformanceLogger::GetInstance().Initialize(filename);
}

void
SueUtils::ConfigureLogging ()
{
    // Configure logging components
    LogComponentEnable("SueClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("SueServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("SueSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("TrafficGenerator", LOG_LEVEL_INFO);
    LogComponentEnable("LoadBalancer", LOG_LEVEL_INFO);
    LogComponentDisableAll(LOG_ALL);
}

std::string
SueUtils::GetCurrentTimestamp ()
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

double
SueUtils::MillisecondsToSeconds (int64_t milliseconds)
{
    return static_cast<double>(milliseconds) / 1000.0;
}

} // namespace ns3