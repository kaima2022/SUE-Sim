#!/bin/bash

# SUE Protocol SUE Monitoring Analysis Launcher Script
# Analyzes SUE LoadBalancer monitoring data and buffer queue performance
# Usage: ./run_sue_monitoring_analysis.sh
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

echo "SUE LoadBalancer Monitoring Data Analysis Tool"
echo "=============================================="
echo "Will run: SUE buffer queue analysis"
echo ""
# Note: SUE credit mechanism has been replaced by destination queue space check mechanism
# Credit value analysis is no longer provided

# Check Python environment
if ! command -v python3 &> /dev/null; then
    echo "Error: python3 not found"
    exit 1
fi

# Check required Python packages
echo "Checking Python dependencies..."
python3 -c "import numpy, pandas, matplotlib, seaborn, scipy, datetime, os, pathlib" 2>/dev/null || {
    echo "Error: Missing required Python packages"
    echo "Please run: pip install numpy pandas matplotlib seaborn scipy"
    exit 1
}

# Switch to script directory
cd "$(dirname "$0")"
echo "Working directory: $(pwd)"

# Check data directory
if [ ! -d "data" ]; then
    echo "Error: data directory not found"
    exit 1
fi

# Check SUE monitoring data files
echo "Checking SUE monitoring data files..."
buffer_queue_data_found=false

# Note: SUE credit mechanism has been replaced by destination queue space check mechanism
# No longer check sue_credits.csv file

# Check SUE buffer queue data
if ls data/sue_buffer_queue_logs/sue_buffer_queue_*.csv 1> /dev/null 2>&1; then
    echo "Found SUE buffer queue data files"
    buffer_queue_data_found=true
    echo "  - $(ls data/sue_buffer_queue_logs/sue_buffer_queue_*.csv | head -1)"
else
    echo " Warning: SUE buffer queue data files not found (data/sue_buffer_queue_logs/sue_buffer_queue_*.csv)"
fi

# Create output directory
mkdir -p results/sue_buffer_queue_analysis

echo ""
echo "Environment check completed"
echo "Starting SUE monitoring data analysis..."

# Count successfully running analyses
success_count=0
total_count=1

# 1. Run SUE buffer queue analysis
echo ""
echo "[1/1] SUE buffer queue analysis..."
echo "=================================="

if [ "$buffer_queue_data_found" = true ]; then
    echo "Starting SUE buffer queue analysis..."
    cd scripts
    python3 sue_buffer_queue_analyzer.py
    exit_code=$?
    cd ..

    if [ $exit_code -eq 0 ]; then
        echo "SUE buffer queue analysis completed!"
        ((success_count++))
        echo "Results location: results/sue_buffer_queue_analysis/"
        latest_result=$(find results/sue_buffer_queue_analysis/ -maxdepth 1 -type d -name "????????_??????" | sort | tail -1)
        if [ -n "$latest_result" ]; then
            echo "Latest results: $latest_result"
        fi
    else
        echo "SUE buffer queue analysis failed (exit code: $exit_code), skipping this step"
    fi
else
    echo " Skipping SUE buffer queue analysis (missing data files)"
    echo "Hint: Please ensure SUE-Sim simulation has been run with LoadBalancer buffer queue monitoring enabled"
fi

# Output final summary
echo ""
echo "SUE monitoring data analysis summary"
echo "===================================="
echo "Successfully completed: $success_count/$total_count analyses"

if [ $success_count -eq $total_count ]; then
    echo "All SUE monitoring analyses completed!"
elif [ $success_count -eq 0 ]; then
    echo "All SUE monitoring analyses failed"
    exit 1
else
    echo " Some SUE monitoring analyses completed, some skipped due to missing data or errors"
fi

echo ""
echo "View all SUE monitoring results:"
echo "  - SUE buffer queue analysis: ls -la results/sue_buffer_queue_analysis/"

echo ""
echo "View latest SUE monitoring results:"
buffer_latest=$(find results/sue_buffer_queue_analysis/ -maxdepth 1 -type d -name "????????_??????" | sort | tail -1)

[ -n "$buffer_latest" ] && echo "  - SUE buffer queue: $buffer_latest"

echo ""
echo "Usage tips:"
echo "  - Ensure simulation has SUE monitoring enabled"
echo "  - By default analyzes XPU 1 data, can modify in script to analyze other XPUs"
echo "  - Generated charts include time series, distribution analysis, and statistical summaries"