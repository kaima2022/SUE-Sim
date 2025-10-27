#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SUE Protocol Network Visualization Analysis Script

This script is used to analyze network performance data from SUE-Sim simulation, including:
1. Device throughput analysis: Throughput comparison of XPU and switch devices
2. Packet loss event analysis: Time series analysis of various packet loss types
3. Statistical distribution analysis: Throughput distribution and statistical characteristics

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

# Font settings section omitted, using default configuration
def generate_mac_device_mapping(xpu_count=4, devices_per_xpu=8, devices_per_switch=8):
    """Generate device mapping based on MAC address patterns

    MAC address pattern analysis (based on log file observations):
    - Odd MAC addresses (last hexadecimal digit is odd): XPU devices
    - Even MAC addresses (last hexadecimal digit is even): Switch devices

    XPU device pattern:
    - XPU0: Dev1=01, Dev2=03, Dev3=05, Dev4=07, Dev5=09, Dev6=0b, Dev7=0d, Dev8=0f
    - XPU1: Dev1=11, Dev2=13, Dev3=15, Dev4=17, Dev5=19, Dev6=1b, Dev7=1d, Dev8=1f
    - XPU2: Dev1=21, Dev2=23, Dev3=25, Dev4=27, Dev5=29, Dev6=2b, Dev7=2d, Dev8=2f
    - XPU3: Dev1=31, Dev2=33, Dev3=35, Dev4=37, Dev5=39, Dev6=3b, Dev7=3d, Dev8=3f
    - Formula: MAC = XPU_ID * 0x10 + (DEV_ID - 1) * 2 + 1

    Switch device pattern (interleaved allocation):
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
    for switch_id in range(xpu_count):  # Switch numbering starts from 0
        for dev_id in range(1, devices_per_switch + 1):
            # Interleaved allocation pattern
            if dev_id <= 2:
                # Devices 1, 2 use the first even segment
                mac_last_byte = switch_id * 0x04 + dev_id * 2
            elif dev_id <= 4:
                # Devices 3, 4 use the second even segment
                mac_last_byte = 0x10 + switch_id * 0x04 + (dev_id - 2) * 2
            elif dev_id <= 6:
                # Devices 5, 6 use the third even segment
                mac_last_byte = 0x20 + switch_id * 0x04 + (dev_id - 4) * 2
            else:
                # Devices 7, 8 use the fourth even segment
                mac_last_byte = 0x30 + switch_id * 0x04 + (dev_id - 6) * 2

            mac_addr = f"00:00:00:00:00:{mac_last_byte:02x}"
            device_name = f"Switch{switch_id + 1} Dev{dev_id}"  # Switch numbering starts from 1 for display
            mac_to_device_map[mac_addr] = device_name

    return mac_to_device_map

# Generate MAC address to device mapping table
MAC_TO_DEVICE_MAP = generate_mac_device_mapping()

# Generate device to MAC address reverse mapping
DEVICE_TO_MAC_MAP = {v: k for k, v in MAC_TO_DEVICE_MAP.items()}

def load_performance_data(filename):
    """Load performance data"""
    data_path = Path(".") / filename
    df = pd.read_csv(data_path)
    df.columns = ['Time', 'XpuId', 'DeviceId', 'VCId', 'Direction', 'Rate', 'MacAddress']
    return df

def find_latest_csv_file():
    """Automatically find the latest timestamped performance_log file"""
    performance_logs_dir = Path("..") / "data" / "performance_logs"

    if not performance_logs_dir.exists():
        print(f"Performance log directory does not exist: {performance_logs_dir}")
        return None

    # Find all performance*.csv files (supports performance.csv_timestamp.csv and performance_timestamp.csv formats)
    csv_files = list(performance_logs_dir.glob("performance*.csv"))

    if not csv_files:
        print(f"No performance*.csv files found in directory {performance_logs_dir}")
        return None

    # Sort by file modification time, select the latest
    latest_file = max(csv_files, key=lambda x: x.stat().st_mtime)
    print(f"Automatically selected latest file: {latest_file}")

    return str(latest_file)

def create_output_directory():
    """Create network visualization output directory"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_dir = Path("../results") / "network_visualizations" / timestamp
    output_dir.mkdir(parents=True, exist_ok=True)
    return output_dir

