#!/bin/bash

# SUE Protocol Complete Analysis Launcher Script
# Run all SUE protocol analysis functions including performance metrics, network visualization, packet loss analysis, load balancing, queue usage, monitoring, XPU delay, and credit analysis
# Usage: ./run_analysis.sh
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

# Note: Removed 'set -e' to allow script to continue even if individual analyses fail

echo "SUE Protocol Complete Performance Data Analysis Platform"
echo "======================================================"
echo "Will run sequentially: Performance Metrics Analysis → Network Visualization Analysis → Packet Loss Analysis → Load Balance Analysis → Queue Usage Analysis → SUE Monitoring Analysis → XPU Delay Analysis → Link Layer Credit Analysis"
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
if [ ! -d "data" ]; then
    echo "Error: data directory not found"
    exit 1
fi

# Check various data files
echo "Checking data files..."
packing_data_found=false
performance_data_found=false
log_data_found=false
load_balance_data_found=false
xpu_delay_data_found=false
credit_data_found=false

# Check packing data
if ls data/wait_time_logs/wait_time_*.csv 1> /dev/null 2>&1 || ls data/pack_num_logs/pack_num_*.csv 1> /dev/null 2>&1; then
    echo "Found packing data files"
    packing_data_found=true
    if ls data/wait_time_logs/wait_time_*.csv 1> /dev/null 2>&1; then
        echo "  - $(ls data/wait_time_logs/wait_time_*.csv | head -1)"
    fi
    if ls data/pack_num_logs/pack_num_*.csv 1> /dev/null 2>&1; then
        echo "  - $(ls data/pack_num_logs/pack_num_*.csv | head -1)"
    fi
else
    echo " Warning: Packing data files not found (data/wait_time_logs/wait_time_*.csv, data/pack_num_logs/pack_num_*.csv)"
fi

# Check performance log data
if ls data/performance_logs/performance*.csv 1> /dev/null 2>&1; then
    echo "Found performance log files"
    performance_data_found=true
    ls data/performance_logs/performance*.csv | head -3 | sed 's/^/  - /'
else
    echo " Warning: Performance log files not found (data/performance_logs/performance*.csv)"
fi

