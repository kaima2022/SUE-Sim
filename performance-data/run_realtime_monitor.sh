#!/bin/bash

# SUE Protocol Real-time Queue Monitoring Launcher Script
# Launches real-time queue monitoring for SUE protocol
# Usage: ./run_realtime_monitor.sh
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

echo "Starting real-time queue monitoring tool..."

# Check Python environment
if ! command -v python3 &> /dev/null; then
    echo "Error: python3 not found"
    exit 1
fi

# Check required Python packages
echo "Checking Python packages..."
python3 -c "import pandas, matplotlib" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Error: Missing required Python packages"
    echo "Please install: pip install pandas matplotlib"
    exit 1
fi

# Set script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Check data files
echo "Checking data files..."
if [ ! -f "data/packing_logs/destination_queue_usage.csv" ]; then
    echo " Warning: Destination queue data file not found"
    echo "Please run SUE-Sim simulation first to generate data"
else
    echo "Found destination queue data file"
fi

if [ ! -f "data/packing_logs/device_queue_usage.csv" ]; then
    echo " Warning: Device queue data file not found"
    echo "Please run SUE-Sim simulation first to generate data"
else
    echo "Found device queue data file"
fi

# Start real-time monitoring
echo "Starting real-time queue monitoring..."
echo "Charts will display: Destination queue usage + Device main queue usage"
echo " Default update interval: 1000ms"
echo "Press Ctrl+C to stop monitoring"
echo ""

python3 scripts/realtime_queue_monitor.py "$@"