def plot_xpu_device_throughput(df, output_dir, xpu_count=4):
    """Plot device throughput for each XPU/Switch"""
    # Filter all data
    filtered_df = df[
        (df['Direction'].isin(['Tx', 'Rx'])) &
        (pd.to_numeric(df['Rate'], errors='coerce') > 0)
        ].copy()

    # Separate data into XPU and switch
    xpu_df = filtered_df[filtered_df['XpuId'] <= xpu_count]
    switch_df = filtered_df[filtered_df['XpuId'] > xpu_count]

    # Return if no data
    if xpu_df.empty and switch_df.empty:
        print("Warning: No valid data available for plotting")
        return

    # Create figure - two subplots
    fig, axes = plt.subplots(1, 2, figsize=(30, 10))
    fig.suptitle('Device Throughput Comparison', fontsize=16)

    # XPU subplot
    if not xpu_df.empty:
        # Group by XpuId
        xpu_groups = xpu_df.groupby('XpuId')

        # Create curves for each XPU
        for xpu_id, group in xpu_groups:
            # Sum data for same timestamp and same device
            summed_df = group.groupby(['Time', 'DeviceId'])['Rate'].sum().reset_index()

            for dev_id, dev_df in summed_df.groupby('DeviceId'):
                dev_df = dev_df.sort_values('Time')
                # Convert to Gbps (original Rate unit is Gbps, no conversion needed)
                throughput = dev_df['Rate']  # Keep Gbps unit
                # Convert time unit to seconds
                time_seconds = dev_df['Time'] / 1e9
                axes[0].plot(
                    time_seconds,
                    throughput,
                    label=f'XPU {xpu_id} Dev{dev_id}',
                    linewidth=1.5,
                    linestyle='-'
                )

        axes[0].set_title(f'XPU Throughput (XpuId <= {xpu_count})')
        axes[0].set_xlabel('Time (seconds)')  # Modified unit
        axes[0].set_ylabel('Throughput (Gbps)')  # Modified unit
        axes[0].legend(title='Device', bbox_to_anchor=(1.05, 1))
        axes[0].grid(True, linestyle=':')
        min_time = xpu_df['Time'].min() / 1e9  # Convert to seconds
        max_time = xpu_df['Time'].max() / 1e9  # Convert to seconds
        axes[0].set_xlim(left=min_time, right=max_time)

    # Switch subplot
    if not switch_df.empty:
        # Group by XpuId
        switch_groups = switch_df.groupby('XpuId')

        # Create curves for each switch
        for switch_id, group in switch_groups:
            # Sum data for same timestamp and same device
            summed_df = group.groupby(['Time', 'DeviceId'])['Rate'].sum().reset_index()

            for dev_id, dev_df in summed_df.groupby('DeviceId'):
                dev_df = dev_df.sort_values('Time')
                # Convert to Gbps (original Rate unit is Gbps, no conversion needed)
                throughput = dev_df['Rate']  # Keep Gbps unit
                # Convert time unit to seconds
                time_seconds = dev_df['Time'] / 1e9
                axes[1].plot(
                    time_seconds,
                    throughput,
                    label=f'Switch {switch_id - xpu_count} Dev{dev_id}',
                    linewidth=1.5,
                    linestyle='-'
                )

        axes[1].set_title(f'Switch Throughput (XpuId > {xpu_count})')
        axes[1].set_xlabel('Time (seconds)')  # Modified unit
        axes[1].set_ylabel('Throughput (Gbps)')  # Modified unit
        axes[1].legend(title='Device', bbox_to_anchor=(1.05, 1))
        axes[1].grid(True, linestyle=':')
        min_time = switch_df['Time'].min() / 1e9  # Convert to seconds
        max_time = switch_df['Time'].max() / 1e9  # Convert to seconds
        axes[1].set_xlim(left=min_time, right=max_time)

    plt.tight_layout()

    output_file = output_dir / 'device_throughput_comparison.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Device throughput comparison chart generated: {output_file}")

