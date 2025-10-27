#!/bin/bash

# SUE Protocol Network Visualization Launcher Script
# Launches the network visualization analysis for SUE protocol
# Usage: ./run_network_visualizer.sh
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

echo "SUE Protocol Network Visualization Analysis"
echo "=========================================="

# Check Python environment
if ! command -v python3 &> /dev/null; then
    echo "Error: python3 not found"
    exit 1
fi

# Check dependencies
python3 -c "import pandas, matplotlib, seaborn, numpy, pathlib" 2>/dev/null || {
    echo "Error: Missing required Python packages"
    echo "Please run: pip install pandas matplotlib seaborn numpy"
    exit 1
}

# Switch to script directory
cd "$(dirname "$0")"
cd scripts

echo "Working directory: $(pwd)"

# Check input files
if [ $# -eq 0 ]; then
    echo " No file specified, will automatically select the latest performance log file"
    echo "Checking available performance log files:"
    ls ../data/performance_logs/*.csv 2>/dev/null | head -5 || echo "  No performance log files found"
    echo ""
    echo "Using default parameter xpu_count=4"
    # Run script without file parameter, let script auto-select
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
mkdir -p ../results/network_visualizations

echo "Starting network visualization analysis..."

# Run network visualization script
if [ "$auto_mode" = true ]; then
    # Auto mode: don't pass file parameter, script will auto-select latest file
    python3 network_visualizer.py --xpu_count 4
else
    # Manual mode: pass specified file
    python3 network_visualizer.py --Name "$log_file" --xpu_count 4
fi

if [ $? -eq 0 ]; then
    echo ""
    echo "Network visualization analysis completed!"
    echo "View results: ls -la ../results/network_visualizations/"
    echo "View latest results: find ../results/network_visualizations/ -maxdepth 1 -type d | sort | tail -1"
else
    echo "Network visualization analysis failed"
    exit 1
fi