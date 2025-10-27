#!/bin/bash

# SUE Protocol Link Layer Credit Analysis Launcher Script
# Automatically use the latest credit data files for analysis
# Usage: ./run_credit_analysis.sh
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

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Enter scripts directory
cd "$SCRIPT_DIR/scripts"

# Check Python environment
if ! command -v python3 &> /dev/null; then
    echo "Error: python3 command not found, please ensure Python 3 is installed"
    exit 1
fi

# Check required Python packages
echo "Checking Python environment..."
python3 -c "import pandas, matplotlib, seaborn, pathlib" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Warning: Some Python packages may be missing, recommended to run:"
    echo "pip install pandas matplotlib seaborn pathlib"
fi

echo "Starting link layer credit analysis..."
echo "================================================="

# Check data directory
if [ ! -d "../data/link_credit_logs" ] && [ ! -d "../data/performance_logs" ]; then
    echo "Error: Data directory not found, please run SUE-Sim simulation first to generate data"
    exit 1
fi

# Run credit analysis script
echo "Running credit analysis script..."
python3 credit_analyzer.py --xpu_count 4

if [ $? -eq 0 ]; then
    echo ""
    echo "================================================="
    echo "Link layer credit analysis completed!"
    echo ""
    echo "Results saved in: ../results/credit_analysis/"
    echo "Including:"
    echo "- Credit value change charts"
    echo "- Switch credit value change charts"
    echo "- Credit statistics report"
    echo "- CSV statistical data files"
    echo ""
    echo "For custom parameters, use:"
    echo "python3 credit_analyzer.py --xpu_count <count> --vc_ids <VC_list>"
else
    echo "Error occurred during credit analysis, please check data files and error messages"
    exit 1
fi