def plot_device_throughput_Tx_Rx(df, output_dir, xpu_count=4):
    """Plot device throughput for each XPU/Switch, separated by Tx and Rx"""
    # Filter all data
    filtered_df = df[
        (df['Direction'].isin(['Tx', 'Rx'])) &
        (pd.to_numeric(df['Rate'], errors='coerce') > 0)
        ].copy()

    # Separate data into XPU and switch
    xpu_df = filtered_df[filtered_df['XpuId'] <= xpu_count]
    switch_df = filtered_df[filtered_df['XpuId'] > xpu_count]

    # Return if no data
    if xpu_df.empty and switch_df.empty:
        print("Warning: No valid data available for plotting")
        return

    # Create figure - four subplots
    fig, axes = plt.subplots(2, 2, figsize=(30, 20))
    fig.suptitle('Device Throughput Comparison (Separated by Tx/Rx)', fontsize=16)

    # XPU Tx subplot
    if not xpu_df.empty:
        # Filter Tx data
        xpu_tx_df = xpu_df[xpu_df['Direction'] == 'Tx']

        if not xpu_tx_df.empty:
            # Group by XpuId
            xpu_groups = xpu_tx_df.groupby('XpuId')

            # Create curves for each XPU
            for xpu_id, group in xpu_groups:
                # Sum data for same timestamp and same device
                summed_df = group.groupby(['Time', 'DeviceId'])['Rate'].sum().reset_index()

                for dev_id, dev_df in summed_df.groupby('DeviceId'):
                    dev_df = dev_df.sort_values('Time')
                    # Convert to Gbps (original Rate unit is Gbps, no conversion needed)
                    throughput = dev_df['Rate']  # Keep Gbps unit
                    # Convert time unit to seconds
                    time_seconds = dev_df['Time'] / 1e9
                    axes[0, 0].plot(
                        time_seconds,
                        throughput,
                        label=f'XPU {xpu_id} Dev{dev_id}',
                        linewidth=1.5,
                        linestyle='-'
                    )

            axes[0, 0].set_title(f'XPU Tx Throughput (XpuId <= {xpu_count})')
            axes[0, 0].set_xlabel('Time (seconds)')
            axes[0, 0].set_ylabel('Throughput (Gbps)')
            axes[0, 0].legend(title='Device', bbox_to_anchor=(1.05, 1))
            axes[0, 0].grid(True, linestyle=':')
            min_time = xpu_tx_df['Time'].min() / 1e9
            max_time = xpu_tx_df['Time'].max() / 1e9
            axes[0, 0].set_xlim(left=min_time, right=max_time)

    # XPU Rx subplot
    if not xpu_df.empty:
        # Filter Rx data
        xpu_rx_df = xpu_df[xpu_df['Direction'] == 'Rx']

        if not xpu_rx_df.empty:
            # Group by XpuId
            xpu_groups = xpu_rx_df.groupby('XpuId')

            # Create curves for each XPU
            for xpu_id, group in xpu_groups:
                # Sum data for same timestamp and same device
                summed_df = group.groupby(['Time', 'DeviceId'])['Rate'].sum().reset_index()

                for dev_id, dev_df in summed_df.groupby('DeviceId'):
                    dev_df = dev_df.sort_values('Time')
                    # Convert to Gbps (original Rate unit is Gbps, no conversion needed)
                    throughput = dev_df['Rate']  # Keep Gbps unit
                    # Convert time unit to seconds
                    time_seconds = dev_df['Time'] / 1e9
                    axes[0, 1].plot(
                        time_seconds,
                        throughput,
                        label=f'XPU {xpu_id} Dev{dev_id}',
                        linewidth=1.5,
                        linestyle='-'
                    )

            axes[0, 1].set_title(f'XPU Rx Throughput (XpuId <= {xpu_count})')
            axes[0, 1].set_xlabel('Time (seconds)')
            axes[0, 1].set_ylabel('Throughput (Gbps)')
            axes[0, 1].legend(title='Device', bbox_to_anchor=(1.05, 1))
            axes[0, 1].grid(True, linestyle=':')
            min_time = xpu_rx_df['Time'].min() / 1e9
            max_time = xpu_rx_df['Time'].max() / 1e9
            axes[0, 1].set_xlim(left=min_time, right=max_time)

    # Switch Tx subplot
    if not switch_df.empty:
        # Filter Tx data
        switch_tx_df = switch_df[switch_df['Direction'] == 'Tx']

        if not switch_tx_df.empty:
            # Group by XpuId
            switch_groups = switch_tx_df.groupby('XpuId')

            # Create curves for each switch
            for switch_id, group in switch_groups:
                # Sum data for same timestamp and same device
                summed_df = group.groupby(['Time', 'DeviceId'])['Rate'].sum().reset_index()

                for dev_id, dev_df in summed_df.groupby('DeviceId'):
                    dev_df = dev_df.sort_values('Time')
                    # Convert to Gbps (original Rate unit is Gbps, no conversion needed)
                    throughput = dev_df['Rate']  # Keep Gbps unit
                    # Convert time unit to seconds
                    time_seconds = dev_df['Time'] / 1e9
                    axes[1, 0].plot(
                        time_seconds,
                        throughput,
                        label=f'Switch {switch_id} Dev{dev_id}',
                        linewidth=1.5,
                        linestyle='-'
                    )

            axes[1, 0].set_title(f'Switch Tx Throughput (XpuId > {xpu_count})')
            axes[1, 0].set_xlabel('Time (seconds)')
            axes[1, 0].set_ylabel('Throughput (Gbps)')
            axes[1, 0].legend(title='Device', bbox_to_anchor=(1.05, 1))
            axes[1, 0].grid(True, linestyle=':')
            min_time = switch_tx_df['Time'].min() / 1e9
            max_time = switch_tx_df['Time'].max() / 1e9
            axes[1, 0].set_xlim(left=min_time, right=max_time)

    # Switch Rx subplot
    if not switch_df.empty:
        # Filter Rx data
        switch_rx_df = switch_df[switch_df['Direction'] == 'Rx']

        if not switch_rx_df.empty:
            # Group by XpuId
            switch_groups = switch_rx_df.groupby('XpuId')

            # Create curves for each switch
            for switch_id, group in switch_groups:
                # Sum data for same timestamp and same device
                summed_df = group.groupby(['Time', 'DeviceId'])['Rate'].sum().reset_index()

                for dev_id, dev_df in summed_df.groupby('DeviceId'):
                    dev_df = dev_df.sort_values('Time')
                    # Convert to Gbps (original Rate unit is Gbps, no conversion needed)
                    throughput = dev_df['Rate']  # Keep Gbps unit
                    # Convert time unit to seconds
                    time_seconds = dev_df['Time'] / 1e9
                    axes[1, 1].plot(
                        time_seconds,
                        throughput,
                        label=f'Switch {switch_id} Dev{dev_id}',
                        linewidth=1.5,
                        linestyle='-'
                    )

            axes[1, 1].set_title(f'Switch Rx Throughput (XpuId > {xpu_count})')
            axes[1, 1].set_xlabel('Time (seconds)')
            axes[1, 1].set_ylabel('Throughput (Gbps)')
            axes[1, 1].legend(title='Device', bbox_to_anchor=(1.05, 1))
            axes[1, 1].grid(True, linestyle=':')
            min_time = switch_rx_df['Time'].min() / 1e9
            max_time = switch_rx_df['Time'].max() / 1e9
            axes[1, 1].set_xlim(left=min_time, right=max_time)

    plt.tight_layout()

    output_file = output_dir / 'device_throughput_comparison_separated.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Device throughput comparison chart (Tx/Rx separated) generated: {output_file}")

