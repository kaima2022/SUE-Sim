#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Simplified Queue Usage Analysis Script
Generate 3 core charts for XPU and Switch queue usage analysis

Copyright 2025 SUE-Sim Contributors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import seaborn as sns
import numpy as np
import os
import sys
from pathlib import Path
import argparse
from datetime import datetime

# Set font support
plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'Liberation Sans', 'Arial', 'sans-serif']
plt.rcParams['axes.unicode_minus'] = False

def xpu_id_to_switch_number(xpu_id, n_xpus=4):
    """
    Convert XpuId to switch number.
    In SUE-Sim: Switch 1 corresponds to XpuId nXpus + 1
    """
    if xpu_id <= n_xpus:
        return xpu_id  # This is an XPU, not a switch
    else:
        return xpu_id - n_xpus  # Switch number = XpuId - nXpus

def load_latest_data(data_dir):
    """Load the latest queue usage data"""
    try:
        # Find the latest data files from respective directories
        dest_queue_dir = Path(data_dir) / "destination_queue_logs"
        device_queue_dir = Path(data_dir) / "device_queue_logs"
        processing_queue_dir = Path(data_dir) / "processing_queue_logs"

        # Find the latest timestamped files
        dest_queue_files = list(dest_queue_dir.glob("destination_queue_*.csv")) if dest_queue_dir.exists() else []
        device_queue_files = list(device_queue_dir.glob("device_queue_*.csv")) if device_queue_dir.exists() else []
        processing_queue_files = list(processing_queue_dir.glob("processing_queue_*.csv")) if processing_queue_dir.exists() else []

        if not dest_queue_files or not device_queue_files:
            print("Queue usage data files not found")
            print(f"   Looked for destination queues in: {dest_queue_dir}")
            print(f"   Looked for device queues in: {device_queue_dir}")
            return None, None, None

        # Sort by modification time, take the latest
        dest_queue_file = max(dest_queue_files, key=lambda f: f.stat().st_mtime)
        device_queue_file = max(device_queue_files, key=lambda f: f.stat().st_mtime)

        print(f"Loading destination queue data: {dest_queue_file}")
        print(f"Loading device queue data: {device_queue_file}")

        # Read data
        dest_queue_data = pd.read_csv(dest_queue_file)
        device_queue_data = pd.read_csv(device_queue_file)

        # Load processing queue data if available
        processing_queue_data = None
        if processing_queue_files:
            processing_queue_file = max(processing_queue_files, key=lambda f: f.stat().st_mtime)
            print(f"Loading processing queue data: {processing_queue_file}")
            processing_queue_data = pd.read_csv(processing_queue_file)

        return dest_queue_data, device_queue_data, processing_queue_data

    except Exception as e:
        print(f"Failed to load data: {e}")
        return None, None, None

