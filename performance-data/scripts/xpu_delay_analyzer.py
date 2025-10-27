#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
XPU Delay Analysis Script for SUE-Sim Project

This script analyzes XPU end-to-end delay data and generates:
1. Delay distribution analysis (histogram, boxplot, PDF, CDF)
2. Tail latency analysis and percentiles
3. XPU and port delay comparison heatmaps
4. Statistical summaries and reports

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

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import scipy.stats as stats
from matplotlib.ticker import EngFormatter, ScalarFormatter
import os
import warnings
import datetime
import matplotlib as mpl
import glob

# Ignore warnings
warnings.filterwarnings('ignore')

# Set academic conference plotting style
sns.set_style("whitegrid")

# Solve font issues - check available fonts and set them
available_fonts = set(mpl.font_manager.findSystemFonts(fontpaths=None, fontext='ttf'))
serif_fonts = ['Times New Roman', 'DejaVu Serif', 'Liberation Serif', 'Nimbus Roman', 'FreeSerif']

# Select the first available serif font
selected_serif = None
for font in serif_fonts:
    if any(font.lower() in f.lower() for f in available_fonts):
        selected_serif = font
        break

# If no serif font found, use default font
if selected_serif is None:
    plt.rcParams.update({
        'font.family': 'sans-serif',
        'font.sans-serif': ['Arial', 'DejaVu Sans', 'Liberation Sans'],
        'font.size': 10,
        'axes.labelsize': 12,
        'axes.titlesize': 14,
        'xtick.labelsize': 10,
        'ytick.labelsize': 10,
        'legend.fontsize': 10,
        'figure.dpi': 300,
        'savefig.dpi': 300,
        'savefig.format': 'pdf',
        'savefig.bbox': 'tight',
        'pdf.fonttype': 42,  # Embed fonts in PDFs
        'ps.fonttype': 42,
    })
else:
    plt.rcParams.update({
        'font.family': 'serif',
        'font.serif': [selected_serif],
        'font.size': 10,
        'axes.labelsize': 12,
        'axes.titlesize': 14,
        'xtick.labelsize': 10,
        'ytick.labelsize': 10,
        'legend.fontsize': 10,
        'figure.dpi': 300,
        'savefig.dpi': 300,
        'savefig.format': 'pdf',
        'savefig.bbox': 'tight',
        'pdf.fonttype': 42,  # Embed fonts in PDFs
        'ps.fonttype': 42,
    })


def load_xpu_delay_data(file_path):
    """Load XPU delay data, format: (TimeNs,XpuId,PortId,Delay(ns))"""
    if not os.path.exists(file_path):
        raise FileNotFoundError(f"File not found: {file_path}")

    data = pd.read_csv(file_path)

    # Check if there is data
    if data.empty:
        print(f"Warning: File {file_path} is empty")
        return pd.DataFrame(columns=['TimeNs', 'XpuId', 'PortId', 'Delay_ns'])

    # Check if column names are correct
    required_columns = ['TimeNs', 'XpuId', 'PortId', 'Delay(ns)']
    missing_columns = [col for col in required_columns if col not in data.columns]
    if missing_columns:
        raise ValueError(f"Invalid CSV format. Missing columns: {missing_columns}. Found: {list(data.columns)}")

    # Rename columns for consistency
    data = data.rename(columns={'Delay(ns)': 'Delay_ns'})

    return data


def calculate_delay_statistics(data):
    """Calculate statistical parameters of XPU delay"""
    if data.empty:
        return pd.DataFrame()

    stats_dict = {
        'count': len(data),
        'min': np.min(data['Delay_ns']),
        'max': np.max(data['Delay_ns']),
        'mean': np.mean(data['Delay_ns']),
        'median': np.median(data['Delay_ns']),
        'std': np.std(data['Delay_ns']),
        'variance': np.var(data['Delay_ns']),
        'skewness': stats.skew(data['Delay_ns']),
        'kurtosis': stats.kurtosis(data['Delay_ns']),
        '95th': np.percentile(data['Delay_ns'], 95),
        '99th': np.percentile(data['Delay_ns'], 99),
        '99.9th': np.percentile(data['Delay_ns'], 99.9),
        '99.99th': np.percentile(data['Delay_ns'], 99.99)
    }

    # Convert statistics results to DataFrame
    stats_df = pd.DataFrame([stats_dict])
    return stats_df


