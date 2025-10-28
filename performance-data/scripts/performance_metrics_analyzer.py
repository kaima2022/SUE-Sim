#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SUE Protocol Performance Metrics Analysis Script

This script is used to analyze performance metrics data from SUE-Sim simulation, including:
1. Delay analysis: Queue delay statistics, distribution analysis, tail delay analysis
2. Packing analysis: Packing quantity statistics, distribution analysis, efficiency analysis
3. XPU comparison analysis: Performance comparison between XPUs

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

# Suppress warning messages
warnings.filterwarnings('ignore')

# Set scientific plotting style
sns.set_style("whitegrid")

# Font configuration: Check available fonts and set the best font
available_fonts = set(mpl.font_manager.findSystemFonts(fontpaths=None, fontext='ttf'))
serif_fonts = ['Times New Roman', 'DejaVu Serif', 'Liberation Serif', 'Nimbus Roman', 'FreeSerif']

# Select the first available serif font
selected_serif = None
for font in serif_fonts:
    if any(font.lower() in f.lower() for f in available_fonts):
        selected_serif = font
        break

# If no serif font is found, use default font
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
        'pdf.fonttype': 42,  # Embed fonts in PDF
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
        'pdf.fonttype': 42,  # Embed fonts in PDF
        'ps.fonttype': 42,
    })


def load_and_convert_data(file_path):
    """Load data and adapt to new CSV format (XpuId,WaitTime(ns))"""
    if not os.path.exists(file_path):
        raise FileNotFoundError(f"File not found: {file_path}")

    data = pd.read_csv(file_path)

    # Check if there is data
    if data.empty:
        print(f"Warning: File {file_path} is empty")
        return pd.DataFrame(columns=['XpuId', 'WaitTime(ns)'])

    # Check if column names are correct
    if 'XpuId' not in data.columns or 'WaitTime(ns)' not in data.columns:
        raise ValueError(f"Invalid CSV format. Expected columns: XpuId, WaitTime(ns). Found: {list(data.columns)}")

    # Rename columns for consistency
    data = data.rename(columns={'WaitTime(ns)': 'WaitTime_ns'})

    return data


def load_pack_num_data(file_path):
    """Load pack number data and adapt to new CSV format (XpuId,PackNums)"""
    if not os.path.exists(file_path):
        raise FileNotFoundError(f"File not found: {file_path}")

    data = pd.read_csv(file_path)

    # Check if there is data
    if data.empty:
        print(f"Warning: File {file_path} is empty")
        return pd.DataFrame(columns=['XpuId', 'PackNums'])

    # Check if column names are correct
    if 'XpuId' not in data.columns or 'PackNums' not in data.columns:
        raise ValueError(f"Invalid CSV format. Expected columns: XpuId, PackNums. Found: {list(data.columns)}")

    return data


def calculate_statistics(data):
    """Calculate statistical parameters for queueing delay"""
    if data.empty:
        return pd.DataFrame()

    stats_dict = {
        'count': len(data),
        'min': np.min(data['WaitTime_ns']),
        'max': np.max(data['WaitTime_ns']),
        'mean': np.mean(data['WaitTime_ns']),
        'median': np.median(data['WaitTime_ns']),
        'std': np.std(data['WaitTime_ns']),
        'variance': np.var(data['WaitTime_ns']),
        'skewness': stats.skew(data['WaitTime_ns']),
        'kurtosis': stats.kurtosis(data['WaitTime_ns']),
        '99th': np.percentile(data['WaitTime_ns'], 99),
        '99.9th': np.percentile(data['WaitTime_ns'], 99.9),
        '99.99th': np.percentile(data['WaitTime_ns'], 99.99)
    }

    # Convert statistical results to DataFrame
    stats_df = pd.DataFrame([stats_dict])
    return stats_df


def calculate_pack_stats(data):
    """Calculate statistical parameters for pack numbers"""
    if data.empty:
        return pd.DataFrame()

    stats_dict = {
        'count': len(data),
        'min': np.min(data['PackNums']),
        'max': np.max(data['PackNums']),
        'mean': np.mean(data['PackNums']),
        'median': np.median(data['PackNums']),
        'std': np.std(data['PackNums']),
        'variance': np.var(data['PackNums']),
        'skewness': stats.skew(data['PackNums']),
        'kurtosis': stats.kurtosis(data['PackNums']),
        '95th': np.percentile(data['PackNums'], 95),
        '99th': np.percentile(data['PackNums'], 99),
        'total_packets': np.sum(data['PackNums'])
    }

    # Convert statistical results to DataFrame
    stats_df = pd.DataFrame([stats_dict])
    return stats_df