# Check log files
if ls ../log/*.log 1> /dev/null 2>&1; then
    echo "Found log files"
    log_data_found=true
    ls ../log/*.log | head -3 | sed 's/^/  - /'
else
    echo " Warning: Log files not found (../log/*.log)"
fi

# Check load balance data
if ls data/load_balance_logs/load_balance_*.csv 1> /dev/null 2>&1; then
    echo "Found load balance data files"
    load_balance_data_found=true
    ls data/load_balance_logs/load_balance_*.csv | head -3 | sed 's/^/  - /'
else
    echo " Warning: Load balance data files not found (data/load_balance_logs/load_balance_*.csv)"
fi

# Check queue usage data
queue_usage_data_found=false
if ls data/destination_queue_logs/destination_queue_*.csv 1> /dev/null 2>&1 && ls data/device_queue_logs/device_queue_*.csv 1> /dev/null 2>&1; then
    echo "Found queue usage data files"
    queue_usage_data_found=true
    echo "  - Destination queue files: $(ls data/destination_queue_logs/destination_queue_*.csv | head -1)"
    echo "  - Device queue files: $(ls data/device_queue_logs/device_queue_*.csv | head -1)"
else
    echo " Warning: Complete queue usage data files not found"
    if ls data/destination_queue_logs/destination_queue_*.csv 1> /dev/null 2>&1; then
        echo "  - Found destination queue files: $(ls data/destination_queue_logs/destination_queue_*.csv | head -1)"
    fi
    if ls data/device_queue_logs/device_queue_*.csv 1> /dev/null 2>&1; then
        echo "  - Found device queue files: $(ls data/device_queue_logs/device_queue_*.csv | head -1)"
    fi
fi

# Check SUE monitoring data
sue_buffer_queue_data_found=false

# Note: SUE credit mechanism has been replaced by destination queue space check mechanism
# No longer checking sue_credits.csv file

if [ -f "data/packing_logs/sue_buffer_queue.csv" ]; then
    echo "Found SUE buffer queue data file"
    sue_buffer_queue_data_found=true
    echo "  - data/packing_logs/sue_buffer_queue.csv"
else
    echo " Warning: SUE buffer queue data file not found (data/packing_logs/sue_buffer_queue.csv)"
fi

# Check XPU delay data
if ls data/xpu_delay_logs/xpu_delay_*.csv 1> /dev/null 2>&1; then
    echo "Found XPU delay data files"
    xpu_delay_data_found=true
    ls data/xpu_delay_logs/xpu_delay_*.csv | head -3 | sed 's/^/  - /'
else
    echo " Warning: XPU delay data files not found (data/xpu_delay_logs/xpu_delay_*.csv)"
fi

# Check link layer credit data
if ls data/link_credit_logs/link_credit_*.csv 1> /dev/null 2>&1; then
    echo "Found link layer credit data files"
    credit_data_found=true
    ls data/link_credit_logs/link_credit_*.csv | head -3 | sed 's/^/  - /'
else
    echo " Warning: Link layer credit data files not found (data/link_credit_logs/link_credit_*.csv)"
    echo "  Note: Will attempt to extract credit data from performance.csv (backward compatibility)"
fi

# Create all output directories
mkdir -p results/performance_metrics
mkdir -p results/network_visualizations
mkdir -p results/loss_analysis
mkdir -p results/load_balance_analysis
mkdir -p results/queue_usage
mkdir -p results/sue_buffer_queue_analysis
mkdir -p results/xpu_delay_analysis
mkdir -p results/credit_analysis

echo ""
echo "Environment check completed"
echo "Starting complete analysis workflow..."

# Count successfully running analyses
success_count=0
total_count=8

# 1. Run performance metrics analysis
echo ""
echo "[1/8] Performance Metrics Analysis..."
echo "================================"

if [ "$packing_data_found" = true ]; then
    echo "Starting performance metrics analysis..."
    cd scripts
    python3 performance_metrics_analyzer.py
    exit_code=$?
    cd ..

    if [ $exit_code -eq 0 ]; then
        echo "Performance metrics analysis completed!"
        ((success_count++))
        echo "Results location: results/performance_metrics/"
        latest_result=$(find results/performance_metrics/ -maxdepth 1 -type d | sort | tail -1)
        echo "Latest result: $latest_result"
    else
        echo "Performance metrics analysis failed (exit code: $exit_code), skipping this step"
    fi
else
    echo " Skipping performance metrics analysis (missing data files)"
fi

# 2. Run network visualization analysis
echo ""
echo "[2/8] Network Visualization Analysis..."
echo "================================"

if [ "$performance_data_found" = true ]; then
    echo "Starting network visualization analysis..."
    ./run_network_visualizer.sh
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo "Network visualization analysis completed!"
        ((success_count++))
        echo "Results location: results/network_visualizations/"
        latest_result=$(find results/network_visualizations/ -maxdepth 1 -type d | sort | tail -1)
        echo "Latest result: $latest_result"
    else
        echo "Network visualization analysis failed (exit code: $exit_code), skipping this step"
    fi
else
    echo " Skipping network visualization analysis (missing data files)"
fi

# 3. Run packet loss analysis
echo ""
echo "[3/8] Packet Loss Analysis..."
echo "================================"

# Check if there are valid application runtime log files (not compilation logs)
app_log_found=false
if [ -f "../log/sue-sim.log" ]; then
    # Check if log file contains application runtime data
    if grep -q "Summary: Sent" ../log/sue-sim.log || grep -q "Received" ../log/sue-sim.log; then
        app_log_found=true
        echo "Found valid application log file"
    else
        echo " sue-sim.log exists but does not contain application runtime data (might be compilation log)"
    fi
fi

# Search for other possible application logs
if [ "$app_log_found" = false ]; then
    echo "Searching for other application log files..."
    for log_file in ../log/*.log; do
        if [ -f "$log_file" ]; then
            if grep -q "Summary: Sent\|Received\|Dropped" "$log_file"; then
                echo "Found valid application log: $(basename "$log_file")"
                app_log_found=true
                break
            fi
        fi
    done
fi

if [ "$app_log_found" = true ]; then
    echo "Starting packet loss analysis..."
    ./run_loss_analyzer.sh
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo "Packet loss analysis completed!"
        ((success_count++))
        echo "Results location: results/loss_analysis/"
        latest_result=$(find results/loss_analysis/ -maxdepth 1 -type d | sort | tail -1)
        echo "Latest result: $latest_result"
    else
        echo "Packet loss analysis failed (exit code: $exit_code), skipping this step"
        echo "Note: Log file format might not match, please ensure log contains send/receive packet data"
    fi
else
    echo " Skipping packet loss analysis (no valid application runtime log files found)"
    echo "Note: Please ensure log files contain application runtime data like 'Summary: Sent', 'Received', 'Dropped'"
fi

# 4. Run load balance analysis
echo ""
echo "[4/8] Load Balance Analysis..."
echo "================================"

if [ "$load_balance_data_found" = true ]; then
    echo "Starting load balance analysis..."
    ./run_load_balance_analysis.sh
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo "Load balance analysis completed!"
        ((success_count++))
        echo "Results location: results/load_balance_analysis/"
        latest_result=$(find results/load_balance_analysis/ -maxdepth 1 -type d | sort | tail -1)
        echo "Latest result: $latest_result"
    else
        echo "Load balance analysis failed (exit code: $exit_code), skipping this step"
        echo "Note: load_balance.csv file format might not match or Python dependencies missing"
    fi
else
    echo "Skipping load balance analysis (missing data files)"
    echo "Note: Please ensure SUE-Sim simulation has been run with LoadBalancer logging enabled"
fi

# 5. Run queue usage analysis
echo ""
echo "[5/8] Queue Usage Analysis..."
echo "================================"

if [ "$queue_usage_data_found" = true ]; then
    echo "Starting queue usage analysis..."
    ./run_queue_usage_analysis.sh
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo "Queue usage analysis completed!"
        ((success_count++))
        echo "Results location: results/queue_usage/"
        latest_result=$(find results/queue_usage/ -maxdepth 1 -type d -name "????????_??????" | sort | tail -1)
        echo "Latest result: $latest_result"
    else
        echo "Queue usage analysis failed (exit code: $exit_code), skipping this step"
        echo "Note: Queue usage data file format might not match or Python dependencies missing"
    fi
else
    echo "Skipping queue usage analysis (missing data files)"
    echo "Note: Please ensure SUE-Sim simulation has been run with queue usage logging enabled"
fi

# 6. Run XPU delay analysis
echo ""
echo " [6/8] XPU Delay Analysis..."
echo "==========================="

if [ "$xpu_delay_data_found" = true ]; then
    echo "Starting XPU delay analysis..."
    cd scripts
    python3 xpu_delay_analyzer.py
    exit_code=$?
    cd ..

    if [ $exit_code -eq 0 ]; then
        echo "XPU delay analysis completed!"
        ((success_count++))
        echo "Results location: results/xpu_delay_analysis/"
        latest_result=$(find results/xpu_delay_analysis/ -maxdepth 1 -type d -name "????????_??????" | sort | tail -1)
        echo "Latest result: $latest_result"
    else
        echo "XPU delay analysis failed (exit code: $exit_code), skipping this step"
        echo "Note: XPU delay data file format might not match or Python dependencies missing"
    fi
else
    echo "Skipping XPU delay analysis (missing data files)"
    echo "Note: Please ensure SUE-Sim simulation has been run with XPU delay logging enabled"
fi

# 7. Run SUE monitoring analysis
echo ""
echo "[7/8] SUE Buffer Queue Analysis..."
echo "==========================="

sue_monitoring_success=false

if [ "$sue_buffer_queue_data_found" = true ]; then
    echo "Starting SUE buffer queue analysis..."
    cd scripts
    python3 sue_buffer_queue_analyzer.py
    exit_code=$?
    cd ..

    if [ $exit_code -eq 0 ]; then
        echo "SUE buffer queue analysis completed!"
        ((success_count++))
        sue_monitoring_success=true
        echo "Results location: results/sue_buffer_queue_analysis/"
    else
        echo "SUE buffer queue analysis failed (exit code: $exit_code), skipping this step"
        echo "Note: SUE monitoring data file format might not match or Python dependencies missing"
    fi
else
    echo " Skipping SUE monitoring analysis (missing data files)"
    echo "Note: Please ensure SUE-Sim simulation has been run"
fi

# 8. Run link layer credit analysis
echo ""
echo "[8/8] Link Layer Credit Analysis..."
echo "==========================="

# Credit data can be obtained in two ways: separate files or extracted from performance.csv
credit_analysis_possible=false

if [ "$credit_data_found" = true ] || [ "$performance_data_found" = true ]; then
    credit_analysis_possible=true
    echo "Starting link layer credit analysis..."
    ./run_credit_analysis.sh
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo "Link layer credit analysis completed!"
        ((success_count++))
        echo "Results location: results/credit_analysis/"
        latest_result=$(find results/credit_analysis/ -maxdepth 1 -type d -name "????????_??????" | sort | tail -1)
        echo "Latest result: $latest_result"
    else
        echo "Link layer credit analysis failed (exit code: $exit_code), skipping this step"
        echo "Note: Credit data file format might not match or Python dependencies missing"
    fi
else
    echo "Skipping link layer credit analysis (missing data files)"
    echo "Note: Please ensure SUE-Sim simulation has been run with credit logging enabled"
fi

# Output final summary
echo ""
echo "Complete Analysis Workflow Summary"
echo "==================="
echo "Successfully completed: $success_count/$total_count analyses"

if [ $success_count -eq $total_count ]; then
    echo "All analyses completed successfully!"
elif [ $success_count -eq 0 ]; then
    echo "All analyses failed"
    exit 1
else
    echo " Partial analyses completed, some skipped due to missing data or errors"
fi

echo ""
echo "View all results:"
echo "  - Performance Metrics Analysis: ls -la results/performance_metrics/"
echo "  - Network Visualization:   ls -la results/network_visualizations/"
echo "  - Packet Loss Analysis:      ls -la results/loss_analysis/"
echo "  - Load Balance Analysis: ls -la results/load_balance_analysis/"
echo "  - Queue Usage Analysis: ls -la results/queue_usage/"
echo "  - XPU Delay Analysis: ls -la results/xpu_delay_analysis/"
echo "  - SUE Buffer Queue Analysis: ls -la results/sue_buffer_queue_analysis/"
echo "  - Link Layer Credit Analysis: ls -la results/credit_analysis/"

echo ""
echo "View latest results:"
[ -d "results/performance_metrics" ] && echo "  - Performance Metrics: $(find results/performance_metrics/ -maxdepth 1 -type d | sort | tail -1)"
[ -d "results/network_visualizations" ] && echo "  - Network Visualization: $(find results/network_visualizations/ -maxdepth 1 -type d | sort | tail -1)"
[ -d "results/loss_analysis" ] && echo "  - Packet Loss Analysis: $(find results/loss_analysis/ -maxdepth 1 -type d | sort | tail -1)"
[ -d "results/load_balance_analysis" ] && echo "  - Load Balance: $(find results/load_balance_analysis/ -maxdepth 1 -type d | sort | tail -1)"
[ -d "results/queue_usage" ] && echo "  - Queue Usage: $(find results/queue_usage/ -maxdepth 1 -type d -name "????????_??????" | sort | tail -1)"
[ -d "results/xpu_delay_analysis" ] && echo "  - XPU Delay Analysis: $(find results/xpu_delay_analysis/ -maxdepth 1 -type d -name "????????_??????" | sort | tail -1)"
[ -d "results/sue_buffer_queue_analysis" ] && echo "  - SUE Buffer Queue: $(find results/sue_buffer_queue_analysis/ -maxdepth 1 -type d -name "????????_??????" | sort | tail -1)"
[ -d "results/credit_analysis" ] && echo "  - Link Layer Credit Analysis: $(find results/credit_analysis/ -maxdepth 1 -type d -name "????????_??????" | sort | tail -1)"