def plot_device_throughput_boxplot(df, output_dir, xpu_count=4):
    """
    Plot total throughput for each XPU and switch (sum of all devices)
    Separate Tx and Rx directions
    """
    # Filter all data
    filtered_df = df[
        (df['Direction'].isin(['Tx', 'Rx'])) &
        (pd.to_numeric(df['Rate'], errors='coerce') > 0)
        ].copy()

    if filtered_df.empty:
        print("Warning: No valid data available for plotting total throughput")
        return

    # Separate data into XPU and switch
    xpu_df = filtered_df[filtered_df['XpuId'] <= xpu_count]
    switch_df = filtered_df[filtered_df['XpuId'] > xpu_count]

    # Create figure - two subplots: XPU and switch
    fig, axes = plt.subplots(2, 1, figsize=(20, 15))
    fig.suptitle('Total Throughput per XPU/Switch (Sum of All Devices)', fontsize=16)

    # XPU total throughput
    if not xpu_df.empty:
        # Group by time, XPU ID and direction, sum
        xpu_total_df = xpu_df.groupby(['Time', 'XpuId', 'Direction'])['Rate'].sum().reset_index()

        # Plot Tx and Rx curves for each XPU
        for xpu_id, group in xpu_total_df.groupby('XpuId'):
            # Tx direction
            tx_df = group[group['Direction'] == 'Tx'].sort_values('Time')
            if not tx_df.empty:
                time_seconds = tx_df['Time'] / 1e9
                axes[0].plot(
                    time_seconds,
                    tx_df['Rate'],
                    label=f'XPU {xpu_id} Tx',
                    linewidth=2,
                    linestyle='-'
                )

            # Rx direction
            rx_df = group[group['Direction'] == 'Rx'].sort_values('Time')
            if not rx_df.empty:
                time_seconds = rx_df['Time'] / 1e9
                axes[0].plot(
                    time_seconds,
                    rx_df['Rate'],
                    label=f'XPU {xpu_id} Rx',
                    linewidth=2,
                    linestyle='--'
                )

        axes[0].set_title(f'XPU Total Throughput (XpuId <= {xpu_count})')
        axes[0].set_xlabel('Time (seconds)')
        axes[0].set_ylabel('Throughput (Gbps)')
        axes[0].legend(title='XPU & Direction', loc='upper right')
        axes[0].grid(True, linestyle=':')
        min_time = xpu_df['Time'].min() / 1e9
        max_time = xpu_df['Time'].max() / 1e9
        axes[0].set_xlim(left=min_time, right=max_time)

    # Switch total throughput
    if not switch_df.empty:
        # Group by time, switch ID and direction, sum
        switch_total_df = switch_df.groupby(['Time', 'XpuId', 'Direction'])['Rate'].sum().reset_index()

        # Plot Tx and Rx curves for each switch
        for switch_id, group in switch_total_df.groupby('XpuId'):
            # Tx direction
            tx_df = group[group['Direction'] == 'Tx'].sort_values('Time')
            if not tx_df.empty:
                time_seconds = tx_df['Time'] / 1e9
                axes[1].plot(
                    time_seconds,
                    tx_df['Rate'],
                    label=f'Switch {switch_id} Tx',
                    linewidth=2,
                    linestyle='-'
                )

            # Rx direction
            rx_df = group[group['Direction'] == 'Rx'].sort_values('Time')
            if not rx_df.empty:
                time_seconds = rx_df['Time'] / 1e9
                axes[1].plot(
                    time_seconds,
                    rx_df['Rate'],
                    label=f'Switch {switch_id} Rx',
                    linewidth=2,
                    linestyle='--'
                )

        axes[1].set_title(f'Switch Total Throughput (XpuId > {xpu_count})')
        axes[1].set_xlabel('Time (seconds)')
        axes[1].set_ylabel('Throughput (Gbps)')
        axes[1].legend(title='Switch & Direction', loc='upper right')
        axes[1].grid(True, linestyle=':')
        min_time = switch_df['Time'].min() / 1e9
        max_time = switch_df['Time'].max() / 1e9
        axes[1].set_xlim(left=min_time, right=max_time)

    plt.tight_layout(pad=3.0)

    output_file = output_dir / 'total_throughput_per_xpu_switch.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"XPU/Switch total throughput chart generated: {output_file}")