def plot_delay_distribution(data, output_dir, file_name):
    """Plot queueing delay distribution charts"""
    if data.empty:
        print(f"Warning: No data to plot for {file_name}")
        return None

    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    fig.suptitle('Queueing Delay Distribution Analysis', fontsize=16)

    # Histogram
    sns.histplot(data['WaitTime_ns'], bins=100, kde=False, ax=axes[0, 0], color='#4c72b0')
    axes[0, 0].set_xlabel('Queueing Delay (ns)')
    axes[0, 0].set_ylabel('Count')
    axes[0, 0].set_title('Histogram of Queueing Delay')
    axes[0, 0].grid(True, linestyle='--', alpha=0.7)

    # Use engineering unit formatter
    formatter = EngFormatter(unit='ns', places=1)
    axes[0, 0].xaxis.set_major_formatter(formatter)

    # Boxplot
    sns.boxplot(y=data['WaitTime_ns'], ax=axes[0, 1], color='#55a868', width=0.3)
    axes[0, 1].set_ylabel('Queueing Delay (ns)')
    axes[0, 1].set_title('Boxplot of Queueing Delay')
    axes[0, 1].grid(True, linestyle='--', alpha=0.7)
    axes[0, 1].yaxis.set_major_formatter(formatter)

    # Probability Density Function (PDF)
    sns.kdeplot(data['WaitTime_ns'], ax=axes[1, 0], color='#c44e52', fill=True)
    axes[1, 0].set_xlabel('Queueing Delay (ns)')
    axes[1, 0].set_ylabel('Probability Density')
    axes[1, 0].set_title('Probability Density Function')
    axes[1, 0].grid(True, linestyle='--', alpha=0.7)
    axes[1, 0].xaxis.set_major_formatter(formatter)

    # Cumulative Distribution Function (CDF)
    sorted_data = np.sort(data['WaitTime_ns'])
    cdf = np.arange(1, len(sorted_data) + 1) / len(sorted_data)
    axes[1, 1].plot(sorted_data, cdf, color='#8172b3', linewidth=2)
    axes[1, 1].set_xlabel('Queueing Delay (ns)')
    axes[1, 1].set_ylabel('Cumulative Probability')
    axes[1, 1].set_title('Cumulative Distribution Function')
    axes[1, 1].grid(True, linestyle='--', alpha=0.7)
    axes[1, 1].xaxis.set_major_formatter(formatter)

    # Add 99% and 99.9% lines
    # axes[1, 1].axhline(y=0.99, color='r', linestyle='--', alpha=0.7)
    axes[1, 1].axhline(y=0.999, color='g', linestyle='--', alpha=0.7)
    # axes[1, 1].text(sorted_data[-1] * 0.8, 0.99, '99%', color='r', fontsize=10)
    axes[1, 1].text(sorted_data[-1] * 0.8, 0.999, '99.9%', color='g', fontsize=10)

    # Adjust layout
    plt.tight_layout(rect=[0, 0, 1, 0.96])

    # Save plot
    plot_path = os.path.join(output_dir, f"{file_name}_delay_distribution.pdf")
    plt.savefig(plot_path)
    plt.close()

    return plot_path


def plot_tail_latency(data, output_dir, file_name):
    """Plot tail latency analysis chart"""
    if data.empty:
        return None

    # Calculate delay at different percentiles
    percentiles = np.linspace(90, 99.99, 100)
    latency_values = [np.percentile(data['WaitTime_ns'], p) for p in percentiles]

    plt.figure(figsize=(8, 6))

    # Main plot - Tail latency curve
    plt.semilogy(percentiles, latency_values, 'b-', linewidth=2)
    plt.xlabel('Percentile')
    plt.ylabel('Queueing Delay (ns)')
    plt.title('Tail Latency Analysis')
    plt.grid(True, which='both', linestyle='--', alpha=0.7)

    # Save plot
    plot_path = os.path.join(output_dir, f"{file_name}_tail_latency.pdf")
    plt.savefig(plot_path)
    plt.close()

    return plot_path


