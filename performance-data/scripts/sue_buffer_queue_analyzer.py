#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SUE Buffer Queue Analysis Script for SUE-Sim Project

This script analyzes LoadBalancer SUE buffer queue data and generates:
1. Time series plots showing buffer queue size for XPU 1
2. Queue usage statistics and patterns
3. Summary statistics

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
import numpy as np
import os
import sys
from pathlib import Path
from datetime import datetime
import matplotlib.patches as mpatches

# Set matplotlib font configuration
plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'SimHei', 'Arial Unicode MS']
plt.rcParams['axes.unicode_minus'] = False

def find_sue_buffer_queue_data():
    """Find the latest sue_buffer_queue.csv file"""
    possible_paths = [
        Path("performance-data/data/sue_buffer_queue_logs"),
        Path("data/sue_buffer_queue_logs"),
        Path("../data/sue_buffer_queue_logs"),
        Path("../performance-data/data/sue_buffer_queue_logs")
    ]

    data_dir = None
    for path in possible_paths:
        if path.exists():
            data_dir = path
            break

    if data_dir is None:
        print(f"Data directory does not exist, attempted paths:")
        for path in possible_paths:
            print(f"   - {path}")
        return None

    # Find the latest timestamped SUE buffer queue file
    sue_buffer_files = list(data_dir.glob("sue_buffer_queue_*.csv"))
    if not sue_buffer_files:
        print(f"No SUE buffer queue data files found")
        return None
    
    # Sort by modification time and take the latest
    sue_buffer_file = max(sue_buffer_files, key=lambda f: f.stat().st_mtime)
    if not sue_buffer_file.exists():
        print(f"SUE buffer queue data file does not exist: {sue_buffer_file}")
        return None

    print(f"Found SUE buffer queue data file: {sue_buffer_file}")
    return sue_buffer_file

def load_and_preprocess_data(file_path):
    """Load and preprocess SUE buffer queue data"""
    try:
        df = pd.read_csv(file_path)

        # Check required columns
        required_columns = ['TimeNs', 'XpuId', 'BufferSize']
        missing_columns = [col for col in required_columns if col not in df.columns]
        if missing_columns:
            print(f"Data file missing required columns: {missing_columns}")
            print(f"Actual column names: {list(df.columns)}")
            return None

        # Convert time unit from nanoseconds to microseconds
        df['TimeUs'] = df['TimeNs'] / 1000.0

        # Filter XPU 1 data (default focus on XPU 1)
        # Note: xpuId is base-1, so XPU 1 corresponds to XpuId == 1
        xpu_1_data = df[df['XpuId'] == 1].copy()

        if xpu_1_data.empty:
            print(f"No data found for XPU 1, available XPU IDs: {sorted(df['XpuId'].unique())}")
            print(f"Note: xpuId is base-1, XPU 1 corresponds to XpuId == 1")
            print(f"Will use all available XPU data for analysis")
            return df
        else:
            print(f"Successfully loaded XPU 1 data, containing {len(xpu_1_data)} records")
            print(f"Note: XPU ID range in data: {sorted(df['XpuId'].unique())} (base-1)")
            return xpu_1_data

    except Exception as e:
        print(f"Error loading data file: {e}")
        return None

