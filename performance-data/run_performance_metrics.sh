#!/bin/bash

# SUE Protocol Performance Metrics Analysis Launcher Script
# Launches the performance metrics analysis for SUE protocol
# Usage: ./run_performance_metrics.sh
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

echo "SUE Protocol Performance Metrics Analysis"
echo "========================================="

# Check Python environment
if ! command -v python3 &> /dev/null; then
    echo "Error: python3 not found"
    exit 1
fi

# Check required Python packages
python3 -c "import numpy, pandas, matplotlib, seaborn, scipy, datetime, os" 2>/dev/null || {
    echo "Error: Missing required Python packages"
    echo "Please run: pip install numpy pandas matplotlib seaborn scipy"
    exit 1
}

# Switch to script directory
cd "$(dirname "$0")"
cd scripts

echo "Working directory: $(pwd)"

# Check data files
data_dir="../data/packing_logs"
if [ ! -f "$data_dir/wait_time.csv" ]; then
    echo " Warning: Wait time data file not found: $data_dir/wait_time.csv"
fi

if [ ! -f "$data_dir/pack_num.csv" ]; then
    echo " Warning: Pack number data file not found: $data_dir/pack_num.csv"
fi

if [ ! -f "$data_dir/wait_time.csv" ] && [ ! -f "$data_dir/pack_num.csv" ]; then
    echo "Error: No data files found"
    echo "Please ensure the following files exist:"
    echo "  - $data_dir/wait_time.csv"
    echo "  - $data_dir/pack_num.csv"
    exit 1
fi

# Create output directory
mkdir -p ../results/performance_metrics

echo "Starting performance metrics analysis..."

# Run performance metrics analysis script
python3 performance_metrics_analyzer.py

if [ $? -eq 0 ]; then
    echo ""
    echo "Performance metrics analysis completed!"
    echo "View results: ls -la ../results/performance_metrics/"
    echo "View latest results: find ../results/performance_metrics/ -maxdepth 1 -type d | sort | tail -1"
    echo "View analysis report: find ../results/performance_metrics/ -name \"analysis_summary.txt\" | xargs cat"
else
    echo "Performance metrics analysis failed"
    exit 1
fi