def plot_comparison_chart(all_stats, output_dir):
    """Plot delay comparison chart for all XPUs"""
    if not all_stats:
        return None

    # Merge all statistical data
    combined_stats = pd.concat(all_stats, ignore_index=True)

    # Create comparison chart
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))

    # Average delay comparison
    sns.barplot(x='xpu_id', y='mean', data=combined_stats, ax=axes[0], palette='viridis')
    axes[0].set_title('Average Queueing Delay Comparison')
    axes[0].set_xlabel('XPU ID')
    axes[0].set_ylabel('Mean Delay (ns)')

    # 99.9% delay comparison
    sns.barplot(x='xpu_id', y='99.9th', data=combined_stats, ax=axes[1], palette='viridis')
    axes[1].set_title('99.9th Percentile Queueing Delay Comparison')
    axes[1].set_xlabel('XPU ID')
    axes[1].set_ylabel('99.9th Percentile Delay (ns)')

    # Adjust layout
    plt.tight_layout()

    # Save plot
    plot_path = os.path.join(output_dir, "xpu_delay_comparison.pdf")
    plt.savefig(plot_path)
    plt.close()

    return plot_path


def plot_pack_distribution(data, output_dir, file_name):
    """Plot pack number distribution charts"""
    if data.empty:
        print(f"Warning: No pack data to plot for {file_name}")
        return None

    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    fig.suptitle('Pack Number Distribution Analysis', fontsize=16)

    # Histogram
    sns.histplot(data['PackNums'], bins=50, kde=False, ax=axes[0, 0], color='#4c72b0')
    axes[0, 0].set_xlabel('Pack Number')
    axes[0, 0].set_ylabel('Count')
    axes[0, 0].set_title('Histogram of Pack Numbers')
    axes[0, 0].grid(True, linestyle='--', alpha=0.7)

    # Boxplot
    sns.boxplot(y=data['PackNums'], ax=axes[0, 1], color='#55a868', width=0.3)
    axes[0, 1].set_ylabel('Pack Number')
    axes[0, 1].set_title('Boxplot of Pack Numbers')
    axes[0, 1].grid(True, linestyle='--', alpha=0.7)

    # Probability Density Function (PDF)
    sns.kdeplot(data['PackNums'], ax=axes[1, 0], color='#c44e52', fill=True)
    axes[1, 0].set_xlabel('Pack Number')
    axes[1, 0].set_ylabel('Probability Density')
    axes[1, 0].set_title('Probability Density Function')
    axes[1, 0].grid(True, linestyle='--', alpha=0.7)

    # Cumulative Distribution Function (CDF)
    sorted_data = np.sort(data['PackNums'])
    cdf = np.arange(1, len(sorted_data) + 1) / len(sorted_data)
    axes[1, 1].plot(sorted_data, cdf, color='#8172b3', linewidth=2)
    axes[1, 1].set_xlabel('Pack Number')
    axes[1, 1].set_ylabel('Cumulative Probability')
    axes[1, 1].set_title('Cumulative Distribution Function')
    axes[1, 1].grid(True, linestyle='--', alpha=0.7)

    # Add 95% and 99% lines
    axes[1, 1].axhline(y=0.95, color='r', linestyle='--', alpha=0.7)
    axes[1, 1].axhline(y=0.99, color='g', linestyle='--', alpha=0.7)
    axes[1, 1].text(sorted_data[-1] * 0.8, 0.95, '95%', color='r', fontsize=10)
    axes[1, 1].text(sorted_data[-1] * 0.8, 0.99, '99%', color='g', fontsize=10)

    # Adjust layout
    plt.tight_layout(rect=[0, 0, 1, 0.96])

    # Save plot
    plot_path = os.path.join(output_dir, f"{file_name}_pack_distribution.pdf")
    plt.savefig(plot_path)
    plt.close()

    return plot_path


def plot_pack_comparison(all_pack_stats, output_dir):
    """Plot pack number comparison chart for all XPUs"""
    if not all_pack_stats:
        return None

    # Merge all statistical data
    combined_stats = pd.concat(all_pack_stats, ignore_index=True)

    # Create comparison chart
    fig, axes = plt.subplots(1, 3, figsize=(18, 6))

    # Average pack number comparison
    sns.barplot(x='xpu_id', y='mean', data=combined_stats, ax=axes[0], palette='viridis')
    axes[0].set_title('Average Pack Number Comparison')
    axes[0].set_xlabel('XPU ID')
    axes[0].set_ylabel('Mean Pack Number')

    # 99% pack number comparison
    sns.barplot(x='xpu_id', y='99th', data=combined_stats, ax=axes[1], palette='viridis')
    axes[1].set_title('99th Percentile Pack Number Comparison')
    axes[1].set_xlabel('XPU ID')
    axes[1].set_ylabel('99th Percentile Pack Number')

    # Total packets comparison
    sns.barplot(x='xpu_id', y='total_packets', data=combined_stats, ax=axes[2], palette='viridis')
    axes[2].set_title('Total Packets Comparison')
    axes[2].set_xlabel('XPU ID')
    axes[2].set_ylabel('Total Packets')

    # Adjust layout
    plt.tight_layout()

    # Save plot
    plot_path = os.path.join(output_dir, "xpu_pack_comparison.pdf")
    plt.savefig(plot_path)
    plt.close()

    return plot_path