def plot_xpu_total_throughput(df, output_dir, xpu_count=4):
    """Plot XPU/Switch total throughput chart"""
    # Filter all data
    filtered_df = df[
        (df['Direction'].isin(['Tx', 'Rx'])) &
        (pd.to_numeric(df['Rate'], errors='coerce') > 0)
        ].copy()

    # Separate data into XPU and switch
    xpu_df = filtered_df[filtered_df['XpuId'] <= xpu_count]
    switch_df = filtered_df[filtered_df['XpuId'] > xpu_count]

    # Return if no data
    if xpu_df.empty and switch_df.empty:
        print("Warning: No valid data available for plotting total throughput")
        return

    # Create figure - two subplots
    fig, axes = plt.subplots(1, 2, figsize=(30, 10))
    fig.suptitle('Total Throughput Comparison', fontsize=16)

    # XPU subplot
    if not xpu_df.empty:
        # Group by XpuId and timestamp, calculate total throughput
        total_df = xpu_df.groupby(['XpuId', 'Time'])['Rate'].sum().reset_index()

        # Plot total throughput curves for each XPU
        for xpu_id, xpu_data in total_df.groupby('XpuId'):
            xpu_data = xpu_data.sort_values('Time')
            # Convert to Gbps (original Rate unit is Gbps, no conversion needed)
            throughput = xpu_data['Rate']  # Keep Gbps unit
            # Convert time unit to seconds
            time_seconds = xpu_data['Time'] / 1e9
            axes[0].plot(
                time_seconds,
                throughput,
                label=f'XPU {xpu_id}',
                linewidth=2,
                linestyle='-'
            )

        axes[0].set_title(f'XPU Total Throughput (XpuId <= {xpu_count})')
        axes[0].set_xlabel('Time (seconds)')  # Modified unit
        axes[0].set_ylabel('Throughput (Gbps)')  # Modified unit
        axes[0].legend(title='XPU ID', bbox_to_anchor=(1.05, 1))
        axes[0].grid(True, linestyle=':')
        min_time = xpu_df['Time'].min() / 1e9  # Convert to seconds
        max_time = xpu_df['Time'].max() / 1e9  # Convert to seconds
        axes[0].set_xlim(left=min_time, right=max_time)

    # Switch subplot
    if not switch_df.empty:
        # Group by XpuId and timestamp, calculate total throughput
        total_df = switch_df.groupby(['XpuId', 'Time'])['Rate'].sum().reset_index()

        # Plot total throughput curves for each switch
        for switch_id, switch_data in total_df.groupby('XpuId'):
            switch_data = switch_data.sort_values('Time')
            # Convert to Gbps (original Rate unit is Gbps, no conversion needed)
            throughput = switch_data['Rate']  # Keep Gbps unit
            # Convert time unit to seconds
            time_seconds = switch_data['Time'] / 1e9
            axes[1].plot(
                time_seconds,
                throughput,
                label=f'Switch {switch_id - xpu_count}',
                linewidth=2,
                linestyle='-'
            )

        axes[1].set_title(f'Switch Total Throughput (XpuId > {xpu_count})')
        axes[1].set_xlabel('Time (seconds)')  # Modified unit
        axes[1].set_ylabel('Throughput (Gbps)')  # Modified unit
        axes[1].legend(title='Switch ID', bbox_to_anchor=(1.05, 1))
        axes[1].grid(True, linestyle=':')
        min_time = switch_df['Time'].min() / 1e9  # Convert to seconds
        max_time = switch_df['Time'].max() / 1e9  # Convert to seconds
        axes[1].set_xlim(left=min_time, right=max_time)

    plt.tight_layout()

    output_file = output_dir / 'total_throughput_comparison.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Total throughput comparison chart generated: {output_file}")


def generate_xpu_total_stats(df, output_dir, xpu_count=4):
    """Generate XPU APP direction throughput statistics table and boxplot (only XPU part)"""
    # Filter APP direction data, specified number of XPUs
    filtered_df = df[
        (df['XpuId'] <= xpu_count) &
        (df['Direction'] == 'APP') &  # Only keep APP direction
        (pd.to_numeric(df['Rate'], errors='coerce') > 0)
        ].copy()

    if filtered_df.empty:
        print("Warning: No valid data available for generating XPU APP throughput statistics")
        return

    # Convert Rate from Mbps to Gbps (divide by 1000)
    filtered_df['Rate_Gbps'] = filtered_df['Rate'] / 1000

    # 1. Generate statistics table (only keep Gbps units)
    stats_df = filtered_df.groupby('XpuId')['Rate_Gbps'].agg(
        ['mean', 'median', 'std', 'min', 'max', 'count'])
    stats_df.columns = ['Mean (Gbps)', 'Median (Gbps)', 'Std Dev', 'Min (Gbps)', 'Max (Gbps)', 'Count']
    stats_df.reset_index(inplace=True)

    # Reorder columns (only keep Gbps related columns)
    stats_df = stats_df[['XpuId', 'Mean (Gbps)', 'Median (Gbps)',
                         'Std Dev', 'Min (Gbps)', 'Max (Gbps)', 'Count']]

    output_file = output_dir / 'xpu_app_stats.csv'
    stats_df.to_csv(output_file, index=False)
    print(f"XPU APP throughput statistics table generated: {output_file}")

    # 2. Generate boxplot (hide outliers)
    plt.figure(figsize=(12, 8))
    # Add showfliers=False parameter to hide outliers
    # sns.boxplot(data=filtered_df, x='XpuId', y='Rate_Gbps', palette='Set2', showfliers=False)
    sns.boxplot(data=filtered_df, x='XpuId', y='Rate_Gbps', hue='XpuId', palette='Set2', showfliers=False, legend=False)

    # Calculate and mark median and mean
    medians = filtered_df.groupby(['XpuId'])['Rate_Gbps'].median().values
    means = filtered_df.groupby(['XpuId'])['Rate_Gbps'].mean().values

    # Get x-axis positions
    xticks = plt.xticks()[0]

    # Mark median and mean
    for i, (median, mean) in enumerate(zip(medians, means)):
        plt.text(xticks[i], median, f'{median:.2f}Gbps',
                 horizontalalignment='center', verticalalignment='bottom', color='blue')
        # plt.text(xticks[i], mean, f'Mean: {mean:.2f}Gbps',
        #          horizontalalignment='center', verticalalignment='top', color='red')

    plt.title(f'XPU APP Throughput ')
    plt.xlabel('XPU ID')
    plt.ylabel('Throughput (Gbps)')
    plt.grid(True, linestyle=':')

    output_file = output_dir / 'xpu_app_boxplot.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"XPU APP throughput boxplot generated: {output_file}")