def plot_delay_overview(data, output_dir, file_name):
    """Plot XPU delay overview charts"""
    if data.empty:
        print(f"Warning: No data to plot for {file_name}")
        return None

    fig, axes = plt.subplots(2, 2, figsize=(14, 12))
    fig.suptitle('XPU End-to-End Delay Analysis Overview', fontsize=16)

    # Use engineering unit formatter
    formatter = EngFormatter(unit='ns', places=1)

    # Delay histogram
    sns.histplot(data['Delay_ns'], bins=100, kde=False, ax=axes[0, 0], color='#4c72b0')
    axes[0, 0].set_xlabel('End-to-End Delay (ns)')
    axes[0, 0].set_ylabel('Count')
    axes[0, 0].set_title('Histogram of End-to-End Delay')
    axes[0, 0].grid(True, linestyle='--', alpha=0.7)
    axes[0, 0].xaxis.set_major_formatter(formatter)

    # Delay boxplot (without outliers)
    sns.boxplot(y=data['Delay_ns'], ax=axes[0, 1], color='#55a868', width=0.3, showfliers=False)
    axes[0, 1].set_ylabel('End-to-End Delay (ns)')
    axes[0, 1].set_title('Boxplot of End-to-End Delay (No Outliers)')
    axes[0, 1].grid(True, linestyle='--', alpha=0.7)
    axes[0, 1].yaxis.set_major_formatter(formatter)

    # Delay probability density function (PDF)
    sns.kdeplot(data['Delay_ns'], ax=axes[1, 0], color='#c44e52', fill=True)
    axes[1, 0].set_xlabel('End-to-End Delay (ns)')
    axes[1, 0].set_ylabel('Probability Density')
    axes[1, 0].set_title('Probability Density Function')
    axes[1, 0].grid(True, linestyle='--', alpha=0.7)
    axes[1, 0].xaxis.set_major_formatter(formatter)

    # Delay cumulative distribution function (CDF)
    sorted_data = np.sort(data['Delay_ns'])
    cdf = np.arange(1, len(sorted_data) + 1) / len(sorted_data)
    axes[1, 1].plot(sorted_data, cdf, color='#8172b3', linewidth=2)
    axes[1, 1].set_xlabel('End-to-End Delay (ns)')
    axes[1, 1].set_ylabel('Cumulative Probability')
    axes[1, 1].set_title('Cumulative Distribution Function')
    axes[1, 1].grid(True, linestyle='--', alpha=0.7)
    axes[1, 1].xaxis.set_major_formatter(formatter)

    # Add 95%, 99% and 99.9% lines
    axes[1, 1].axhline(y=0.95, color='r', linestyle='--', alpha=0.7, label='95%')
    axes[1, 1].axhline(y=0.99, color='orange', linestyle='--', alpha=0.7, label='99%')
    axes[1, 1].axhline(y=0.999, color='g', linestyle='--', alpha=0.7, label='99.9%')
    axes[1, 1].legend()

    # Adjust layout
    plt.tight_layout(rect=[0, 0, 1, 0.96])

    # Save plot
    plot_path = os.path.join(output_dir, f"{file_name}_delay_overview.pdf")
    plt.savefig(plot_path)
    plt.close()

    return plot_path




def plot_xpu_port_comparison(data, output_dir):
    """Plot XPU and port delay comparison chart"""
    if data.empty:
        return None

    # Group by XPU ID and Port ID to calculate average delay
    grouped_data = data.groupby(['XpuId', 'PortId'])['Delay_ns'].mean().reset_index()

    # Create heatmap
    pivot_data = grouped_data.pivot(index='XpuId', columns='PortId', values='Delay_ns')

    plt.figure(figsize=(12, 8))
    sns.heatmap(pivot_data, annot=True, fmt='.0f', cmap='YlOrRd',
                cbar_kws={'label': 'Average End-to-End Delay (ns)'})
    plt.title('Average End-to-End Delay by XPU and Port')
    plt.xlabel('Port ID')
    plt.ylabel('XPU ID')

    # Save plot
    plot_path = os.path.join(output_dir, "xpu_port_delay_heatmap.pdf")
    plt.savefig(plot_path)
    plt.close()

    return plot_path


