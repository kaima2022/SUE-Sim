#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SUE Protocol Performance Data Analysis Platform Project Information

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

# Project metadata
PROJECT_NAME = "SUE Protocol Performance Data Analysis Platform"
VERSION = "1.0.0"
DESCRIPTION = "Complete data analysis platform designed for SUE protocol performance analysis in ns-3.33 network simulator"

# Author information
AUTHOR = "SUE Protocol Development Team"
MAINTAINER = "SUE Protocol Development Team"

# Project structure configuration
DIRECTORY_STRUCTURE = {
    "scripts": {
        "description": "Analysis scripts directory",
        "files": {
            "performance_analyzer.py": "Main performance analysis script",
            "packet_loss_calculator.py": "Packet loss calculation script",
            "packet_analyzer.py": "Packet analysis script",
            "visualizer.py": "Basic visualization script",
            "main_visualizer.py": "Main visualization script",
            "checkpoint_analyzer.py": "Checkpoint analysis script"
        }
    },
    "data": {
        "description": "Raw data directory",
        "subdirs": {
            "performance_logs": "Performance log files",
            "packing_logs": "Packing log files",
            "legacy": "Historical data files"
        }
    },
    "results": {
        "description": "Analysis results directory",
        "subdirs": {
            "analysis_reports": "Analysis reports",
            "visualizations": "Visualization charts",
            "statistics": "Statistical data"
        }
    },
    "config": {
        "description": "Configuration files directory"
    }
}

# Data format specifications
DATA_FORMATS = {
    "wait_time": {
        "file": "wait_time.csv",
        "columns": ["XpuId", "WaitTime(ns)"],
        "description": "Wait time data in nanoseconds"
    },
    "pack_num": {
        "file": "pack_num.csv",
        "columns": ["XpuId", "PackNums"],
        "description": "Packing quantity data"
    }
}

# Analysis metrics configuration
METRICS = {
    "delay": {
        "statistics": ["mean", "median", "std", "min", "max", "99th", "99.9th", "99.99th"],
        "unit": "nanoseconds",
        "description": "Delay related metrics"
    },
    "packing": {
        "statistics": ["mean", "median", "std", "min", "max", "95th", "99th", "total"],
        "unit": "count",
        "description": "Packing related metrics"
    }
}

# Visualization configuration
VISUALIZATION = {
    "delay_distribution": {
        "type": "multi_plot",
        "subplots": ["histogram", "boxplot", "pdf", "cdf"],
        "output_format": "pdf"
    },
    "pack_distribution": {
        "type": "multi_plot",
        "subplots": ["histogram", "boxplot", "pdf", "cdf"],
        "output_format": "pdf"
    },
    "comparison": {
        "type": "bar_chart",
        "categories": ["delay", "packing"],
        "output_format": "pdf"
    }
}

# Dependency package list
DEPENDENCIES = [
    "numpy>=1.19.0",
    "pandas>=1.3.0",
    "matplotlib>=3.3.0",
    "seaborn>=0.11.0",
    "scipy>=1.5.0"
]

# Supported Python version
PYTHON_VERSION = ">=3.6"

# License
LICENSE = "Apache-2.0"

# Creation time
CREATED_DATE = "2025-10-01"
LAST_UPDATED = "2025-10-01"

def get_project_info():
    """Return basic project information"""
    return {
        "name": PROJECT_NAME,
        "version": VERSION,
        "description": DESCRIPTION,
        "author": AUTHOR,
        "created": CREATED_DATE,
        "updated": LAST_UPDATED
    }

def get_directory_structure():
    """Return directory structure information"""
    return DIRECTORY_STRUCTURE

def get_data_formats():
    """Return data format specifications"""
    return DATA_FORMATS

def print_project_info():
    """Print project information"""
    info = get_project_info()
    print(f"Project Name: {info['name']}")
    print(f"Version: {info['version']}")
    print(f"Description: {info['description']}")
    print(f"Author: {info['author']}")
    print(f"Created: {info['created']}")
    print(f"Last Updated: {info['updated']}")

if __name__ == "__main__":
    print_project_info()