def generate_statistical_plots(df, output_dir, xpu_count=4):
    """Generate statistical charts (boxplots, distribution plots, etc.)"""
    # Filter all data
    filtered_df = df[
        (df['Direction'].isin(['Tx', 'Rx'])) &
        (pd.to_numeric(df['Rate'], errors='coerce') > 0)
        ].copy()

    # Separate data into XPU and switch
    xpu_df = filtered_df[filtered_df['XpuId'] <= xpu_count]
    switch_df = filtered_df[filtered_df['XpuId'] > xpu_count]

    if xpu_df.empty and switch_df.empty:
        print("Warning: No valid data available for generating statistical charts")
        return

    # TODO Total throughput needs to be reaggregated over small time intervals for all devices, not aggregated based on nanosecond timestamps - groupby(['XpuId', 'Time'])
    #
    # # 1. Boxplot - grouped by XPU/Switch
    # fig, axes = plt.subplots(1, 2, figsize=(24, 8))
    # fig.suptitle('Total Throughput Distribution Comparison', fontsize=16)

    # # XPU boxplot
    # if not xpu_df.empty:
    #     # Sum data for same XPU, same device, same timestamp
    #     summed_df = xpu_df.groupby(['XpuId', 'Time'])['Rate'].sum().reset_index()

    #     # Boxplot - grouped by XPU
    #     sns.boxplot(data=summed_df, x='XpuId', y='Rate', ax=axes[0])
    #     axes[0].set_title(f'XPU Total Throughput Distribution (XpuId <= {xpu_count})')
    #     axes[0].set_xlabel('XPU ID')
    #     axes[0].set_ylabel('Throughput (Gbps)')
    #     axes[0].grid(True, linestyle=':')

    # # Switch boxplot
    # if not switch_df.empty:
    #     # Sum data for same switch, same device, same timestamp
    #     summed_df = switch_df.groupby(['XpuId', 'Time'])['Rate'].sum().reset_index()

    #     # Boxplot - grouped by switch
    #     sns.boxplot(data=summed_df, x='XpuId', y='Rate', ax=axes[1])
    #     axes[1].set_title(f'Switch Total Throughput Distribution (XpuId > {xpu_count})')
    #     axes[1].set_xlabel('Switch ID')
    #     axes[1].set_ylabel('Throughput (Gbps)')
    #     axes[1].grid(True, linestyle=':')

    # plt.tight_layout()

    # output_file = output_dir / 'total_throughput_distribution_comparison.png'
    # plt.savefig(output_file, dpi=300, bbox_inches='tight')
    # plt.close()
    # print(f"Total throughput distribution comparison chart generated: {output_file}")

    # 2. Boxplot grouped by XPU/Switch and Device
    fig, axes = plt.subplots(1, 2, figsize=(30, 10))
    fig.suptitle('Device Throughput Distribution Comparison', fontsize=16)

    # XPU boxplot
    if not xpu_df.empty:
        # Sum data for same XPU, same device, same timestamp
        summed_df = xpu_df.groupby(['XpuId', 'DeviceId', 'Time'])['Rate'].sum().reset_index()

        sns.boxplot(data=summed_df, x='XpuId', y='Rate', hue='DeviceId', palette='Set3', ax=axes[0])
        axes[0].set_title(f'XPU & Device Throughput Distribution (XpuId <= {xpu_count})')
        axes[0].set_xlabel('XPU ID')
        axes[0].set_ylabel('Throughput (Gbps)')
        axes[0].legend(title='Device ID', bbox_to_anchor=(1.05, 1))
        axes[0].grid(True, linestyle=':')

    # Switch boxplot
    if not switch_df.empty:
        # Sum data for same switch, same device, same timestamp
        summed_df = switch_df.groupby(['XpuId', 'DeviceId', 'Time'])['Rate'].sum().reset_index()

        sns.boxplot(data=summed_df, x='XpuId', y='Rate', hue='DeviceId', palette='Set3', ax=axes[1])
        axes[1].set_title(f'Switch & Device Throughput Distribution (XpuId > {xpu_count})')
        axes[1].set_xlabel('Switch ID')
        axes[1].set_ylabel('Throughput (Gbps)')
        axes[1].legend(title='Device ID', bbox_to_anchor=(1.05, 1))
        axes[1].grid(True, linestyle=':')

    plt.tight_layout()

    output_file = output_dir / 'device_throughput_distribution_comparison.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Device throughput distribution comparison chart generated: {output_file}")
    
    # 3. Distribution plots - Fixed FacetGrid issues
    # XPU distribution plots
    if not xpu_df.empty:
        # Sum data for same XPU, same device, same timestamp
        summed_df = xpu_df.groupby(['XpuId', 'DeviceId', 'Time'])['Rate'].sum().reset_index()

        # Create figure
        plt.figure(figsize=(15, 10))
        g = sns.FacetGrid(summed_df, col='XpuId', hue='DeviceId',
                          col_wrap=2, height=5, aspect=1.5)
        g.map(sns.kdeplot, 'Rate', fill=True, alpha=0.5)
        g.add_legend(title='Device ID')
        g.fig.suptitle(f'XPU Throughput Distribution (XpuId <= {xpu_count})', y=1.02)

        # Set X-axis label units
        g.set_axis_labels('Throughput (Gbps)', 'Density')  # Add units

        output_file = output_dir / 'xpu_distribution.png'
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"XPU distribution plot generated: {output_file}")

    # Switch distribution plots
    if not switch_df.empty:
        # Sum data for same switch, same device, same timestamp
        summed_df = switch_df.groupby(['XpuId', 'DeviceId', 'Time'])['Rate'].sum().reset_index()

        # Create figure
        plt.figure(figsize=(15, 10))
        g = sns.FacetGrid(summed_df, col='XpuId', hue='DeviceId',
                          col_wrap=2, height=5, aspect=1.5)
        g.map(sns.kdeplot, 'Rate', fill=True, alpha=0.5)
        g.add_legend(title='Device ID')
        g.fig.suptitle(f'Switch Throughput Distribution (XpuId > {xpu_count})', y=1.02)

        # Set X-axis label units
        g.set_axis_labels('Throughput (Gbps)', 'Density')  # Add units

        output_file = output_dir / 'switch_distribution.png'
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"Switch distribution plot generated: {output_file}")


    # 5. Aggregate statistics tables
    if not xpu_df.empty:
        # Sum data for same XPU, same device, same timestamp
        summed_df = xpu_df.groupby(['XpuId', 'DeviceId', 'Time'])['Rate'].sum().reset_index()

        stats_df = summed_df.groupby(['XpuId', 'DeviceId'])['Rate'].agg(
            ['mean', 'median', 'std', 'min', 'max', 'count'])
        stats_df.columns = ['Mean', 'Median', 'Std Dev', 'Min', 'Max', 'Count']
        stats_df.reset_index(inplace=True)

        output_file = output_dir / 'xpu_stats.csv'
        stats_df.to_csv(output_file, index=False)
        print(f"XPU statistics table generated: {output_file}")

    if not switch_df.empty:
        # Sum data for same switch, same device, same timestamp
        summed_df = switch_df.groupby(['XpuId', 'DeviceId', 'Time'])['Rate'].sum().reset_index()

        stats_df = summed_df.groupby(['XpuId', 'DeviceId'])['Rate'].agg(
            ['mean', 'median', 'std', 'min', 'max', 'count'])
        stats_df.columns = ['Mean', 'Median', 'Std Dev', 'Min', 'Max', 'Count']
        stats_df.reset_index(inplace=True)

        output_file = output_dir / 'switch_stats.csv'
        stats_df.to_csv(output_file, index=False)
        print(f"Switch statistics table generated: {output_file}")

