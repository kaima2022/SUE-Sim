#!/bin/bash

# SUE Protocol Load Balancing Analysis Launcher Script
# Used to start LoadBalancer load balancing effectiveness analysis
# Usage: ./run_load_balance_analysis.sh
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

echo "Starting LoadBalancer load balancing effectiveness analysis..."
echo "=================================================="

# Check Python environment
if ! command -v python3 &> /dev/null; then
    echo "Python3 not found, please install Python3"
    exit 1
fi

# Check required Python packages
echo "Checking Python package dependencies..."
python3 -c "import pandas, matplotlib, seaborn, numpy" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Missing required Python packages, please install:"
    echo "   pip install pandas matplotlib seaborn numpy"
    exit 1
fi

# Switch to script directory
cd "$(dirname "$0")"

# Check data files
echo "Checking data files..."
if ls data/load_balance_logs/load_balance_*.csv 1> /dev/null 2>&1; then
    echo "Found load balance data files"
    ls data/load_balance_logs/load_balance_*.csv | head -3 | sed 's/^/  - /'
else
    echo "No load balance data files found"
    echo "   Please run SUE-Sim simulation first to generate data:"
    echo "   ./waf --run 'scratch/SUE-Sim/SUE-Sim'"
    exit 1
fi

# Results directory will be automatically created by script (with timestamp)

# Run analysis script
echo "Running LoadBalancer analysis script..."
python3 scripts/load_balance_analyzer.py

# Check results
if [ $? -eq 0 ]; then
    echo ""
    echo "LoadBalancer analysis completed!"

    # Find latest results directory
    latest_dir=$(find results/load_balance_analysis -maxdepth 1 -type d -name "????????_??????" | sort | tail -1)

    if [ -n "$latest_dir" ]; then
        echo "Results saved in: $latest_dir"
        echo ""
        echo "Generated charts:"
        ls -la "$latest_dir"/*.png 2>/dev/null | while read line; do
            filename=$(basename "$line")
            echo "   - $filename"
        done
        echo ""
        echo "Analysis reports:"
        ls -la "$latest_dir"/*.txt 2>/dev/null | while read line; do
            filename=$(basename "$line")
            echo "   - $filename"
        done
    else
        echo "No results directory found"
    fi
else
    echo "Analysis script execution failed"
    exit 1
fi