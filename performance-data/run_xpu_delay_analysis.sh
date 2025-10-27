#!/bin/bash

# SUE Protocol XPU Delay Analysis Launcher Script
# Analyzes XPU end-to-end delay statistical performance
# Usage: ./run_xpu_delay_analysis.sh
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

echo "XPU Delay Analysis"
echo "=================="
echo "Analyzing XPU end-to-end delay statistical performance"
echo ""

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
if [ ! -d "data/xpu_delay_logs" ]; then
    echo "Error: XPU delay data directory not found (data/xpu_delay_logs)"
    exit 1
fi

# Check XPU delay data files
echo "Checking XPU delay data files..."
xpu_delay_data_found=false

if ls data/xpu_delay_logs/xpu_delay_*.csv 1> /dev/null 2>&1; then
    echo "Found XPU delay data files"
    xpu_delay_data_found=true
    ls data/xpu_delay_logs/xpu_delay_*.csv | head -3 | sed 's/^/  - /'
else
    echo "Error: XPU delay data files not found (data/xpu_delay_logs/xpu_delay_*.csv)"
    echo "Hint: Please ensure SUE-Sim simulation has been run with XPU delay recording enabled"
    exit 1
fi

# Create output directory
mkdir -p results/xpu_delay_analysis

echo ""
echo "Environment check completed"
echo "Starting XPU delay analysis..."

# Run XPU delay analysis
if [ "$xpu_delay_data_found" = true ]; then
    echo "Starting XPU delay analysis..."
    cd scripts
    python3 xpu_delay_analyzer.py
    exit_code=$?
    cd ..

    if [ $exit_code -eq 0 ]; then
        echo ""
        echo "XPU delay analysis completed!"
        echo "Results location: results/xpu_delay_analysis/"
        latest_result=$(find results/xpu_delay_analysis/ -maxdepth 1 -type d -name "????????_??????" | sort | tail -1)
        echo "Latest results: $latest_result"
        echo ""
        echo "Generated charts include:"
        echo "  - Delay overview charts (histogram, box plot, PDF, CDF)"
        echo "  - Delay time series charts"
        echo "  - XPU-port delay heatmaps"
        echo "  - Tail delay analysis charts"
        echo "  - Statistical data tables"
    else
        echo "XPU delay analysis failed (exit code: $exit_code)"
        echo "Hint: Possible XPU delay data file format mismatch or missing Python dependencies"
        exit 1
    fi
fi

echo ""
echo "View result files:"
echo "  - List results directory: ls -la results/xpu_delay_analysis/"
echo "  - View latest results: ls -la $latest_result"
echo ""
echo "View analysis report:"
echo "  - Analysis summary: cat $latest_result/analysis_summary.txt"
echo ""
echo "✨ XPU delay analysis completed!"