def create_buffer_queue_time_series(df, output_dir):
    """Create buffer queue size time series plot"""
    if df is None or df.empty:
        print("No data available for plotting")
        return

    plt.figure(figsize=(14, 8))

    # Check if there are multiple XPU data
    unique_xpus = sorted(df['XpuId'].unique())
    colors = plt.cm.tab10(np.linspace(0, 1, len(unique_xpus)))

    if len(unique_xpus) == 1:
        # Single XPU case
        plt.plot(df['TimeUs'], df['BufferSize'],
                 color='steelblue', linewidth=2, alpha=0.8, label='Buffer Queue Size')
        plt.fill_between(df['TimeUs'], df['BufferSize'],
                         color='steelblue', alpha=0.3)
        title_suffix = f"XPU {unique_xpus[0]}"
    else:
        # Multiple XPU case
        for i, xpu_id in enumerate(unique_xpus):
            xpu_data = df[df['XpuId'] == xpu_id]
            plt.plot(xpu_data['TimeUs'], xpu_data['BufferSize'],
                     color=colors[i], linewidth=2, alpha=0.8, label=f'XPU {xpu_id}')
        title_suffix = "All XPUs"

    plt.xlabel('Time (μs)', fontsize=12)
    plt.ylabel('Buffer Queue Size (packets)', fontsize=12)
    plt.title(f'SUE Buffer Queue Size Over Time - {title_suffix}', fontsize=14, fontweight='bold')
    plt.grid(True, alpha=0.3)
    plt.legend()

    # Add statistical information text
    max_queue = df['BufferSize'].max()
    avg_queue = df['BufferSize'].mean()
    std_queue = df['BufferSize'].std()

    stats_text = f'Max: {max_queue} packets\nAvg: {avg_queue:.2f} packets\nStd: {std_queue:.2f} packets'
    plt.text(0.02, 0.98, stats_text, transform=plt.gca().transAxes,
             fontsize=10, verticalalignment='top',
             bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))

    plt.tight_layout()

    # Save the plot
    output_file = output_dir / f'sue_buffer_queue_{title_suffix.lower().replace(" ", "_")}_timeseries.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Buffer queue time series plot saved: {output_file}")
    plt.close()

def create_buffer_queue_distribution(df, output_dir):
    """Create buffer queue size distribution analysis"""
    if df is None or df.empty:
        print("No data available for analysis")
        return

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))

    # Determine title suffix
    unique_xpus = sorted(df['XpuId'].unique())
    if len(unique_xpus) == 1:
        title_suffix = f"XPU {unique_xpus[0]}"
    else:
        title_suffix = "All XPUs"

    # 1. Histogram + KDE
    buffer_sizes = df['BufferSize'].values
    ax1.hist(buffer_sizes, bins=50, density=True, alpha=0.7, color='skyblue', edgecolor='black')

    # Add KDE curve
    from scipy import stats
    kde = stats.gaussian_kde(buffer_sizes)
    x_range = np.linspace(buffer_sizes.min(), buffer_sizes.max(), 200)
    ax1.plot(x_range, kde(x_range), 'r-', linewidth=2, label='KDE')

    ax1.set_xlabel('Buffer Queue Size (packets)', fontsize=12)
    ax1.set_ylabel('Density', fontsize=12)
    ax1.set_title(f'Buffer Queue Size Distribution - {title_suffix}', fontsize=14, fontweight='bold')
    ax1.grid(True, alpha=0.3)
    ax1.legend()

    # 2. Box plot
    ax2.boxplot(buffer_sizes, patch_artist=True)
    ax2.set_facecolor('lightblue')
    ax2.set_ylabel('Buffer Queue Size (packets)', fontsize=12)
    ax2.set_title(f'Buffer Queue Size Box Plot - {title_suffix}', fontsize=14, fontweight='bold')
    ax2.grid(True, alpha=0.3)

    # Add statistical information
    stats_text = f'Mean: {buffer_sizes.mean():.2f}\nMedian: {np.median(buffer_sizes):.2f}\n'
    stats_text += f'Std: {buffer_sizes.std():.2f}\nQ1: {np.percentile(buffer_sizes, 25):.2f}\n'
    stats_text += f'Q3: {np.percentile(buffer_sizes, 75):.2f}'
    ax2.text(0.05, 0.95, stats_text, transform=ax2.transAxes,
             fontsize=10, verticalalignment='top',
             bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))

    plt.tight_layout()

    # Save the plot
    output_file = output_dir / f'sue_buffer_queue_{title_suffix.lower().replace(" ", "_")}_distribution.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Buffer queue distribution analysis plot saved: {output_file}")
    plt.close()

