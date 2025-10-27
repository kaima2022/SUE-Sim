#!/bin/bash

# SUE Protocol Queue Usage Analysis Launcher Script
# Generate charts for XPU, Switch, and Link Layer Processing queue usage analysis
# Usage: ./run_queue_usage_analysis.sh
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

echo "Starting enhanced queue usage analysis tool..."

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

# Check if data files exist in new directory structure
DEST_QUEUE_AVAILABLE=false
DEVICE_QUEUE_AVAILABLE=false
PROCESSING_AVAILABLE=false

if ls data/destination_queue_logs/destination_queue_*.csv 1> /dev/null 2>&1; then
    DEST_QUEUE_AVAILABLE=true
    echo "Found destination queue data files"
else
    echo " Destination queue data files not found"
fi

if ls data/device_queue_logs/device_queue_*.csv 1> /dev/null 2>&1; then
    DEVICE_QUEUE_AVAILABLE=true
    echo "Found device queue data files"
else
    echo " Device queue data files not found"
fi

if ls data/processing_queue_logs/processing_queue_*.csv 1> /dev/null 2>&1; then
    PROCESSING_AVAILABLE=true
    echo "Found link layer processing queue data files"
else
    echo " Link layer processing queue data files not found"
    echo "   (Optional) To analyze link layer processing queues, ensure simulation generates this data"
fi

# Check if any data files are available
if [ "$DEST_QUEUE_AVAILABLE" = false ] && [ "$DEVICE_QUEUE_AVAILABLE" = false ]; then
    echo "Error: No queue usage data files found"
    echo "Please run SUE-Sim simulation first to generate data"
    echo "Data files should be located in the following directories:"
    echo "  - data/destination_queue_logs/"
    echo "  - data/device_queue_logs/"
    echo "  - data/processing_queue_logs/"
    exit 1
fi

# Create output directory
mkdir -p results/queue_usage

# Parse command line arguments
XPU_ID=1
DEVICE_ID=1
SWITCH_ID=5
INCLUDE_PROCESSING=true  # Enable link layer processing queue analysis by default
AUTO_DETECT=true       # Enable auto-detection by default

while [[ $# -gt 0 ]]; do
    case $1 in
        --xpu-id)
            XPU_ID="$2"
            AUTO_DETECT=false  # Disable auto-detection when manually specified
            shift 2
            ;;
        --device-id)
            DEVICE_ID="$2"
            AUTO_DETECT=false  # Disable auto-detection when manually specified
            shift 2
            ;;
        --switch-id)
            SWITCH_ID="$2"
            AUTO_DETECT=false  # Disable auto-detection when manually specified
            shift 2
            ;;
        --include-processing)
            INCLUDE_PROCESSING=true
            echo "Enabling link layer processing queue analysis"
            shift
            ;;
        --no-processing)
            INCLUDE_PROCESSING=false
            echo "Disabling link layer processing queue analysis"
            shift
            ;;
        --auto-detect)
            AUTO_DETECT=true
            echo "Enabling auto-detection mode"
            shift
            ;;
        --no-auto-detect)
            AUTO_DETECT=false
            echo " Disabling auto-detection mode"
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --xpu-id N           Specify XPU ID (default: auto-detect first XPU)"
            echo "  --device-id N        Specify device ID (default: auto-detect first device)"
            echo "  --switch-id N        Specify switch ID (default: auto-detect first switch)"
            echo "  --include-processing Include link layer processing queue analysis (default enabled)"
            echo "  --no-processing     Disable link layer processing queue analysis"
            echo "  --auto-detect        Enable auto-detection mode (default enabled)"
            echo "  --no-auto-detect     Disable auto-detection mode"
            echo "  --help, -h            Show this help information"
            echo ""
            echo "Examples:"
            echo "  $0                           # Auto-detect and analyze all queues (recommended)"
            echo "  $0 --xpu-id 2 --device-id 3    # Specify XPU and device"
            echo "  $0 --no-processing           # Analyze basic queues only"
            echo "  $0 --no-auto-detect         # Use default ID values"
            echo ""
            exit 0
            ;;
        *)
            echo "Unknown parameter: $1"
            echo "Use --help to see help information"
            exit 1
            ;;
    esac
done