def plot_tail_latency_analysis(data, output_dir, file_name):
    """Plot tail latency analysis chart"""
    if data.empty:
        return None

    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    fig.suptitle('Tail Latency Analysis', fontsize=16)

    # Calculate latency at different percentiles
    percentiles = np.linspace(90, 99.99, 100)
    latency_values = [np.percentile(data['Delay_ns'], p) for p in percentiles]

    # Tail latency curve
    axes[0].semilogy(percentiles, latency_values, 'b-', linewidth=2)
    axes[0].set_xlabel('Percentile')
    axes[0].set_ylabel('End-to-End Delay (ns)')
    axes[0].set_title('Tail Latency Curve')
    axes[0].grid(True, which='both', linestyle='--', alpha=0.7)

    # Mark key percentile points
    key_percentiles = [95, 99, 99.9, 99.99]
    key_values = [np.percentile(data['Delay_ns'], p) for p in key_percentiles]
    colors = ['red', 'orange', 'green', 'purple']
    markers = ['o', 's', '^', 'D']

    for p, v, c, m in zip(key_percentiles, key_values, colors, markers):
        axes[0].plot(p, v, color=c, marker=m, markersize=8, label=f'{p}%: {v:.0f}ns')
    axes[0].legend()

    # Violin plot of delay distribution
    sample_data = data.sample(min(10000, len(data)))  # Limit sample size for performance
    sns.violinplot(y=sample_data['Delay_ns'], ax=axes[1], color='#55a868')
    axes[1].set_ylabel('End-to-End Delay (ns)')
    axes[1].set_title('Delay Distribution (Sample)')
    axes[1].grid(True, linestyle='--', alpha=0.7)

    # Use engineering unit formatter
    formatter = EngFormatter(unit='ns', places=1)
    for ax in axes:
        ax.yaxis.set_major_formatter(formatter)

    # Adjust layout
    plt.tight_layout(rect=[0, 0, 1, 0.96])

    # Save plot
    plot_path = os.path.join(output_dir, f"{file_name}_tail_latency.pdf")
    plt.savefig(plot_path)
    plt.close()

    return plot_path