def create_queue_usage_heatmap(df, output_dir):
    """Create queue usage heatmap"""
    if df is None or df.empty:
        print("No data available for analysis")
        return

    # Resample data to create heatmap
    df['TimeMs'] = df['TimeUs'] / 1000.0  # Convert to milliseconds

    # Calculate statistics for each time segment
    time_bins = np.linspace(df['TimeMs'].min(), df['TimeMs'].max(), 50)
    size_bins = np.arange(0, df['BufferSize'].max() + 1, max(1, df['BufferSize'].max() // 20))

    # Create 2D histogram
    heatmap_data, xedges, yedges = np.histogram2d(df['TimeMs'], df['BufferSize'],
                                                  bins=[time_bins, size_bins])

    plt.figure(figsize=(14, 8))

    # Draw heatmap
    im = plt.imshow(heatmap_data.T, aspect='auto', origin='lower',
                    extent=[xedges[0], xedges[-1], yedges[0], yedges[-1]],
                    cmap='YlOrRd', interpolation='nearest')

    # Determine title suffix
    unique_xpus = sorted(df['XpuId'].unique())
    if len(unique_xpus) == 1:
        title_suffix = f"XPU {unique_xpus[0]}"
    else:
        title_suffix = "All XPUs"

    plt.colorbar(im, label='Frequency')
    plt.xlabel('Time (ms)', fontsize=12)
    plt.ylabel('Buffer Queue Size (packets)', fontsize=12)
    plt.title(f'Buffer Queue Usage Heatmap - {title_suffix}', fontsize=14, fontweight='bold')
    plt.grid(True, alpha=0.3)

    plt.tight_layout()

    # Save the plot
    output_file = output_dir / f'sue_buffer_queue_{title_suffix.lower().replace(" ", "_")}_heatmap.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Buffer queue usage heatmap saved: {output_file}")
    plt.close()

def generate_summary_statistics(df, output_dir):
    """Generate statistical summary"""
    if df is None or df.empty:
        print("No data available for statistical analysis")
        return

    buffer_sizes = df['BufferSize'].values

    # Calculate statistical metrics
    stats = {
        'Metric': [
            'TotalRecords', 'TimeRange_Us_Min', 'TimeRange_Us_Max', 'TimeRange_Us_Duration',
            'BufferSize_Mean', 'BufferSize_Std', 'BufferSize_Min', 'BufferSize_Max',
            'BufferSize_Median', 'BufferSize_Q25', 'BufferSize_Q75', 'BufferSize_Q95',
            'ZeroBufferRatio', 'NonZeroBufferRatio'
        ],
        'Value': [
            len(df),
            df['TimeUs'].min(),
            df['TimeUs'].max(),
            df['TimeUs'].max() - df['TimeUs'].min(),
            buffer_sizes.mean(),
            buffer_sizes.std(),
            buffer_sizes.min(),
            buffer_sizes.max(),
            np.median(buffer_sizes),
            np.percentile(buffer_sizes, 25),
            np.percentile(buffer_sizes, 75),
            np.percentile(buffer_sizes, 95),
            (buffer_sizes == 0).sum() / len(buffer_sizes) * 100,
            (buffer_sizes > 0).sum() / len(buffer_sizes) * 100
        ]
    }

    # Convert to DataFrame and save
    stats_df = pd.DataFrame(stats)

    # Determine title suffix
    unique_xpus = sorted(df['XpuId'].unique())
    if len(unique_xpus) == 1:
        title_suffix = f"XPU {unique_xpus[0]}"
        xpu_info = f"XPU {unique_xpus[0]}"
    else:
        title_suffix = "all_xpus"
        xpu_info = f"All XPUs ({', '.join(map(str, unique_xpus))})"

    # Save CSV file
    csv_file = output_dir / f'sue_buffer_queue_{title_suffix}_statistics.csv'
    stats_df.to_csv(csv_file, index=False)
    print(f"SUE buffer queue statistical summary saved: {csv_file}")

    # Generate detailed text summary
    text_file = output_dir / f'sue_buffer_queue_{title_suffix}_analysis_report.txt'
    with open(text_file, 'w', encoding='utf-8') as f:
        f.write(f"SUE Buffer Queue Analysis Report - {xpu_info}\n")
        f.write("=" * (45 + len(xpu_info) - 8) + "\n\n")
        f.write(f"Analysis Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"Total Records: {len(df)}\n")
        f.write(f"Time Range: {df['TimeUs'].min():.2f} - {df['TimeUs'].max():.2f} μs ")
        f.write(f"(Duration: {df['TimeUs'].max() - df['TimeUs'].min():.2f} μs)\n\n")

        f.write("Buffer Queue Size Statistics:\n")
        f.write("-" * 30 + "\n")
        f.write(f"Mean: {buffer_sizes.mean():.2f} packets\n")
        f.write(f"Standard Deviation: {buffer_sizes.std():.2f} packets\n")
        f.write(f"Minimum: {buffer_sizes.min()} packets\n")
        f.write(f"Maximum: {buffer_sizes.max()} packets\n")
        f.write(f"Median: {np.median(buffer_sizes):.2f} packets\n")
        f.write(f"25th Percentile: {np.percentile(buffer_sizes, 25):.2f} packets\n")
        f.write(f"75th Percentile: {np.percentile(buffer_sizes, 75):.2f} packets\n")
        f.write(f"95th Percentile: {np.percentile(buffer_sizes, 95):.2f} packets\n\n")

        f.write("Queue Usage Patterns:\n")
        f.write("-" * 20 + "\n")
        zero_ratio = (buffer_sizes == 0).sum() / len(buffer_sizes) * 100
        f.write(f"Time with Empty Queue: {zero_ratio:.2f}%\n")
        f.write(f"Time with Non-Empty Queue: {100 - zero_ratio:.2f}%\n")

        # Analyze queue change patterns
        df_sorted = df.sort_values('TimeUs')
        queue_changes = df_sorted['BufferSize'].diff().abs().sum()
        f.write(f"Total Queue Changes: {int(queue_changes)}\n")

        if queue_changes > 0:
            avg_change_rate = queue_changes / (df['TimeUs'].max() - df['TimeUs'].min()) * 1000000  # per μs
            f.write(f"Average Change Rate: {avg_change_rate:.4f} packets/μs\n")

    print(f"SUE buffer queue analysis report saved: {text_file}")

def main():
    """Main function"""
    print("SUE Buffer Queue Data Analysis Tool")
    print("=" * 45)

    # Find data file
    data_file = find_sue_buffer_queue_data()
    if data_file is None:
        print("Cannot find data file, program exiting")
        sys.exit(1)

    # Load data
    df = load_and_preprocess_data(data_file)
    if df is None:
        print("Cannot load data, program exiting")
        sys.exit(1)

    # Create output directory
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    output_dir = Path("../results/sue_buffer_queue_analysis") / timestamp
    output_dir.mkdir(parents=True, exist_ok=True)
    print(f"Output directory: {output_dir.absolute()}")

    print("\nStarting analysis...")

    # Create plots
    print("Generating buffer queue time series plot...")
    create_buffer_queue_time_series(df, output_dir)

    print("Generating buffer queue distribution analysis plot...")
    create_buffer_queue_distribution(df, output_dir)

    print("Generating buffer queue usage heatmap...")
    create_queue_usage_heatmap(df, output_dir)

    print("Generating statistical summary...")
    generate_summary_statistics(df, output_dir)

    print(f"\nAnalysis completed!")
    print(f"All results saved to: {output_dir.absolute()}")

if __name__ == "__main__":
    main()