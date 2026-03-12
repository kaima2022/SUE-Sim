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

#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include "ns3/core-module.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

namespace ns3 {

// Forward declaration
struct SueSimulationConfig;

/**
 * \brief Utility class for SUE simulation timing and logging operations
 *
 * This class provides functionality for:
 * - Recording simulation start/end times
 * - Calculating execution duration
 * - Formatting time displays
 * - Performance logging setup
 */
class SueUtils
{
public:
    /**
     * \brief Start timing the simulation
     * \return Timing session identifier
     */
    static std::string StartTiming ();

    /**
     * \brief End timing and display results
     * \param sessionId Timing session identifier from StartTiming()
     */
    static void EndTiming (const std::string& sessionId);

    /**
     * \brief Initialize performance logger with CSV output file
     * \param filename Output CSV filename
     */
    static void InitializePerformanceLogger (const std::string& filename);

    /**
     * \brief Configure simulation logging components
     * \param logLevel Log level string ("LOG_LEVEL_DEBUG" or "LOG_LEVEL_INFO")
     */
    static void ConfigureLogging (const std::string& logLevel = "LOG_LEVEL_INFO");

    /**
     * \brief Configure simulation logging components using configuration parameters
     * \param config Simulation configuration containing logging settings
     */
    static void ConfigureLogging (const SueSimulationConfig& config);

    /**
     * \brief Get current timestamp as formatted string
     * \return Formatted timestamp string
     */
    static std::string GetCurrentTimestamp ();

    /**
     * \brief Convert milliseconds to seconds with decimal precision
     * \param milliseconds Duration in milliseconds
     * \return Duration in seconds
     */
    static double MillisecondsToSeconds (int64_t milliseconds);

private:
    /**
     * \brief Structure to store timing session information
     */
    struct TimingSession
    {
        std::string sessionId;                          //!< Session identifier
        std::chrono::high_resolution_clock::time_point startRealTime;  //!< Real start time
        std::chrono::system_clock::time_point startSystemTime;        //!< System start time
    };

    static std::map<std::string, TimingSession> m_activeSessions;   //!< Active timing sessions
};

} // namespace ns3

#endif /* COMMON_UTILS_H */