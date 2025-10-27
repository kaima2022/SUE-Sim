#!/bin/bash

# SUE Protocol Packet Loss Analysis Launcher Script
# Analyze packet loss patterns and statistics in SUE protocol
# Usage: ./run_loss_analyzer.sh
#
# Copyright 2025 SUE-Sim Contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

echo "SUE Protocol Packet Loss Analysis"
echo "==============================="

# Check Python environment
if ! command -v python3 &> /dev/null; then
    echo "Error: python3 not found"
    exit 1
fi

# Switch to script directory
cd "$(dirname "$0")"
cd scripts

echo "Working directory: $(pwd)"

# Check input file
if [ $# -eq 0 ]; then
    echo "No file specified, will automatically use default log file ../log/sue-sim.log"
    echo "Looking for available log files:"
    find ../log -name "*.log" 2>/dev/null | head -5 || echo "  No log files found"
    echo ""
    echo "Using default log file or latest log file"
    # Run script without file parameter, let script automatically select
    auto_mode=true
else
    log_file="$1"
    # Check if file exists
    if [ ! -f "$log_file" ]; then
        echo "Error: File does not exist: $log_file"
        exit 1
    fi
    echo "Input file: $log_file"
    auto_mode=false
fi

# Create output directory
mkdir -p ../results/loss_analysis

echo "Starting packet loss analysis..."

# Run packet loss analysis script
if [ "$auto_mode" = true ]; then
    # Auto mode: don't pass file parameter, script will automatically select default file
    python3 loss_analyzer.py
else
    # Manual mode: pass specified file
    python3 loss_analyzer.py "$log_file"
fi

if [ $? -eq 0 ]; then
    echo ""
    echo "Packet loss analysis completed!"
    echo "View results: ls -la ../results/loss_analysis/"
    echo "View latest results: find ../results/loss_analysis/ -maxdepth 1 -type d | sort | tail -1"
else
    echo "Packet loss analysis failed"
    exit 1
fi