#!/usr/bin/env python3
"""
Load Balancer Analysis Script for SUE-Sim Project

This script analyzes LoadBalancer distribution data and generates:
1. Box plots showing SUE ID distribution for each local XPU
2. Statistical analysis of load balancing effectiveness
3. Distribution fairness metrics

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
from collections import defaultdict
import matplotlib.patches as mpatches

# Set matplotlib font support for multiple character sets
plt.rcParams['font.sans-serif'] = ['DejaVu Sans', 'SimHei', 'Arial Unicode MS']
plt.rcParams['axes.unicode_minus'] = False

def find_load_balance_data():
    """Find the latest load_balance.csv file"""
    # Try multiple possible paths
    possible_paths = [
        Path("performance-data/data/load_balance_logs"),
        Path("data/load_balance_logs"),
        Path("../data/load_balance_logs"),
        Path("../performance-data/data/load_balance_logs")
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

    # Find the latest timestamped load balancing file
    load_balance_files = list(data_dir.glob("load_balance_*.csv"))
    if not load_balance_files:
        print(f"No load balancing data files found")
        return None
    
    # Sort by modification time, take the latest
    load_balance_file = max(load_balance_files, key=lambda f: f.stat().st_mtime)

    if not load_balance_file.exists():
        print(f"Load balancing data file does not exist: {load_balance_file}")
        print("Please run SUE-Sim simulation first to generate load balancing data")
        return None

    print(f"Found load balancing data file: {load_balance_file}")
    return load_balance_file

def load_and_validate_data(file_path):
    """Load and validate load balancing data"""
    try:
        df = pd.read_csv(file_path)
        print(f"Successfully loaded data with {len(df)} records")

        # Validate data format
        required_columns = ['LocalXpuId', 'DestXpuId', 'VcId', 'SueId']
        if not all(col in df.columns for col in required_columns):
            missing_cols = [col for col in required_columns if col not in df.columns]
            print(f"Data file missing required columns: {missing_cols}")
            return None

        # Display basic data information
        print(f"Data Overview:")
        print(f"   - Local XPU range: {df['LocalXpuId'].min()} - {df['LocalXpuId'].max()}")
        print(f"   - Destination XPU range: {df['DestXpuId'].min()} - {df['DestXpuId'].max()}")
        print(f"   - VC ID range: {df['VcId'].min()} - {df['VcId'].max()}")
        print(f"   - SUE ID range: {df['SueId'].min()} - {df['SueId'].max()}")
        print(f"   - Record count per local XPU:")

        for xpu_id in sorted(df['LocalXpuId'].unique()):
            count = len(df[df['LocalXpuId'] == xpu_id])
            print(f"     XPU{xpu_id}: {count} records")

        return df

    except Exception as e:
        print(f"Failed to load data: {e}")
        return None

def calculate_load_balance_metrics(df):
    """Calculate load balancing metrics"""
    metrics = {}

    # Group analysis by local XPU
    for local_xpu in sorted(df['LocalXpuId'].unique()):
        xpu_data = df[df['LocalXpuId'] == local_xpu]
        sue_counts = xpu_data['SueId'].value_counts().sort_index()

        # Calculate distribution metrics
        total_requests = len(xpu_data)
        num_sues = len(sue_counts)
        ideal_per_sue = total_requests / num_sues if num_sues > 0 else 0

        # Calculate fairness metrics
        actual_counts = sue_counts.values
        expected_counts = np.full(num_sues, ideal_per_sue)

        # Coefficient of Variation
        cv = np.std(actual_counts) / np.mean(actual_counts) if np.mean(actual_counts) > 0 else 0

        # Max/Min ratio
        max_min_ratio = np.max(actual_counts) / np.min(actual_counts) if np.min(actual_counts) > 0 else float('inf')

        # Relative Standard Deviation
        rsd = (np.std(actual_counts) / ideal_per_sue) * 100 if ideal_per_sue > 0 else 0

        metrics[local_xpu] = {
            'total_requests': total_requests,
            'num_sues': num_sues,
            'ideal_per_sue': ideal_per_sue,
            'cv': cv,
            'max_min_ratio': max_min_ratio,
            'rsd': rsd,
            'sue_counts': sue_counts
        }

    return metrics

def create_box_plot(df, output_dir):
    """Create box plots for SUE ID distribution"""
    plt.figure(figsize=(15, 10))

    # Create subplots
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('LoadBalancer SUE Distribution Analysis\n(SUE ID Distribution by Local XPU)',
                 fontsize=16, fontweight='bold')

    # Create box plots for each local XPU
    local_xpus = sorted(df['LocalXpuId'].unique())
    colors = ['#FF6B6B', '#4ECDC4', '#45B7D1', '#96CEB4', '#FECA57', '#FF9FF3']

    for i, local_xpu in enumerate(local_xpus):
        if i >= 4:  # Display maximum 4 XPUs
            break

        row = i // 2
        col = i % 2

        # Get data for this XPU
        xpu_data = df[df['LocalXpuId'] == local_xpu]

        # Collect data for each SUE ID
        sue_data = []
        sue_labels = []

        for sue_id in sorted(xpu_data['SueId'].unique()):
            sue_requests = xpu_data[xpu_data['SueId'] == sue_id]['DestXpuId'].count()
            sue_data.append([sue_requests] * sue_requests)  # Create a value for each request
            sue_labels.append(f'SUE{sue_id}')

        # Create box plot
        bp = axes[row, col].boxplot(sue_data, labels=sue_labels, patch_artist=True)

        # Set colors
        for patch, color in zip(bp['boxes'], colors[:len(sue_data)]):
            patch.set_facecolor(color)
            patch.set_alpha(0.7)

        axes[row, col].set_title(f'XPU{local_xpu} → SUE Distribution', fontsize=14, fontweight='bold')
        axes[row, col].set_xlabel('SUE ID', fontsize=12)
        axes[row, col].set_ylabel('Request Count Distribution', fontsize=12)
        axes[row, col].grid(True, alpha=0.3)

        # Add statistical information
        metrics = calculate_load_balance_metrics(df)[local_xpu]
        stats_text = f'CV: {metrics["cv"]:.3f}\nRSD: {metrics["rsd"]:.1f}%\nMax/Min: {metrics["max_min_ratio"]:.2f}'
        axes[row, col].text(0.02, 0.98, stats_text, transform=axes[row, col].transAxes,
                           verticalalignment='top', bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

    plt.tight_layout()

    # Save plot
    output_file = output_dir / 'load_balance_boxplot.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Box plot saved: {output_file}")

    return output_file

def create_distribution_heatmap(df, output_dir):
    """Create distribution heatmap"""
    # Create pivot table: rows=local XPU, columns=SUE ID, values=request count
    pivot_table = df.pivot_table(
        index='LocalXpuId',
        columns='SueId',
        values='DestXpuId',
        aggfunc='count',
        fill_value=0
    )

    plt.figure(figsize=(12, 8))

    # Create heatmap
    sns.heatmap(pivot_table,
                annot=True,
                fmt='d',
                cmap='YlOrRd',
                cbar_kws={'label': 'Request Count'})

    plt.title('LoadBalancer Distribution Heatmap\n(Request Count by Local XPU and SUE ID)',
              fontsize=16, fontweight='bold')
    plt.xlabel('SUE ID', fontsize=12)
    plt.ylabel('Local XPU ID', fontsize=12)

    # Save plot
    output_file = output_dir / 'load_balance_heatmap.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Heatmap saved: {output_file}")

    return output_file

def create_fairness_analysis(metrics, output_dir):
    """Create fairness analysis charts"""
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(15, 12))
    fig.suptitle('LoadBalancer Fairness Analysis', fontsize=16, fontweight='bold')

    local_xpus = sorted(metrics.keys())

    # 1. Coefficient of Variation (CV) - lower is better
    cv_values = [metrics[xpu]['cv'] for xpu in local_xpus]
    colors_cv = ['green' if cv < 0.1 else 'orange' if cv < 0.2 else 'red' for cv in cv_values]
    ax1.bar(local_xpus, cv_values, color=colors_cv, alpha=0.7)
    ax1.set_title('Coefficient of Variation (CV)\n(Lower is Better)', fontsize=12)
    ax1.set_xlabel('Local XPU ID')
    ax1.set_ylabel('CV Value')
    ax1.axhline(y=0.1, color='green', linestyle='--', alpha=0.7, label='Good (0.1)')
    ax1.axhline(y=0.2, color='orange', linestyle='--', alpha=0.7, label='Fair (0.2)')
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # 2. Relative Standard Deviation (RSD) - lower is better
    rsd_values = [metrics[xpu]['rsd'] for xpu in local_xpus]
    colors_rsd = ['green' if rsd < 10 else 'orange' if rsd < 20 else 'red' for rsd in rsd_values]
    ax2.bar(local_xpus, rsd_values, color=colors_rsd, alpha=0.7)
    ax2.set_title('Relative Standard Deviation (RSD %)\n(Lower is Better)', fontsize=12)
    ax2.set_xlabel('Local XPU ID')
    ax2.set_ylabel('RSD (%)')
    ax2.axhline(y=10, color='green', linestyle='--', alpha=0.7, label='Good (10%)')
    ax2.axhline(y=20, color='orange', linestyle='--', alpha=0.7, label='Fair (20%)')
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    # 3. Max/Min ratio - closer to 1 is better
    max_min_ratios = [metrics[xpu]['max_min_ratio'] for xpu in local_xpus]
    colors_ratio = ['green' if ratio < 1.5 else 'orange' if ratio < 2.0 else 'red' for ratio in max_min_ratios]
    ax3.bar(local_xpus, max_min_ratios, color=colors_ratio, alpha=0.7)
    ax3.set_title('Max/Min Ratio\n(Closer to 1 is Better)', fontsize=12)
    ax3.set_xlabel('Local XPU ID')
    ax3.set_ylabel('Max/Min Ratio')
    ax3.axhline(y=1.5, color='green', linestyle='--', alpha=0.7, label='Good (1.5)')
    ax3.axhline(y=2.0, color='orange', linestyle='--', alpha=0.7, label='Fair (2.0)')
    ax3.legend()
    ax3.grid(True, alpha=0.3)

    # 4. Overall score
    # Calculate comprehensive fairness score for each XPU (0-100)
    fairness_scores = []
    for xpu in local_xpus:
        cv_score = max(0, 100 - metrics[xpu]['cv'] * 500)  # Higher CV weight
        rsd_score = max(0, 100 - metrics[xpu]['rsd'] * 2.5)
        ratio_score = max(0, 100 - (metrics[xpu]['max_min_ratio'] - 1) * 50)
        overall_score = (cv_score + rsd_score + ratio_score) / 3
        fairness_scores.append(overall_score)

    colors_score = ['green' if score > 80 else 'orange' if score > 60 else 'red' for score in fairness_scores]
    bars = ax4.bar(local_xpus, fairness_scores, color=colors_score, alpha=0.7)
    ax4.set_title('Overall Fairness Score\n(Higher is Better)', fontsize=12)
    ax4.set_xlabel('Local XPU ID')
    ax4.set_ylabel('Score (0-100)')
    ax4.axhline(y=80, color='green', linestyle='--', alpha=0.7, label='Good (80)')
    ax4.axhline(y=60, color='orange', linestyle='--', alpha=0.7, label='Fair (60)')
    ax4.legend()
    ax4.grid(True, alpha=0.3)

    # Display values on bar chart
    for bar, score in zip(bars, fairness_scores):
        height = bar.get_height()
        ax4.text(bar.get_x() + bar.get_width()/2., height + 1,
                f'{score:.1f}', ha='center', va='bottom')

    plt.tight_layout()

    # Save plot
    output_file = output_dir / 'load_balance_fairness.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Fairness analysis chart saved: {output_file}")

    return output_file

def generate_analysis_report(df, metrics, output_dir):
    """Generate detailed analysis report"""
    report_file = output_dir / 'load_balance_analysis_report.txt'

    with open(report_file, 'w', encoding='utf-8') as f:
        f.write("=" * 80 + "\n")
        f.write("LoadBalancer Analysis Report\n")
        f.write("=" * 80 + "\n\n")

        f.write(f"Generated: {pd.Timestamp.now()}\n")
        f.write(f"Total Records: {len(df)}\n")
        f.write(f"Local XPUs: {sorted(df['LocalXpuId'].unique())}\n")
        f.write(f"SUE IDs: {sorted(df['SueId'].unique())}\n")
        f.write(f"VC IDs: {sorted(df['VcId'].unique())}\n\n")

        f.write("DETAILED ANALYSIS BY LOCAL XPU\n")
        f.write("-" * 50 + "\n\n")

        for local_xpu in sorted(metrics.keys()):
            m = metrics[local_xpu]
            f.write(f"Local XPU {local_xpu}:\n")
            f.write(f"  Total Requests: {m['total_requests']}\n")
            f.write(f"  Number of SUEs: {m['num_sues']}\n")
            f.write(f"  Ideal per SUE: {m['ideal_per_sue']:.1f}\n")
            f.write(f"  Coefficient of Variation: {m['cv']:.4f}\n")
            f.write(f"  Relative Standard Deviation: {m['rsd']:.2f}%\n")
            f.write(f"  Max/Min Ratio: {m['max_min_ratio']:.2f}\n")

            f.write("  SUE Distribution:\n")
            for sue_id, count in m['sue_counts'].items():
                percentage = (count / m['total_requests']) * 100
                deviation = ((count - m['ideal_per_sue']) / m['ideal_per_sue']) * 100
                f.write(f"    SUE {sue_id}: {count} requests ({percentage:.1f}%, {deviation:+.1f}% from ideal)\n")
            f.write("\n")

        # Summary and recommendations
        f.write("SUMMARY AND RECOMMENDATIONS\n")
        f.write("-" * 50 + "\n\n")

        avg_cv = np.mean([m['cv'] for m in metrics.values()])
        avg_rsd = np.mean([m['rsd'] for m in metrics.values()])
        avg_ratio = np.mean([m['max_min_ratio'] for m in metrics.values()])

        f.write("Overall Performance:\n")
        f.write(f"  Average CV: {avg_cv:.4f}\n")
        f.write(f"  Average RSD: {avg_rsd:.2f}%\n")
        f.write(f"  Average Max/Min Ratio: {avg_ratio:.2f}\n\n")

        f.write("Assessment:\n")
        if avg_cv < 0.1 and avg_rsd < 10 and avg_ratio < 1.5:
            f.write("  EXCELLENT: Load balancing is working very well\n")
        elif avg_cv < 0.2 and avg_rsd < 20 and avg_ratio < 2.0:
            f.write("   GOOD: Load balancing is working reasonably well\n")
        else:
            f.write("  POOR: Load balancing needs improvement\n")

        f.write("\nRecommendations:\n")
        if avg_cv > 0.2:
            f.write("  - Consider using a different hash algorithm or larger prime number\n")
        if avg_rsd > 20:
            f.write("  - Check if hash seeds are properly distributed across XPUs\n")
        if avg_ratio > 2.0:
            f.write("  - Consider increasing the number of SUE clients\n")
            f.write("  - Verify that all SUE clients are properly registered\n")

    print(f"Analysis report saved: {report_file}")
    return report_file

def main():
    """Main function"""
    print("LoadBalancer Analysis Script Starting...")

    # 1. Find data file
    data_file = find_load_balance_data()
    if data_file is None:
        sys.exit(1)

    # 2. Load and validate data
    df = load_and_validate_data(data_file)
    if df is None:
        sys.exit(1)

    # 3. Calculate load balancing metrics
    print("\nCalculating load balancing metrics...")
    metrics = calculate_load_balance_metrics(df)

    # 4. Create output directory - calculate relative path from current script location with timestamp
    from datetime import datetime
    script_dir = Path(__file__).parent.parent
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_dir = script_dir / "results" / "load_balance_analysis" / timestamp
    output_dir.mkdir(parents=True, exist_ok=True)

    # 5. Generate visualization charts
    print("\nGenerating visualization charts...")

    try:
        # Box plot
        create_box_plot(df, output_dir)

        # Heatmap
        create_distribution_heatmap(df, output_dir)

        # Fairness analysis
        create_fairness_analysis(metrics, output_dir)

    except Exception as e:
        print(f"Error generating charts: {e}")
        import traceback
        traceback.print_exc()

    # 6. Generate analysis report
    print("\nGenerating analysis report...")
    generate_analysis_report(df, metrics, output_dir)

    print(f"\nLoadBalancer analysis completed!")
    print(f"Results saved at: {output_dir}")
    print(f"   - load_balance_boxplot.png: Box plot")
    print(f"   - load_balance_heatmap.png: Heatmap")
    print(f"   - load_balance_fairness.png: Fairness analysis")
    print(f"   - load_balance_analysis_report.txt: Detailed report")

if __name__ == "__main__":
    main()