# Credit analysis functionality has been moved to credit_analyzer.py, please use that script for dedicated credit analysis

def plot_drop_events(df, output_dir, xpu_count=4):
    """
    Plot time variation of three packet loss types:
    - AppXpuSendDrop (XPU destination queue packet loss)
    - LinkReceiveDrop (XPU device receiver queue packet loss)
    - LinkSendDrop (XPU device sender queue packet loss)
    """
    # Define three packet loss types
    drop_types = ['AppXpuSendDrop', 'LinkReceiveDrop', 'LinkSendDrop']

    # Create figure - three subplots
    fig, axes = plt.subplots(3, 1, figsize=(15, 18))
    fig.suptitle('Packet Drop Events Analysis', fontsize=16)

    # Iterate through each packet loss type
    for i, drop_type in enumerate(drop_types):
        ax = axes[i]

        # Filter drop data for current type
        drop_df = df[df['Direction'] == drop_type].copy()

        if drop_df.empty:
            ax.text(0.5, 0.5, f'No {drop_type} Data Available',
                    ha='center', va='center', fontsize=12)
            ax.set_title(f'{drop_type} (No Data)')
            continue

        # Separate data into XPU and switch
        xpu_df = drop_df[drop_df['XpuId'] <= xpu_count]
        switch_df = drop_df[drop_df['XpuId'] > xpu_count]

        # Plot XPU data
        if not xpu_df.empty:
            for (xpu_id, dev_id), group in xpu_df.groupby(['XpuId', 'DeviceId']):
                group = group.sort_values('Time')
                # Convert time unit to seconds
                time_seconds = group['Time'] / 1e9
                ax.plot(
                    time_seconds,
                    group['Rate'],
                    label=f'XPU {xpu_id} Dev {dev_id}',
                    linewidth=1.5,
                    linestyle='-'
                )

        # Plot switch data
        if not switch_df.empty:
            for (switch_id, dev_id), group in switch_df.groupby(['XpuId', 'DeviceId']):
                group = group.sort_values('Time')
                # Convert time unit to seconds
                time_seconds = group['Time'] / 1e9
                ax.plot(
                    time_seconds,
                    group['Rate'],
                    label=f'Switch {switch_id - xpu_count} Dev {dev_id}',
                    linewidth=1.5,
                    linestyle='--'
                )

        # Set subplot properties
        ax.set_title(f'{drop_type} Events')
        ax.set_xlabel('Time (seconds)')  # Modified unit
        ax.set_ylabel('Drop Count')
        ax.legend(title='Device', loc='upper right')
        ax.grid(True, linestyle=':')

        # Set X-axis range
        min_time = drop_df['Time'].min() / 1e9  # Convert to seconds
        max_time = drop_df['Time'].max() / 1e9  # Convert to seconds
        ax.set_xlim(left=min_time, right=max_time)

        # Add data point markers
        if not drop_df.empty:
            time_seconds = drop_df['Time'] / 1e9  # Convert to seconds
            ax.scatter(time_seconds, drop_df['Rate'], s=10, alpha=0.6)

    plt.tight_layout(pad=3.0)

    output_file = output_dir / 'packet_drop_analysis.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Packet drop event analysis chart generated: {output_file}")