def main():
    # Set input and output directories
    current_dir = os.path.dirname(os.path.abspath(__file__))
    base_dir = os.path.dirname(current_dir)

    # Find latest XPU delay data file
    def find_latest_file(base_dir, file_pattern):
        """Find latest timestamped file"""
        pattern = os.path.join(base_dir, file_pattern)
        files = glob.glob(pattern)
        if not files:
            return None
        return max(files, key=os.path.getmtime)

    input_dir = os.path.join(base_dir, 'data', 'xpu_delay_logs')
    base_output_dir = os.path.join(base_dir, 'results', 'xpu_delay_analysis')

    # Create timestamped output directory
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    output_dir = os.path.join(base_output_dir, timestamp)
    os.makedirs(output_dir, exist_ok=True)

    # Create statistics summary file
    summary_file = os.path.join(output_dir, "analysis_summary.txt")
    with open(summary_file, 'w') as f:
        f.write(f"XPU End-to-End Delay Analysis Report - {timestamp}\n")
        f.write("="*50 + "\n\n")

    # Find latest XPU delay file
    xpu_delay_file = find_latest_file(input_dir, "xpu_delay_*.csv")

    if not xpu_delay_file:
        print(f"XPU delay file not found: {input_dir}/xpu_delay_*.csv")
        with open(summary_file, 'a') as f:
            f.write(f"XPU delay file not found: {input_dir}/xpu_delay_*.csv\n")
        return

    print(f"Analyzing XPU delay data from: {xpu_delay_file}")
    with open(summary_file, 'a') as f:
        f.write(f"Data source: {xpu_delay_file}\n\n")

    try:
        # Load XPU delay data
        delay_data = load_xpu_delay_data(xpu_delay_file)

        if delay_data.empty:
            print("No XPU delay data available")
            with open(summary_file, 'a') as f:
                f.write("No XPU delay data available\n")
            return

        print(f"Loaded {len(delay_data)} delay records")
        print(f"XPU IDs: {sorted(delay_data['XpuId'].unique())}")
        print(f"Port IDs: {sorted(delay_data['PortId'].unique())}")

        # Global statistical analysis
        print("\nAnalyzing overall delay statistics...")
        with open(summary_file, 'a') as f:
            f.write("Overall Delay Statistics\n")
            f.write("-"*40 + "\n")

        overall_stats = calculate_delay_statistics(delay_data)
        if not overall_stats.empty:
            # Save overall statistics results
            stats_path = os.path.join(output_dir, "overall_delay_stats.csv")
            overall_stats.to_csv(stats_path, index=False)
            print(f"Overall statistics saved to {stats_path}")

            # Print key statistics
            print(f"  - Total packets: {overall_stats['count'].values[0]:,}")
            print(f"  - Mean Delay: {overall_stats['mean'].values[0]:.2f} ns")
            print(f"  - 95th Percentile: {overall_stats['95th'].values[0]:.2f} ns")
            print(f"  - 99th Percentile: {overall_stats['99th'].values[0]:.2f} ns")
            print(f"  - 99.9th Percentile: {overall_stats['99.9th'].values[0]:.2f} ns")

            # Write statistics to summary file
            with open(summary_file, 'a') as f:
                f.write(f"Total packets: {overall_stats['count'].values[0]:,}\n")
                f.write(f"Mean Delay: {overall_stats['mean'].values[0]:.2f} ns\n")
                f.write(f"95th Percentile: {overall_stats['95th'].values[0]:.2f} ns\n")
                f.write(f"99th Percentile: {overall_stats['99th'].values[0]:.2f} ns\n")
                f.write(f"99.9th Percentile: {overall_stats['99.9th'].values[0]:.2f} ns\n")

        # Analyze by XPU ID grouping
        print("\nAnalyzing delay by XPU ID...")
        with open(summary_file, 'a') as f:
            f.write("\n\nDelay Analysis by XPU ID\n")
            f.write("-"*40 + "\n")

        xpu_stats_list = []
        for xpu_id in sorted(delay_data['XpuId'].unique()):
            xpu_data = delay_data[delay_data['XpuId'] == xpu_id]

            print(f"XPU {xpu_id}: {len(xpu_data)} packets")

            # Calculate statistical parameters
            xpu_stats = calculate_delay_statistics(xpu_data)
            if not xpu_stats.empty:
                xpu_stats['XpuId'] = xpu_id
                xpu_stats_list.append(xpu_stats)

                # Save statistics results for each XPU
                xpu_stats_path = os.path.join(output_dir, f"xpu_{xpu_id}_delay_stats.csv")
                xpu_stats.to_csv(xpu_stats_path, index=False)

                # Print key statistics
                print(f"  - Mean Delay: {xpu_stats['mean'].values[0]:.2f} ns")
                print(f"  - 99.9th Percentile: {xpu_stats['99.9th'].values[0]:.2f} ns")

                # Write statistics to summary file
                with open(summary_file, 'a') as f:
                    f.write(f"XPU {xpu_id}:\n")
                    f.write(f"  Packets: {xpu_stats['count'].values[0]:,}\n")
                    f.write(f"  Mean Delay: {xpu_stats['mean'].values[0]:.2f} ns\n")
                    f.write(f"  99.9th Percentile: {xpu_stats['99.9th'].values[0]:.2f} ns\n")

        # Merge all XPU statistics data
        if xpu_stats_list:
            all_xpu_stats = pd.concat(xpu_stats_list, ignore_index=True)
            all_xpu_stats_path = os.path.join(output_dir, "all_xpu_delay_stats.csv")
            all_xpu_stats.to_csv(all_xpu_stats_path, index=False)
            print(f"All XPU statistics saved to {all_xpu_stats_path}")

        # Generate plots
        print("\nGenerating plots...")
        with open(summary_file, 'a') as f:
            f.write("\n\nGenerated Plots\n")
            f.write("-"*40 + "\n")

        # 1. Delay overview plot
        try:
            overview_path = plot_delay_overview(delay_data, output_dir, "overview")
            if overview_path:
                print(f"Delay overview plot saved to {overview_path}")
                with open(summary_file, 'a') as f:
                    f.write(f"Delay overview plot: {overview_path}\n")
        except Exception as e:
            print(f"Error creating overview plot: {e}")

        # 2. XPU port comparison heatmap
        try:
            heatmap_path = plot_xpu_port_comparison(delay_data, output_dir)
            if heatmap_path:
                print(f"XPU-Port delay heatmap saved to {heatmap_path}")
                with open(summary_file, 'a') as f:
                    f.write(f"XPU-Port delay heatmap: {heatmap_path}\n")
        except Exception as e:
            print(f"Error creating heatmap plot: {e}")

        # 3. Tail latency analysis
        try:
            tail_path = plot_tail_latency_analysis(delay_data, output_dir, "tail")
            if tail_path:
                print(f"Tail latency analysis plot saved to {tail_path}")
                with open(summary_file, 'a') as f:
                    f.write(f"Tail latency analysis plot: {tail_path}\n")
        except Exception as e:
            print(f"Error creating tail latency plot: {e}")

        print(f"\nXPU delay analysis complete!")
        print(f"All results saved to: {output_dir}")
        with open(summary_file, 'a') as f:
            f.write(f"\nAnalysis complete!\n")
            f.write(f"All results saved to: {output_dir}\n")

    except Exception as e:
        print(f"Error processing XPU delay data: {e}")
        with open(summary_file, 'a') as f:
            f.write(f"Error processing XPU delay data: {e}\n")


if __name__ == "__main__":
    main()