def main():
    """Main function: Execute performance metrics analysis"""
    # Set input and output directories, using structured directory layout
    current_dir = os.path.dirname(os.path.abspath(__file__))
    base_dir = os.path.dirname(current_dir)

    # Find the latest timestamped data files
    def find_latest_file(base_dir, file_pattern):
        """Find the latest timestamped data files"""
        pattern = os.path.join(base_dir, file_pattern)
        files = glob.glob(pattern)
        if not files:
            return None
        return max(files, key=os.path.getctime)

    delay_input_dir = os.path.join(base_dir, 'data', 'wait_time_logs')
    pack_input_dir = os.path.join(base_dir, 'data', 'pack_num_logs')
    base_output_dir = os.path.join(base_dir, 'results', 'performance_metrics')

    # Create timestamped output directory
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    output_dir = os.path.join(base_output_dir, timestamp)
    os.makedirs(output_dir, exist_ok=True)

    # Create analysis summary file
    summary_file = os.path.join(output_dir, "analysis_summary.txt")
    with open(summary_file, 'w') as f:
        f.write(f"SUE Performance Analysis Report - {timestamp}\n")
        f.write("="*50 + "\n\n")

    # Find the latest wait time files
    wait_time_file = find_latest_file(delay_input_dir, "wait_time_*.csv")
    pack_num_file = find_latest_file(pack_input_dir, "pack_num_*.csv")

    
    if not os.path.exists(wait_time_file):
        print(f"Wait time file not found: {wait_time_file}")
        with open(summary_file, 'a') as f:
            f.write(f"Wait time file not found: {wait_time_file}\n")
        return

    if not os.path.exists(pack_num_file):
        print(f"Pack number file not found: {pack_num_file}")
        with open(summary_file, 'a') as f:
            f.write(f"Pack number file not found: {pack_num_file}\n")
        return

    all_stats = []  # Store delay statistics for all XPUs
    all_pack_stats = []  # Store pack statistics for all XPUs

    print("Analyzing wait time data...")
    with open(summary_file, 'a') as f:
        f.write("Wait Time Analysis\n")
        f.write("-"*40 + "\n")

    try:
        # Load wait time data
        delay_data = load_and_convert_data(wait_time_file)

        if delay_data.empty:
            print("No wait time data available")
            with open(summary_file, 'a') as f:
                f.write("No wait time data available\n")
        else:
            # Process by XPU ID groups
            for xpu_id in sorted(delay_data['XpuId'].unique()):
                xpu_delay_data = delay_data[delay_data['XpuId'] == xpu_id]

                print(f"Analyzing queueing delay for XPU {xpu_id}")
                with open(summary_file, 'a') as f:
                    f.write(f"\nXPU {xpu_id} Analysis:\n")
                    f.write("-"*30 + "\n")

                # Calculate statistical parameters
                stats_df = calculate_statistics(xpu_delay_data)

                if stats_df.empty:
                    print(f"  - No statistics calculated (empty data)")
                    continue

                # Add XPU ID to statistical data
                stats_df['xpu_id'] = xpu_id

                # Save statistical results
                stats_path = os.path.join(output_dir, f"wait_time_stats_xpu_{xpu_id}.csv")
                stats_df.to_csv(stats_path, index=False)
                print(f"  - Statistics saved to {stats_path}")

                # Add to all statistical data
                all_stats.append(stats_df)

                # Plot distribution chart
                try:
                    dist_plot_path = plot_delay_distribution(xpu_delay_data, output_dir, f"xpu_{xpu_id}")
                    print(f"  - Distribution plot saved to {dist_plot_path}")
                except Exception as e:
                    print(f"  - Error plotting distribution: {e}")

                # Plot tail latency chart
                try:
                    tail_plot_path = plot_tail_latency(xpu_delay_data, output_dir, f"xpu_{xpu_id}")
                    if tail_plot_path:
                        print(f"  - Tail latency plot saved to {tail_plot_path}")
                except Exception as e:
                    print(f"  - Error plotting tail latency: {e}")

                # Print key statistical information
                print(f"  - Mean Delay: {stats_df['mean'].values[0]:.2f} ns")
                print(f"  - 99.9th Percentile: {stats_df['99.9th'].values[0]:.2f} ns")

                # Write key statistical information to summary file
                with open(summary_file, 'a') as f:
                    f.write(f"Mean Delay: {stats_df['mean'].values[0]:.2f} ns\n")
                    f.write(f"99.9th Percentile: {stats_df['99.9th'].values[0]:.2f} ns\n")

    except Exception as e:
        print(f"Error processing wait time data: {e}")
        with open(summary_file, 'a') as f:
            f.write(f"Error processing wait time data: {e}\n")

    # Analyze pack number data
    if os.path.exists(pack_num_file):
        print("\nAnalyzing pack number data...")
        with open(summary_file, 'a') as f:
            f.write("\n\nPack Number Analysis\n")
            f.write("-"*40 + "\n")

        try:
            # Load pack number data
            pack_data = load_pack_num_data(pack_num_file)

            if pack_data.empty:
                print("No pack number data available")
                with open(summary_file, 'a') as f:
                    f.write("No pack number data available\n")
            else:
                # Process by XPU ID groups
                for xpu_id in sorted(pack_data['XpuId'].unique()):
                    xpu_pack_data = pack_data[pack_data['XpuId'] == xpu_id]

                    print(f"Analyzing pack numbers for XPU {xpu_id}")
                    with open(summary_file, 'a') as f:
                        f.write(f"\nXPU {xpu_id} Pack Analysis:\n")
                        f.write("-"*30 + "\n")

                    # Calculate statistical parameters
                    pack_stats_df = calculate_pack_stats(xpu_pack_data)

                    if pack_stats_df.empty:
                        print(f"  - No pack statistics calculated (empty data)")
                        continue

                    # Add XPU ID to statistical data
                    pack_stats_df['xpu_id'] = xpu_id

                    # Save statistical results
                    pack_stats_path = os.path.join(output_dir, f"pack_num_stats_xpu_{xpu_id}.csv")
                    pack_stats_df.to_csv(pack_stats_path, index=False)
                    print(f"  - Pack statistics saved to {pack_stats_path}")

                    # Add to all statistical data
                    all_pack_stats.append(pack_stats_df)

                    # Plot pack distribution chart
                    try:
                        pack_dist_path = plot_pack_distribution(xpu_pack_data, output_dir, f"xpu_{xpu_id}")
                        if pack_dist_path:
                            print(f"  - Pack distribution plot saved to {pack_dist_path}")
                    except Exception as e:
                        print(f"  - Error plotting pack distribution: {e}")

                    # Print key statistical information
                    print(f"  - Mean Pack Number: {pack_stats_df['mean'].values[0]:.2f}")
                    print(f"  - Total Packets: {pack_stats_df['total_packets'].values[0]}")

                    # Write key statistical information to summary file
                    with open(summary_file, 'a') as f:
                        f.write(f"Mean Pack Number: {pack_stats_df['mean'].values[0]:.2f}\n")
                        f.write(f"Total Packets: {pack_stats_df['total_packets'].values[0]}\n")

        except Exception as e:
            print(f"Error processing pack number data: {e}")
            with open(summary_file, 'a') as f:
                f.write(f"Error processing pack number data: {e}\n")

    # Plot comparison charts for all XPUs
    if all_stats:
        try:
            comp_plot_path = plot_comparison_chart(all_stats, output_dir)
            if comp_plot_path:
                print(f"Delay comparison chart saved to {comp_plot_path}")
                with open(summary_file, 'a') as f:
                    f.write(f"\nDelay comparison chart saved to {comp_plot_path}\n")
        except Exception as e:
            print(f"Error creating delay comparison chart: {e}")

    if all_pack_stats:
        try:
            pack_comp_path = plot_pack_comparison(all_pack_stats, output_dir)
            if pack_comp_path:
                print(f"Pack comparison chart saved to {pack_comp_path}")
                with open(summary_file, 'a') as f:
                    f.write(f"Pack comparison chart saved to {pack_comp_path}\n")
        except Exception as e:
            print(f"Error creating pack comparison chart: {e}")

    print(f"\nAnalysis complete!")
    print(f"All results saved to: {output_dir}")
    with open(summary_file, 'a') as f:
        f.write(f"\nAnalysis complete!\n")
        f.write(f"All results saved to: {output_dir}\n")


if __name__ == "__main__":
    main()