def main():
    """Main function: Execute network visualization analysis"""
    try:
        # Set up command line argument parsing
        parser = argparse.ArgumentParser(description='XPU device throughput analysis tool')
        parser.add_argument('--xpu_count', type=int,
                            help='Number of XPUs to analyze', default=4)
        parser.add_argument('--Name', type=str,
                            help='Data file name to plot', default=None)
        # Add new parameters
        parser.add_argument('--switch_ids', type=str,
                            help='List of switch IDs to display, comma-separated, e.g. "7,8"', default="6")
        parser.add_argument('--device_ids', type=str,
                            help='List of device IDs to display, comma-separated, e.g. "1,2"', default="1")
        parser.add_argument('--target_device_names', type=str,
                            help='List of target device names to display, comma-separated, e.g. "XPU0 Dev1,XPU1 Dev2"', default=None)
        parser.add_argument('--vc_ids', type=str,
                            help='List of VC IDs to display, comma-separated, e.g. "0,1"', default="0")

        args = parser.parse_args()

        print("Starting data processing...")

        # If no filename specified, automatically select latest file
        if args.Name is None:
            print("No filename specified, automatically finding latest file...")
            args.Name = find_latest_csv_file()
            if args.Name is None:
                print("Unable to find available data file, exiting program")
                exit(1)

        df = load_performance_data(args.Name)
        print(f"Using data file: {args.Name}")

        # Create output directory
        output_dir = create_output_directory()
        print(f"All output files will be saved in: {output_dir}")
        print(f"Analyzing {args.xpu_count} XPUs")

        # Parse new parameters
        switch_ids = None
        if args.switch_ids:
            switch_ids = [int(x.strip()) for x in args.switch_ids.split(',')]

        device_ids = None
        if args.device_ids:
            device_ids = [int(x.strip()) for x in args.device_ids.split(',')]

        target_device_names = None
        if args.target_device_names:
            target_device_names = [x.strip() for x in args.target_device_names.split(',')]

        vc_ids = None
        if args.vc_ids:
            vc_ids = [int(x.strip()) for x in args.vc_ids.split(',')]

        # Plot complete throughput charts
        plot_xpu_device_throughput(df, output_dir, args.xpu_count)

        # Plot send and receive separated throughput charts
        plot_device_throughput_Tx_Rx(df, output_dir, args.xpu_count)

        # Plot send and receive separated throughput boxplots (temporarily commented)
        # plot_device_throughput_boxplot(df, output_dir, args.xpu_count)

        # Plot XPU total throughput chart (temporarily commented)
        # plot_xpu_total_throughput(df, output_dir, args.xpu_count)

        plot_drop_events(df, output_dir, args.xpu_count)

        # Generate statistical charts
        generate_statistical_plots(df, output_dir, args.xpu_count)
        # generate_xpu_total_stats(df, output_dir, args.xpu_count)  # Temporarily commented


    except Exception as e:
        print(f"Processing error: {str(e)}")


if __name__ == "__main__":
    main()