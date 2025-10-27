#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SUE Protocol Packet Loss Analysis Script

This script analyzes packet loss data from SUE-Sim simulation logs and generates:
1. Network transmission loss rate analysis
2. Receiver drop rate analysis
3. Total system loss rate analysis
4. Statistical reports and summaries

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

import re
import os
import sys
import argparse
from datetime import datetime
from pathlib import Path

def create_output_directory():
    """Create packet loss analysis output directory"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_dir = Path("../results") / "loss_analysis" / timestamp
    output_dir.mkdir(parents=True, exist_ok=True)
    return output_dir

def calculate_packet_loss(log_file):
    total_sent = 0
    total_received = 0
    total_dropped = 0
    
    with open(log_file, 'r') as file:
        for line in file:
            # Process sent packet data
            if 'Summary: Sent' in line:
                match = re.search(r'Sent (\d+) packets', line)
                if match:
                    total_sent += int(match.group(1))
            
            # Process received packet data
            elif 'Received' in line and 'Port' in line:
                match = re.search(r'Received (\d+) packets', line)
                if match:
                    total_received += int(match.group(1))
            
            # Process dropped packet data
            elif 'Dropped' in line and 'Port' in line:
                match = re.search(r'Dropped (\d+) packets', line)
                if match:
                    total_dropped += int(match.group(1))
    
    # Temporary fix: Add switch packet loss calculation
    # If sent > recv and dropped == 0, the difference is switch packet loss
    if total_sent > total_received and total_dropped == 0:
        total_dropped = total_sent - total_received
    
    # Calculate various packet loss rates
    if total_sent == 0:
        return "Error: No sent packet data found"
    
    # Network transmission loss rate (packets sent but not received)
    network_loss_rate = (total_sent - total_received) / total_sent * 100
    
    # Receiver drop rate (packets received but dropped)
    if total_received == 0:
        receiver_loss_rate = 0
    else:
        receiver_loss_rate = total_dropped / total_received * 100
    
    # Total loss rate (packets sent but not successfully processed)
    total_loss_rate = (total_sent - (total_received - total_dropped)) / total_sent * 100
    
    return total_sent, total_received, total_dropped, network_loss_rate, receiver_loss_rate, total_loss_rate

def save_loss_analysis(output_dir, log_file, results):
    """Save packet loss analysis results to file"""
    if isinstance(results, str):
        # Error case
        error_file = output_dir / "error_report.txt"
        with open(error_file, 'w') as f:
            f.write(f"Error Analysis Report - {datetime.now()}\n")
            f.write("=" * 40 + "\n\n")
            f.write(f"Log File: {log_file}\n")
            f.write(f"Error: {results}\n")
        print(f"Error report saved to: {error_file}")
    else:
        # Success case
        sent, recv, dropped, net_loss, recv_loss, total_loss = results

        # Save detailed analysis report
        report_file = output_dir / "loss_analysis_report.txt"
        with open(report_file, 'w') as f:
            f.write(f"Packet Loss Analysis Report - {datetime.now()}\n")
            f.write("=" * 40 + "\n\n")
            f.write(f"Log File: {log_file}\n\n")
            f.write("Analysis Results:\n")
            f.write("-" * 20 + "\n")
            f.write(f"Total Sent Packets: {sent}\n")
            f.write(f"Total Received Packets: {recv}\n")
            f.write(f"Total Dropped Packets: {dropped}\n\n")
            f.write("Loss Rates:\n")
            f.write("-" * 20 + "\n")
            f.write(f"Network Transmission Loss Rate: {net_loss:.4f}%\n")
            f.write(f"Receiver Drop Rate: {recv_loss:.4f}%\n")
            f.write(f"Total System Loss Rate: {total_loss:.4f}%\n")

        # Save CSV format data
        csv_file = output_dir / "loss_statistics.csv"
        with open(csv_file, 'w') as f:
            f.write("Metric,Count,Rate(%)\n")
            f.write(f"Total Sent,{sent},100.0000\n")
            f.write(f"Total Received,{recv},{(recv/sent*100):.4f}\n")
            f.write(f"Total Dropped,{dropped},{(dropped/sent*100):.4f}\n")
            f.write(f"Network Loss,{sent-recv},{net_loss:.4f}\n")
            f.write(f"Receiver Loss,{dropped},{recv_loss:.4f}\n")
            f.write(f"Total Loss,{sent-(recv-dropped)},{total_loss:.4f}\n")

        print(f"Analysis report saved to: {report_file}")
        print(f"Statistics data saved to: {csv_file}")

def find_default_log_file():
    """Find default log file"""
    # Try to find common log file names (from scripts directory)
    possible_files = [
        Path("../../log") / "sue-sim.log",
        Path("../../log") / "sue-sim.log",
        Path("../../log") / "app.log"
    ]

    # First look for log files containing valid data
    log_dir = Path("../../log")
    if log_dir.exists():
        log_files = list(log_dir.glob("*.log"))

        # Look for log files containing valid data by priority
        for file_path in possible_files:
            if file_path.exists() and file_path in log_files:
                if contains_valid_data(file_path):
                    print(f"Using default log file: {file_path}")
                    return str(file_path)

        # Look for other log files containing valid data
        for log_file in log_files:
            if contains_valid_data(log_file):
                print(f"Found valid data log file: {log_file}")
                return str(log_file)

        # If no files with valid data found, select the newest file
        if log_files:
            latest_file = max(log_files, key=lambda x: x.stat().st_mtime)
            print(f"No valid data log files found, using latest log file: {latest_file}")
            return str(latest_file)

    print("No available log files found")
    return None

def contains_valid_data(file_path):
    """Check if log file contains valid packet loss analysis data"""
    try:
        with open(file_path, 'r') as file:
            content = file.read()
            # Check if content contains keywords related to sending, receiving, or dropping packets
            return any(keyword in content for keyword in [
                'Summary: Sent', 'Received', 'Dropped',
                'Sent', 'packets', 'Total'
            ])
    except:
        return False

def main():
    """Main function"""
    # Setup command line argument parsing
    parser = argparse.ArgumentParser(description='SUE Protocol Packet Loss Analysis Tool')
    parser.add_argument('log_file', nargs='?',
                       help='Log file path (optional, default: ../log/sue-sim.log)')

    args = parser.parse_args()

    # Determine the log file to analyze
    if args.log_file is None:
        print("No log file specified, trying to use default file...")
        log_file = find_default_log_file()
        if log_file is None:
            print("Unable to find available log files, please specify log file path manually")
            sys.exit(1)
    else:
        log_file = args.log_file

    # Check if file exists
    if not os.path.exists(log_file):
        print(f"Error: Log file does not exist: {log_file}")
        sys.exit(1)

    try:
        # Create output directory
        output_dir = create_output_directory()
        print(f"Analysis results will be saved to: {output_dir}")
        print(f"Using log file: {log_file}")

        # Calculate packet loss statistics
        results = calculate_packet_loss(log_file)

        # Save analysis results
        save_loss_analysis(output_dir, log_file, results)

        # Console output summary
        if isinstance(results, str):
            print(f"{results}")
        else:
            sent, recv, dropped, net_loss, recv_loss, total_loss = results
            print("Analysis completed!")
            print(f"Total sent packets: {sent}")
            print(f"Total received packets: {recv}")
            print(f"Total dropped packets: {dropped}")
            print(f"Network transmission loss rate: {net_loss:.2f}%")
            print(f"Receiver drop rate: {recv_loss:.2f}%")
            print(f"Total system loss rate: {total_loss:.2f}%")

    except Exception as e:
        print(f"Error processing log file: {e}")
        # Try to save error information
        try:
            output_dir = create_output_directory()
            save_loss_analysis(output_dir, log_file, str(e))
        except:
            pass

# Usage example
if __name__ == "__main__":
    main()