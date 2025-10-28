#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Standalone Link Layer Credit Analysis Script
Extract credit data from performance.csv and read credit data from independent files for analysis

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
import seaborn as sns
from pathlib import Path
from datetime import datetime
import argparse
import numpy as np
import os
import re

# Set Chinese font support
plt.rcParams['font.sans-serif'] = ['SimHei', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False

def generate_mac_device_mapping(xpu_count=4, devices_per_xpu=8, devices_per_switch=8):
    """Generate device mapping based on MAC address patterns

    MAC Address Pattern Analysis (based on log file observations):
    - Odd MAC addresses (last hex digit is odd): XPU devices
    - Even MAC addresses (last hex digit is even): Switch devices

    XPU Device Pattern:
    - XPU0: Dev1=01, Dev2=03, Dev3=05, Dev4=07, Dev5=09, Dev6=0b, Dev7=0d, Dev8=0f
    - XPU1: Dev1=11, Dev2=13, Dev3=15, Dev4=17, Dev5=19, Dev6=1b, Dev7=1d, Dev8=1f
    - XPU2: Dev1=21, Dev2=23, Dev3=25, Dev4=27, Dev5=29, Dev6=2b, Dev7=2d, Dev8=2f
    - XPU3: Dev1=31, Dev2=33, Dev3=35, Dev4=37, Dev5=39, Dev6=3b, Dev7=3d, Dev8=3f
    - Formula: MAC = XPU_ID * 0x10 + (DEV_ID - 1) * 2 + 1

    Switch Device Pattern (interleaved allocation):
    - Switch0: Dev1=02, Dev2=04, Dev3=12, Dev4=14, Dev5=22, Dev6=24, Dev7=32, Dev8=34
    - Switch1: Dev1=06, Dev2=08, Dev3=16, Dev4=18, Dev5=26, Dev6=28, Dev7=36, Dev8=38
    - Switch2: Dev1=0a, Dev2=0c, Dev3=1a, Dev4=1c, Dev5=2a, Dev6=2c, Dev7=3a, Dev8=3c
    - Switch3: Dev1=0e, Dev2=10, Dev3=1e, Dev4=20, Dev5=2e, Dev6=30, Dev7=3e, Dev8=40
    - Pattern: Each switch's devices are interleaved across 4 consecutive even segments
    """
    mac_to_device_map = {}

    # Generate XPU device mapping (odd MAC addresses)
    for xpu_id in range(xpu_count):
        for dev_id in range(1, devices_per_xpu + 1):
            # Calculate the last byte of MAC address
            mac_last_byte = xpu_id * 0x10 + (dev_id - 1) * 2 + 1
            mac_addr = f"00:00:00:00:00:{mac_last_byte:02x}"
            device_name = f"XPU{xpu_id} Dev{dev_id}"
            mac_to_device_map[mac_addr] = device_name

    # Generate switch device mapping (even MAC addresses, interleaved allocation)
    for switch_id in range(xpu_count):  # Switch ID starts from 0
        for dev_id in range(1, devices_per_switch + 1):
            # Interleaved allocation pattern
            if dev_id <= 2:
                # First and second devices use the first even segment
                mac_last_byte = switch_id * 0x04 + dev_id * 2
            elif dev_id <= 4:
                # Third and fourth devices use the second even segment
                mac_last_byte = 0x10 + switch_id * 0x04 + (dev_id - 2) * 2
            elif dev_id <= 6:
                # Fifth and sixth devices use the third even segment
                mac_last_byte = 0x20 + switch_id * 0x04 + (dev_id - 4) * 2
            else:
                # Seventh and eighth devices use the fourth even segment
                mac_last_byte = 0x30 + switch_id * 0x04 + (dev_id - 6) * 2

            mac_addr = f"00:00:00:00:00:{mac_last_byte:02x}"
            device_name = f"Switch{switch_id + 1} Dev{dev_id}"  # Switch numbering starts from 1 for display
            mac_to_device_map[mac_addr] = device_name

    return mac_to_device_map

# Generate dynamic mapping table based on patterns
MAC_TO_DEVICE_MAP = generate_mac_device_mapping()
DEVICE_TO_MAC_MAP = {v: k for k, v in MAC_TO_DEVICE_MAP.items()}

def find_latest_credit_csv_file():
    """Automatically find the latest timestamped link layer credit log file"""
    credit_logs_dir = Path("..") / "data" / "link_credit_logs"

    if not credit_logs_dir.exists():
        print(f"Link layer credit log directory does not exist: {credit_logs_dir}")
        return None

    # Find all link_credit*.csv files
    csv_files = list(credit_logs_dir.glob("link_credit*.csv"))

    if not csv_files:
        print(f"No link_credit*.csv files found in directory {credit_logs_dir}")
        return None

    # Sort by file modification time, select the latest
    latest_file = max(csv_files, key=lambda x: x.stat().st_mtime)
    print(f"Automatically selected latest credit log file: {latest_file}")

    return str(latest_file)

def find_latest_performance_csv_file():
    """Automatically find the latest timestamped performance log file (for backward compatibility)"""
    performance_logs_dir = Path("..") / "data" / "performance_logs"

    if not performance_logs_dir.exists():
        print(f"Performance log directory does not exist: {performance_logs_dir}")
        return None

    # Find all performance*.csv files
    csv_files = list(performance_logs_dir.glob("performance*.csv"))

    if not csv_files:
        print(f"No performance*.csv files found in directory {performance_logs_dir}")
        return None

    # Sort by file modification time, select the latest
    latest_file = max(csv_files, key=lambda x: x.stat().st_mtime)
    print(f"Automatically selected latest performance log file: {latest_file}")

    return str(latest_file)

def load_credit_data(filename):
    """Load link layer credit data"""
    data_path = Path(".") / filename
    df = pd.read_csv(data_path)
    df.columns = ['TimeNs', 'XpuId', 'DeviceId', 'VCId', 'Direction', 'Credits', 'MacAddress']
    return df

def load_legacy_credit_data(filename):
    """Load credit data from performance.csv (backward compatibility)"""
    data_path = Path(".") / filename
    df = pd.read_csv(data_path)
    df.columns = ['Time', 'XpuId', 'DeviceId', 'VCId', 'Direction', 'Rate', 'MacAddress']

    # Filter credit data (Direction is Tx and Rate has integer values)
    credit_df = df[df['Direction'] == 'Tx'].copy()
    credit_df['Credits'] = credit_df['Rate'].astype(int)
    credit_df['TimeNs'] = credit_df['Time']

    return credit_df[['TimeNs', 'XpuId', 'DeviceId', 'VCId', 'Direction', 'Credits', 'MacAddress']]

def create_output_directory():
    """Create credit analysis output directory"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_dir = Path("../results") / "credit_analysis" / timestamp
    output_dir.mkdir(parents=True, exist_ok=True)
    return output_dir

def plot_credit_changes(df, output_dir, xpu_count=4, vc_ids=None):
    """Plot credit value changes over time
    Args:
        df: DataFrame
        output_dir: Output directory
        xpu_count: Number of XPUs
        vc_ids: List of VC IDs to display, None means display all VCs
    """
    if df.empty:
        print("Warning: No credit data available for plotting")
        return

    # Only process XPU credit data
    xpu_credit_df = df[df['XpuId'] <= xpu_count]

    if xpu_credit_df.empty:
        print("Warning: No XPU credit data available")
        return

    # Filter data if vc_ids is specified
    if vc_ids is not None:
        xpu_credit_df = xpu_credit_df[xpu_credit_df['VCId'].isin(vc_ids)]
        if xpu_credit_df.empty:
            print(f"Warning: No credit data found for VC {vc_ids}")
            return

    # Create figure - group by XPU
    fig, axes = plt.subplots(xpu_count, 1, figsize=(15, 5 * xpu_count))
    fig.suptitle('Link Layer Credit Value Changes Over Time', fontsize=16)

    # If only one XPU, ensure axes is a list
    if xpu_count == 1:
        axes = [axes]

    # Iterate through each XPU
    for i in range(xpu_count):
        xpu_id = i + 1
        ax = axes[i]

        # Get current XPU data
        xpu_data = xpu_credit_df[xpu_credit_df['XpuId'] == xpu_id]

        if xpu_data.empty:
            ax.text(0.5, 0.5, f'No Credit Data for XPU {xpu_id}',
                    ha='center', va='center', fontsize=12)
            ax.set_title(f'XPU {xpu_id} (No Data)')
            continue

        # Group by device and VC
        for (dev_id, vc_id), group in xpu_data.groupby(['DeviceId', 'VCId']):
            group = group.sort_values('TimeNs')
            time_seconds = group['TimeNs'] / 1e9  # Convert to seconds
            credit_value = group['Credits']

            ax.plot(
                time_seconds,
                credit_value,
                label=f'Dev{dev_id} VC{vc_id}',
                linewidth=1.5,
                linestyle='-'
            )

        # Update title to reflect VC filtering
        title = f'XPU {xpu_id} Credit Values'
        if vc_ids is not None:
            vc_str = ', '.join(map(str, vc_ids))
            title += f' (VCs: {vc_str})'
        ax.set_title(title)

        ax.set_xlabel('Time (seconds)')
        ax.set_ylabel('Credit Value')
        ax.legend(title='Device & VC', loc='upper right')
        ax.grid(True, linestyle=':')

        # Set X-axis range
        min_time = xpu_data['TimeNs'].min() / 1e9
        max_time = xpu_data['TimeNs'].max() / 1e9
        ax.set_xlim(left=min_time, right=max_time)

    plt.tight_layout(pad=3.0)

    # Update filename to reflect VC filtering
    filename = 'credit_value_changes'
    if vc_ids is not None:
        vc_str = '_'.join(map(str, vc_ids))
        filename += f'_vc{vc_str}'
    filename += '.png'

    output_file = output_dir / filename
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Credit value change chart generated: {output_file}")

def plot_switch_credit_changes(df, output_dir, xpu_count=4):
    """Plot credit value changes over time for devices recorded on switches"""
    # Filter switch credit data
    switch_credit_df = df[df['XpuId'] > xpu_count].copy()

    if switch_credit_df.empty:
        print("Warning: No switch credit data available for plotting")
        return

    # Use mapping table to convert MAC addresses to target device names
    switch_credit_df['TargetDeviceName'] = switch_credit_df['MacAddress'].map(
        lambda x: MAC_TO_DEVICE_MAP.get(x, f"Unknown ({x})")
    )

    # Group by switch ID and device ID
    switch_device_groups = switch_credit_df.groupby(['XpuId', 'DeviceId'])

    # Create figure - one subplot for each switch-device combination
    n_groups = len(switch_device_groups)
    if n_groups == 0:
        print("Warning: No switch device data found")
        return

    # Calculate subplot rows and columns
    n_cols = 2
    n_rows = (n_groups + n_cols - 1) // n_cols

    fig, axes = plt.subplots(n_rows, n_cols, figsize=(15, 5 * n_rows))
    fig.suptitle('Switch Credit Value Changes Over Time', fontsize=16)

    # If only one subplot, ensure axes is a 2D array
    if n_groups == 1:
        axes = np.array([[axes]])
    elif n_rows == 1:
        axes = axes.reshape(1, -1)

    # Iterate through each switch-device combination
    for i, ((switch_id, device_id), group_data) in enumerate(switch_device_groups):
        row = i // n_cols
        col = i % n_cols
        ax = axes[row, col]

        # Group by target device and VC
        for (target_device_name, vc_id), group in group_data.groupby(['TargetDeviceName', 'VCId']):
            group = group.sort_values('TimeNs')
            time_seconds = group['TimeNs'] / 1e9  # Convert to seconds
            credit_value = group['Credits']

            ax.plot(
                time_seconds,
                credit_value,
                label=f'{target_device_name} VC{vc_id}',
                linewidth=1.5,
                linestyle='-'
            )

        ax.set_title(f'Switch {switch_id - xpu_count} Dev{device_id}')
        ax.set_xlabel('Time (seconds)')
        ax.set_ylabel('Credit Value')
        ax.legend(title='Target Device & VC', loc='upper right')
        ax.grid(True, linestyle=':')

        # Set X-axis range
        min_time = group_data['TimeNs'].min() / 1e9
        max_time = group_data['TimeNs'].max() / 1e9
        ax.set_xlim(left=min_time, right=max_time)

    # Hide extra subplots
    for i in range(n_groups, n_rows * n_cols):
        row = i // n_cols
        col = i % n_cols
        axes[row, col].set_visible(False)

    plt.tight_layout(pad=3.0)

    output_file = output_dir / 'switch_credit_value_changes.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Switch credit value change chart generated: {output_file}")

def generate_credit_statistics(df, output_dir, xpu_count=4):
    """Generate credit value statistical report"""
    if df.empty:
        print("Warning: No credit data available for statistical analysis")
        return

    # Separate XPU and switch data
    xpu_df = df[df['XpuId'] <= xpu_count]
    switch_df = df[df['XpuId'] > xpu_count]

    # Generate statistical report
    report_lines = []
    report_lines.append("Link Layer Credit Analysis Report")
    report_lines.append("=" * 50)
    report_lines.append(f"Analysis Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    report_lines.append(f"Total Credit Records: {len(df)}")
    report_lines.append("")

    # XPU statistics
    if not xpu_df.empty:
        report_lines.append("XPU Credit Statistics:")
        report_lines.append("-" * 30)

        for xpu_id in range(1, xpu_count + 1):
            xpu_data = xpu_df[xpu_df['XpuId'] == xpu_id]
            if not xpu_data.empty:
                min_credit = xpu_data['Credits'].min()
                max_credit = xpu_data['Credits'].max()
                avg_credit = xpu_data['Credits'].mean()
                std_credit = xpu_data['Credits'].std()

                report_lines.append(f"  XPU {xpu_id}:")
                report_lines.append(f"    Min Credit: {min_credit}")
                report_lines.append(f"    Max Credit: {max_credit}")
                report_lines.append(f"    Avg Credit: {avg_credit:.2f}")
                report_lines.append(f"    Std Dev: {std_credit:.2f}")
                report_lines.append("")

    # Switch statistics
    if not switch_df.empty:
        report_lines.append("Switch Credit Statistics:")
        report_lines.append("-" * 30)

        for switch_id in switch_df['XpuId'].unique():
            switch_data = switch_df[switch_df['XpuId'] == switch_id]
            if not switch_data.empty:
                min_credit = switch_data['Credits'].min()
                max_credit = switch_data['Credits'].max()
                avg_credit = switch_data['Credits'].mean()
                std_credit = switch_data['Credits'].std()

                report_lines.append(f"  Switch {switch_id - xpu_count}:")
                report_lines.append(f"    Min Credit: {min_credit}")
                report_lines.append(f"    Max Credit: {max_credit}")
                report_lines.append(f"    Avg Credit: {avg_credit:.2f}")
                report_lines.append(f"    Std Dev: {std_credit:.2f}")
                report_lines.append("")

    # Write report file
    report_content = "\n".join(report_lines)
    report_file = output_dir / "credit_analysis_report.txt"
    with open(report_file, 'w', encoding='utf-8') as f:
        f.write(report_content)

    print(f"Credit analysis report generated: {report_file}")

    # Generate CSV statistical data
    if not xpu_df.empty:
        xpu_stats = xpu_df.groupby(['XpuId', 'DeviceId', 'VCId'])['Credits'].agg(
            ['count', 'mean', 'std', 'min', 'max']
        ).round(2)
        xpu_stats_file = output_dir / "xpu_credit_statistics.csv"
        xpu_stats.to_csv(xpu_stats_file)
        print(f"XPU credit statistics generated: {xpu_stats_file}")

    if not switch_df.empty:
        switch_stats = switch_df.groupby(['XpuId', 'DeviceId', 'VCId'])['Credits'].agg(
            ['count', 'mean', 'std', 'min', 'max']
        ).round(2)
        switch_stats_file = output_dir / "switch_credit_statistics.csv"
        switch_stats.to_csv(switch_stats_file)
        print(f"Switch credit statistics generated: {switch_stats_file}")

def main():
    try:
        # Set up command line argument parsing
        parser = argparse.ArgumentParser(description='Link Layer Credit Analysis Tool')
        parser.add_argument('--xpu_count', type=int,
                            help='Number of XPUs to analyze', default=4)
        parser.add_argument('--credit_file', type=str,
                            help='Link layer credit log file name', default=None)
        parser.add_argument('--performance_file', type=str,
                            help='Performance log file name (backward compatibility)', default=None)
        parser.add_argument('--vc_ids', type=str,
                            help='List of VC IDs to display, comma-separated, e.g., "0,1"', default=None)

        args = parser.parse_args()

        print("Starting link layer credit data processing...")

        # First try to load independent credit log file
        credit_df = None
        if args.credit_file is None:
            print("No credit log file specified, trying to find the latest independent credit log file...")
            credit_file = find_latest_credit_csv_file()
            if credit_file is not None:
                credit_df = load_credit_data(credit_file)
                print(f"Using independent credit log file: {credit_file}")

        if credit_df is None and args.performance_file is None:
            print("No independent credit log file found, trying to find performance log file (backward compatibility)...")
            performance_file = find_latest_performance_csv_file()
            if performance_file is not None:
                credit_df = load_legacy_credit_data(performance_file)
                print(f"Using performance log file (backward compatibility): {performance_file}")

        if credit_df is None:
            print("Unable to find available credit data file, exiting")
            exit(1)

        if credit_df.empty:
            print("Credit data is empty, exiting")
            exit(1)

        # Create output directory
        output_dir = create_output_directory()
        print(f"All output files will be saved to: {output_dir}")
        print(f"Analyzing XPU count: {args.xpu_count}")

        # Parse VC ID parameter
        vc_ids = None
        if args.vc_ids:
            vc_ids = [int(x.strip()) for x in args.vc_ids.split(',')]

        # Plot credit value change charts
        plot_credit_changes(credit_df, output_dir, args.xpu_count, vc_ids=vc_ids)

        # Plot switch credit value change charts
        # plot_switch_credit_changes(credit_df, output_dir, args.xpu_count)  # Temporarily commented

        # Generate statistical report
        generate_credit_statistics(credit_df, output_dir, args.xpu_count)

        print(f"\nCredit analysis completed! Results saved to: {output_dir}")

    except Exception as e:
        print(f"Processing error: {str(e)}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()