# Run analysis script
echo "Starting queue usage analysis..."
echo "Analysis configuration:"
echo "  - XPU ID: $XPU_ID $([ "$AUTO_DETECT" = true ] && echo "(auto-detect)" || echo "(manual)")"
echo "  - Device ID: $DEVICE_ID $([ "$AUTO_DETECT" = true ] && echo "(auto-detect)" || echo "(manual)")"
echo "  - Switch ID: $SWITCH_ID $([ "$AUTO_DETECT" = true ] && echo "(auto-detect)" || echo "(manual)")"
echo "  - Link layer processing queue analysis: $([ "$INCLUDE_PROCESSING" = true ] && echo "enabled" || echo "disabled")"
echo "  - Auto-detection mode: $([ "$AUTO_DETECT" = true ] && echo "enabled" || echo "disabled")"
echo ""

# Build command based on options
ANALYSIS_CMD="python3 scripts/queue_usage_analyzer.py \
    --data-dir data \
    --output-dir results/queue_usage \
    --xpu-id $XPU_ID \
    --device-id $DEVICE_ID \
    --switch-id $SWITCH_ID"

if [ "$INCLUDE_PROCESSING" = true ]; then
    ANALYSIS_CMD="$ANALYSIS_CMD --include-processing"
fi

if [ "$AUTO_DETECT" = true ]; then
    ANALYSIS_CMD="$ANALYSIS_CMD --auto-detect"
fi

echo "Executing command: $ANALYSIS_CMD"
$ANALYSIS_CMD

# Check results
if [ $? -eq 0 ]; then
    echo ""
    echo "Enhanced queue usage analysis completed!"

    # Get the latest created timestamp directory
    OUTPUT_DIR=$(find results/queue_usage -maxdepth 1 -type d -name "????????_??????" 2>/dev/null | sort | tail -1)
    if [[ -z "$OUTPUT_DIR" ]]; then
        OUTPUT_DIR="results/queue_usage"
    fi

    echo "Generated charts:"
    ls -la "$OUTPUT_DIR/"*.png 2>/dev/null || echo "Cannot find output directory: $OUTPUT_DIR"
    echo ""
    echo "Basic analysis charts:"
    echo "  - xpu${XPU_ID}_destination_queues.png: XPU ${XPU_ID} all SUE destination queue usage"
    echo "  - xpu${XPU_ID}_device${DEVICE_ID}_queues.png: XPU ${XPU_ID} device ${DEVICE_ID} all VC queue individual utilization"
    # Note: SWITCH_ID here is XpuId, will be converted to switch number (1-based) in filenames
    SWITCH_NUM=$((SWITCH_ID - 4))  # Convert XpuId to switch number (assuming 4 XPUs)
    echo "  - switch${SWITCH_NUM}_device${DEVICE_ID}_queues.png: Switch ${SWITCH_NUM} device ${DEVICE_ID} all VC queue individual utilization"

    if [ "$INCLUDE_PROCESSING" = true ]; then
        echo ""
        echo "Link layer processing queue analysis charts:"
        echo "  - xpu${XPU_ID}_device${DEVICE_ID}_processing_queues.png: Link layer processing queue detailed analysis"
        echo "  - switch${SWITCH_NUM}_device1_processing_queues.png: Switch ${SWITCH_NUM} device 1 processing queue analysis"
    fi

    echo ""
    echo "Output directory: $OUTPUT_DIR"

    # Show data file info
    echo ""
    echo "Data files used:"
    if [ "$DEST_QUEUE_AVAILABLE" = true ]; then
        echo "  - data/destination_queue_logs/destination_queue_*.csv "
    else
        echo "  - data/destination_queue_logs/destination_queue_*.csv (not found)"
    fi
    if [ "$DEVICE_QUEUE_AVAILABLE" = true ]; then
        echo "  - data/device_queue_logs/device_queue_*.csv "
    else
        echo "  - data/device_queue_logs/device_queue_*.csv (not found)"
    fi
    if [ "$PROCESSING_AVAILABLE" = true ]; then
        echo "  - data/processing_queue_logs/processing_queue_*.csv "
    else
        echo "  - data/processing_queue_logs/processing_queue_*.csv (not found)"
    fi
else
    echo "Analysis failed, please check error messages"
fi