def plot_xpu_destination_queues(dest_data, output_dir, xpu_id=1):
    """Plot destination queue usage for all SUEs in specified XPU, with each subplot showing one SUE"""
    if dest_data is None or dest_data.empty:
        print("Destination queue is empty")
        return

    # Filter data for specified XPU
    xpu_data = dest_data[dest_data['XpuId'] == xpu_id]
    if xpu_data.empty:
        print(f"No data found for XPU {xpu_id}")
        return

    print(f"Plotting destination queues for all SUEs in XPU {xpu_id}...")

    # Check if VcId column exists
    if 'VcId' not in xpu_data.columns:
        print(" Warning: VcId column not found in destination queue data")
        print(f"   Available columns: {list(xpu_data.columns)}")
        return

    # Convert time from nanoseconds to seconds
    xpu_data = xpu_data.copy()
    xpu_data['Time'] = xpu_data['TimeNs'] / 1e9

    # Create output directory
    os.makedirs(output_dir, exist_ok=True)

    # Create figure with subplots for each SUE
    unique_sues = sorted(xpu_data['SueId'].unique())
    n_sues = len(unique_sues)

    if n_sues == 0:
        print(" No SUE data found")
        return

    # Determine subplot layout
    n_cols = min(2, n_sues)
    n_rows = (n_sues + n_cols - 1) // n_cols

    fig, axes = plt.subplots(n_rows, n_cols, figsize=(20, 8 * n_rows))
    fig.suptitle(f"XPU {xpu_id} Destination Queue Usage by SUE", fontsize=16, fontweight="bold")

    # Handle single subplot case
    if n_sues == 1:
        axes = [axes]
    elif n_rows == 1:
        axes = axes.reshape(1, -1)

    # Plot utilization for each SUE
    for i, sue_id in enumerate(unique_sues):
        row = i // n_cols
        col = i % n_cols
        ax = axes[row, col] if n_rows > 1 else axes[col]

        sue_data = xpu_data[xpu_data['SueId'] == sue_id]

        if not sue_data.empty:
            # Create unique {DestXpuId, VcId} combinations for this SUE
            sue_data['DestVcCombo'] = sue_data.apply(lambda row: f"{int(row['DestXpuId'])}-{int(row['VcId'])}", axis=1)
            unique_combos = sorted(sue_data['DestVcCombo'].unique())

            # Use different colors for each {DestXpuId, VcId} combination
            colors = plt.cm.tab10(np.linspace(0, 1, len(unique_combos)))

            for j, combo in enumerate(unique_combos):
                combo_data = sue_data[sue_data['DestVcCombo'] == combo]
                if not combo_data.empty:
                    ax.plot(combo_data['Time'], combo_data['Utilization(%)'],
                           label=f'Dest{combo}', linewidth=2, color=colors[j], alpha=0.8)

        ax.set_title(f'SUE {sue_id} Destination Queue Usage', fontsize=14, fontweight="bold")
        ax.set_xlabel('Time (seconds)', fontsize=12)
        ax.set_ylabel('Queue Utilization (%)', fontsize=12)
        ax.legend(title='Dest-XPU-VC', bbox_to_anchor=(1.05, 1), loc='upper left')
        ax.grid(True, alpha=0.3)
        ax.set_ylim(0, 100)

        # Set x-axis limits to match data range
        min_time = sue_data['Time'].min() if not sue_data.empty else xpu_data['Time'].min()
        max_time = sue_data['Time'].max() if not sue_data.empty else xpu_data['Time'].max()
        ax.set_xlim(left=min_time, right=max_time)

    # Hide unused subplots
    for i in range(n_sues, n_rows * n_cols):
        row = i // n_cols
        col = i % n_cols
        ax = axes[row, col] if n_rows > 1 else axes[col]
        ax.set_visible(False)

    plt.tight_layout()

    output_file = os.path.join(output_dir, f'xpu{xpu_id}_destination_queues_by_sue.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"XPU destination queue chart (by SUE) saved: {output_file}")

def plot_xpu_device_queues(device_data, output_dir, xpu_id=1, device_id=1):
    """Plot main and VC queue usage for specified XPU and device - VC queues on left, Main queue on right"""
    if device_data is None or device_data.empty:
        print("Device queue is empty")
        return

    # Filter data for specified XPU and device
    filtered_data = device_data[(device_data['XpuId'] == xpu_id) & (device_data['DeviceId'] == device_id)]
    if filtered_data.empty:
        print(f"No data found for XPU {xpu_id} Device {device_id}")
        return

    print(f"Plotting device queues for XPU {xpu_id} Device {device_id}...")

    # Convert time from nanoseconds to seconds
    filtered_data = filtered_data.copy()
    filtered_data['Time'] = filtered_data['TimeNs'] / 1e9

    # Create output directory
    os.makedirs(output_dir, exist_ok=True)

    # Create figure with two subplots (VC queues on left, Main queue on right)
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(20, 8))

    # Separate main queue and VC queue data
    main_queue_data = filtered_data[filtered_data['QueueType'] == 'Main']
    vc_queue_data = filtered_data[filtered_data['QueueType'] == 'VC']

    # Left subplot: VC queues
    if not vc_queue_data.empty:
        # Check if VCId column exists
        if 'VCId' not in vc_queue_data.columns:
            print(" Warning: VCId column not found in VC queue data")
            print(f"   Available columns: {list(vc_queue_data.columns)}")
        else:
            # Get unique VC IDs
            vc_ids = sorted(vc_queue_data['VCId'].unique())
            print(f"Found {len(vc_ids)} VCs: {vc_ids}")
            colors = plt.cm.Set3(np.linspace(0, 1, len(vc_ids)))  # Use different colors for each VC

            for i, vc_id in enumerate(vc_ids):
                vc_data = vc_queue_data[vc_queue_data['VCId'] == vc_id]
                if not vc_data.empty:
                    ax1.plot(vc_data['Time'], vc_data['Utilization(%)'],
                            label=f'VC {vc_id}', linewidth=2, color=colors[i], alpha=0.8)

    ax1.set_title(f'VC Queues Utilization - XPU {xpu_id} Device {device_id}', fontsize=14, fontweight="bold")
    ax1.set_xlabel('Time (seconds)', fontsize=12)
    ax1.set_ylabel('Queue Utilization (%)', fontsize=12)
    ax1.legend(fontsize=11)
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim(0, 100)

    # Right subplot: Main queue
    if not main_queue_data.empty:
        ax2.plot(main_queue_data['Time'], main_queue_data['Utilization(%)'],
                label='Main Queue', linewidth=3, color='blue', alpha=0.8)

    ax2.set_title(f'Main Queue Utilization - XPU {xpu_id} Device {device_id}', fontsize=14, fontweight="bold")
    ax2.set_xlabel('Time (seconds)', fontsize=12)
    ax2.set_ylabel('Queue Utilization (%)', fontsize=12)
    ax2.legend(fontsize=12)
    ax2.grid(True, alpha=0.3)
    ax2.set_ylim(0, 100)

    # Set x-axis limits to match data range for both subplots
    min_time = filtered_data['Time'].min()
    max_time = filtered_data['Time'].max()
    ax1.set_xlim(left=min_time, right=max_time)
    ax2.set_xlim(left=min_time, right=max_time)

    # Overall title
    fig.suptitle(f'XPU {xpu_id} Device {device_id} Queue Usage Analysis', fontsize=16, fontweight="bold")
    plt.tight_layout()

    output_file = os.path.join(output_dir, f'xpu{xpu_id}_device{device_id}_queues.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"XPU device queue chart saved: {output_file}")

def plot_switch_device_queues(device_data, output_dir, switch_id=5, device_id=1):
    """Plot main and VC queue usage for specified switch and device - VC queues on left, Main queue on right"""
    if device_data is None or device_data.empty:
        print("Device queue is empty")
        return

    # Filter data for specified switch and device (switch is XPU ID > 4)
    filtered_data = device_data[(device_data['XpuId'] == switch_id) & (device_data['DeviceId'] == device_id)]
    if filtered_data.empty:
        print(f"No data found for Switch {switch_id} Device {device_id}")
        return

    print(f"Plotting switch device queues for Switch {switch_id} Device {device_id}...")

    # Convert time from nanoseconds to seconds
    filtered_data = filtered_data.copy()
    filtered_data['Time'] = filtered_data['TimeNs'] / 1e9

    # Create output directory
    os.makedirs(output_dir, exist_ok=True)

    # Create figure with two subplots (VC queues on left, Main queue on right)
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(20, 8))

    # Separate main queue and VC queue data
    main_queue_data = filtered_data[filtered_data['QueueType'] == 'Main']
    vc_queue_data = filtered_data[filtered_data['QueueType'] == 'VC']

    # Left subplot: VC queues
    if not vc_queue_data.empty:
        # Check if VCId column exists
        if 'VCId' not in vc_queue_data.columns:
            print(" Warning: VCId column not found in VC queue data")
            print(f"   Available columns: {list(vc_queue_data.columns)}")
        else:
            # Get unique VC IDs
            vc_ids = sorted(vc_queue_data['VCId'].unique())
            print(f"Found {len(vc_ids)} VCs: {vc_ids}")
            colors = plt.cm.Set3(np.linspace(0, 1, len(vc_ids)))  # Use different colors for each VC

            for i, vc_id in enumerate(vc_ids):
                vc_data = vc_queue_data[vc_queue_data['VCId'] == vc_id]
                if not vc_data.empty:
                    ax1.plot(vc_data['Time'], vc_data['Utilization(%)'],
                            label=f'VC {vc_id}', linewidth=2, color=colors[i], alpha=0.8)

    ax1.set_title(f'VC Queues Utilization - Switch {switch_id} Device {device_id}', fontsize=14, fontweight="bold")
    ax1.set_xlabel('Time (seconds)', fontsize=12)
    ax1.set_ylabel('Queue Utilization (%)', fontsize=12)
    ax1.legend(fontsize=11)
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim(0, 100)

    # Right subplot: Main queue
    if not main_queue_data.empty:
        ax2.plot(main_queue_data['Time'], main_queue_data['Utilization(%)'],
                label='Main Queue', linewidth=3, color='blue', alpha=0.8)

    ax2.set_title(f'Main Queue Utilization - Switch {switch_id} Device {device_id}', fontsize=14, fontweight="bold")
    ax2.set_xlabel('Time (seconds)', fontsize=12)
    ax2.set_ylabel('Queue Utilization (%)', fontsize=12)
    ax2.legend(fontsize=12)
    ax2.grid(True, alpha=0.3)
    ax2.set_ylim(0, 100)

    # Set x-axis limits to match data range for both subplots
    min_time = filtered_data['Time'].min()
    max_time = filtered_data['Time'].max()
    ax1.set_xlim(left=min_time, right=max_time)
    ax2.set_xlim(left=min_time, right=max_time)

    # Overall title
    fig.suptitle(f'Switch {switch_id} Device {device_id} Queue Usage Analysis', fontsize=16, fontweight="bold")
    plt.tight_layout()

    # Convert XpuId to switch number for filename
    switch_number = xpu_id_to_switch_number(switch_id)
    output_file = os.path.join(output_dir, f'switch{switch_number}_device{device_id}_queues.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Switch device queue chart saved: {output_file}")

def plot_processing_queue_analysis(processing_data, output_dir, xpu_id=1, device_id=1, is_switch=False):
    """Plot link layer processing queue analysis for specified XPU/Switch and device"""
    if processing_data is None or processing_data.empty:
        print("Processing queue data is empty")
        return

    # Filter data for specified XPU/Switch and device
    filtered_data = processing_data[(processing_data['XpuId'] == xpu_id) & (processing_data['DeviceId'] == device_id)]
    if filtered_data.empty:
        device_type = "Switch" if is_switch else "XPU"
        print(f"No processing queue data found for {device_type} {xpu_id} Device {device_id}")
        return

    device_type = "Switch" if is_switch else "XPU"
    print(f"Plotting processing queue analysis for {device_type} {xpu_id} Device {device_id}...")

    # Convert time from nanoseconds to seconds
    filtered_data = filtered_data.copy()
    filtered_data['Time'] = filtered_data['TimeNs'] / 1e9

    # Create output directory
    os.makedirs(output_dir, exist_ok=True)

    # Create figure with subplots for comprehensive analysis
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle(f"{device_type} {xpu_id} Device {device_id} Link Layer Processing Queue Analysis", fontsize=16, fontweight="bold")

    # 1. Queue Length Over Time
    ax1 = axes[0, 0]
    ax1.plot(filtered_data['Time'], filtered_data['QueueLength'],
            linewidth=2, color='blue', alpha=0.8)
    ax1.set_title('Processing Queue Length Over Time')
    ax1.set_xlabel('Time (seconds)')
    ax1.set_ylabel('Queue Length (packets)')
    ax1.grid(True, alpha=0.3)

    # 2. Queue Utilization Over Time
    ax2 = axes[0, 1]
    if 'Utilization(%)' in filtered_data.columns:
        ax2.plot(filtered_data['Time'], filtered_data['Utilization(%)'],
                linewidth=2, color='red', alpha=0.8)
        ax2.set_title('Processing Queue Utilization Over Time')
        ax2.set_ylabel('Utilization (%)')
    else:
        # Calculate utilization if not directly available
        max_queue_size = filtered_data['QueueLength'].max()
        if max_queue_size > 0:
            utilization = (filtered_data['QueueLength'] / max_queue_size) * 100
            ax2.plot(filtered_data['Time'], utilization,
                    linewidth=2, color='red', alpha=0.8)
            ax2.set_title('Processing Queue Utilization Over Time (Calculated)')
            ax2.set_ylabel('Utilization (%)')

    ax2.set_xlabel('Time (seconds)')
    ax2.grid(True, alpha=0.3)

    # 3. Queue Length Distribution (Histogram)
    ax3 = axes[1, 0]
    ax3.hist(filtered_data['QueueLength'], bins=30, alpha=0.7, color='green', edgecolor='black')
    ax3.set_title('Processing Queue Length Distribution')
    ax3.set_xlabel('Queue Length (packets)')
    ax3.set_ylabel('Frequency')
    ax3.grid(True, alpha=0.3)

    # 4. Processing Queue Statistics
    ax4 = axes[1, 1]
    ax4.axis('off')

    # Calculate statistics
    stats_text = f"""
Processing Queue Statistics:
────────────────────────────
Queue Length:
  • Mean: {filtered_data['QueueLength'].mean():.2f} packets
  • Max: {filtered_data['QueueLength'].max():.2f} packets
  • Min: {filtered_data['QueueLength'].min():.2f} packets
  • Std: {filtered_data['QueueLength'].std():.2f} packets

Time Coverage:
  • Start: {filtered_data['Time'].min():.3f}s
  • End: {filtered_data['Time'].max():.3f}s
  • Duration: {filtered_data['Time'].max() - filtered_data['Time'].min():.3f}s

Data Points: {len(filtered_data)}
"""

    ax4.text(0.05, 0.95, stats_text, transform=ax4.transAxes,
            fontsize=10, verticalalignment='top', fontfamily='monospace')

    plt.tight_layout()

    # Save the plot
    if is_switch:
        # Convert XpuId to switch number for filename
        switch_number = xpu_id_to_switch_number(xpu_id)
        output_file = os.path.join(output_dir, f'switch{switch_number}_device{device_id}_processing_queues.png')
    else:
        output_file = os.path.join(output_dir, f'xpu{xpu_id}_device{device_id}_processing_queues.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Processing queue analysis chart saved: {output_file}")

def plot_processing_queue_comparison(processing_data, output_dir, xpu_id=1):
    """Compare processing queue usage across all devices in specified XPU"""
    if processing_data is None or processing_data.empty:
        print("Processing queue data is empty")
        return

    # Filter data for specified XPU
    xpu_data = processing_data[processing_data['XpuId'] == xpu_id]
    if xpu_data.empty:
        print(f"No processing queue data found for XPU {xpu_id}")
        return

    print(f"Plotting processing queue comparison across all devices in XPU {xpu_id}...")

    # Convert time from nanoseconds to seconds
    xpu_data = xpu_data.copy()
    xpu_data['Time'] = xpu_data['TimeNs'] / 1e9

    # Create output directory
    os.makedirs(output_dir, exist_ok=True)

    plt.figure(figsize=(16, 10))

    # Plot queue length for each device
    for device_id in sorted(xpu_data['DeviceId'].unique()):
        device_data = xpu_data[xpu_data['DeviceId'] == device_id]
        if not device_data.empty:
            plt.plot(device_data['Time'], device_data['QueueLength'],
                    label=f'Device {device_id}', linewidth=2, alpha=0.8)

    plt.title(f"XPU {xpu_id} All Devices Processing Queue Comparison", fontsize=16, fontweight="bold")
    plt.xlabel('Time (seconds)', fontsize=12)
    plt.ylabel('Queue Length (packets)', fontsize=12)
    plt.legend(title='Device ID', bbox_to_anchor=(1.05, 1), loc='upper left')
    plt.grid(True, alpha=0.3)

    # Set x-axis limits to match data range
    min_time = xpu_data['Time'].min()
    max_time = xpu_data['Time'].max()
    plt.xlim(left=min_time, right=max_time)

    plt.tight_layout()

    # Save the plot
    output_file = os.path.join(output_dir, f'xpu{xpu_id}_all_devices_processing_queues.png')
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Processing queue comparison chart saved: {output_file}")

def main():
    parser = argparse.ArgumentParser(description='Enhanced Queue Usage Analysis Tool with Link Layer Processing Queue Support')
    parser.add_argument('--data-dir', type=str, default='data',
                        help='Data file directory path')
    parser.add_argument('--output-dir', type=str, default='results/queue_usage',
                        help='Base output directory path')
    parser.add_argument('--xpu-id', type=int, default=1,
                        help='XPU ID to analyze (default: 1)')
    parser.add_argument('--device-id', type=int, default=1,
                        help='Device ID to analyze (default: 1)')
    parser.add_argument('--switch-id', type=int, default=5,
                        help='Switch ID to analyze (default: 5, first switch)')
    parser.add_argument('--include-processing', action='store_true',
                        help='Include link layer processing queue analysis')
    parser.add_argument('--auto-detect', action='store_true',
                        help='Auto-detect XPU and Switch IDs from data')

    args = parser.parse_args()

    # Always use timestamped directory
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    args.output_dir = os.path.join(args.output_dir, timestamp)

    print("Starting Enhanced Queue Usage Analysis Tool")
    print(f"Data directory: {args.data_dir}")
    print(f"Output directory: {args.output_dir}")
    print(f"Analysis targets:")
    print(f"   - XPU {args.xpu_id} Destination Queues")
    print(f"   - XPU {args.xpu_id} Device {args.device_id} Queues")
    print(f"   - Switch {args.switch_id} Device {args.device_id} Queues")
    if args.include_processing:
        print(f"   - XPU {args.xpu_id} Link Layer Processing Queues")

    # Load data
    dest_data, device_data, processing_data = load_latest_data(args.data_dir)

    if dest_data is None and device_data is None and processing_data is None:
        print("No valid data files found")
        sys.exit(1)

    # Auto-detect IDs if requested (must be after data loading)
    if args.auto_detect and processing_data is not None:
        # Auto-detect first XPU ID and device ID for processing queue analysis
        unique_xpu_ids = sorted(processing_data['XpuId'].unique())
        if unique_xpu_ids:
            args.xpu_id = unique_xpu_ids[0]
            xpu_devices = processing_data[processing_data['XpuId'] == args.xpu_id]
            unique_device_ids = sorted(xpu_devices['DeviceId'].unique())
            if unique_device_ids:
                args.device_id = unique_device_ids[0]
            print(f"Auto-detected XPU ID: {args.xpu_id}, Device ID: {args.device_id}")

    # Auto-detect switch ID (must be after data loading)
    if args.auto_detect and processing_data is not None:
        # In SUE-Sim: XPU nodes have XpuId <= nXpus (typically 1-4)
        # Switch nodes have XpuId > nXpus (typically 5+)
        # Switch 1 is the first switch: XpuId == nXpus + 1 (typically 5)

        unique_xpu_ids = sorted(processing_data['XpuId'].unique())

        # Find switch XPU IDs (typically XpuId > 4, assuming nXpus = 4)
        switch_xpu_ids = [xpu_id for xpu_id in unique_xpu_ids if xpu_id > 4]

        if switch_xpu_ids:
            # Switch 1 is the first switch (lowest XpuId > nXpus)
            args.switch_id = min(switch_xpu_ids)
            print(f"Auto-detected Switch 1 ID: {args.switch_id} (first switch, XpuId > nXpus)")
        else:
            # Fallback: use the highest XpuId if no clear switches found
            if len(unique_xpu_ids) > 1:
                args.switch_id = unique_xpu_ids[-1]
                print(f"Fallback: Using highest XpuId as Switch: {args.switch_id}")
            else:
                print(" Could not auto-detect switch ID, using default value")

    # Update the analysis targets display with detected IDs
    print("\nFinal Analysis Targets:")
    print(f"   - XPU {args.xpu_id} Destination Queues")
    print(f"   - XPU {args.xpu_id} Device {args.device_id} Queues")
    print(f"   - Switch {args.switch_id} Device {args.device_id} Queues")
    if args.include_processing:
        print(f"   - XPU {args.xpu_id} Link Layer Processing Queues")

    # Create output directory
    os.makedirs(args.output_dir, exist_ok=True)

    # Generate the core charts
    print("\nGenerating analysis charts...")

    # 1. XPU destination queues
    if dest_data is not None:
        plot_xpu_destination_queues(dest_data, args.output_dir, args.xpu_id)

    # 2. XPU device queues
    if device_data is not None:
        plot_xpu_device_queues(device_data, args.output_dir, args.xpu_id, args.device_id)

    # 3. Switch device queues
    if device_data is not None:
        plot_switch_device_queues(device_data, args.output_dir, args.switch_id, args.device_id)

    # 4. Link layer processing queue analysis (if requested)
    if args.include_processing and processing_data is not None:
        print("\nGenerating link layer processing queue analysis...")

        # Individual device analysis
        plot_processing_queue_analysis(processing_data, args.output_dir, args.xpu_id, args.device_id)

        # Switch device analysis - first switch first device
        print(f"Plotting processing queue analysis for Switch {args.switch_id} Device 1...")
        switch_processing_data = processing_data[processing_data['XpuId'] == args.switch_id]
        if not switch_processing_data.empty:
            switch_device_data = switch_processing_data[switch_processing_data['DeviceId'] == 1]
            if not switch_device_data.empty:
                plot_processing_queue_analysis(switch_device_data, args.output_dir, args.switch_id, 1, is_switch=True)

    print("\nEnhanced queue usage analysis completed!")
    print(f"Results saved in: {args.output_dir}")

    # List generated files
    generated_files = []
    if dest_data is not None:
        generated_files.append(f'xpu{args.xpu_id}_destination_queues_by_sue.png')
    if device_data is not None:
        generated_files.append(f'xpu{args.xpu_id}_device{args.device_id}_queues.png')
        # Convert switch XpuId to switch number for filename
        switch_number = xpu_id_to_switch_number(args.switch_id)
        generated_files.append(f'switch{switch_number}_device{args.device_id}_queues.png')
    if args.include_processing and processing_data is not None:
        generated_files.append(f'xpu{args.xpu_id}_device{args.device_id}_processing_queues.png')

        # Add switch processing queue file if generated
        switch_processing_data = processing_data[processing_data['XpuId'] == args.switch_id]
        if not switch_processing_data.empty:
            switch_device_data = switch_processing_data[switch_processing_data['DeviceId'] == 1]
            if not switch_device_data.empty:
                generated_files.append(f'switch{switch_number}_device1_processing_queues.png')

    print("\nGenerated charts:")
    for file in generated_files:
        print(f"  - {file}")

if __name__ == "__main__":
    main()