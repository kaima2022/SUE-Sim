<div align="center">

# **SUE-Sim: End-to-End Scale-Up Ethernet Simulation Platform**

---

</div>

[![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)
[![ns--3-3.36/3.44](https://img.shields.io/badge/ns--3-3.36%2F3.44-green.svg)](https://www.nsnam.org/)
[![Platform](https://img.shields.io/badge/Platform-Ubuntu%2020.04+-lightgrey.svg)]()
[![Language](https://img.shields.io/badge/Language-C%2B%2B-orange.svg)]()
[![Python](https://img.shields.io/badge/Python-3.6%2B-blue.svg)](https://www.python.org/)


</div>

## Table of Contents

- [SUE-Sim Overview](#sue-sim-overview)
- [System Architecture](#system-architecture)
  - [Core Components](#core-components)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
  - [Environment Requirements](#environment-requirements)
  - [Installation](#installation)
  - [Usage](#usage)
- [Configuration Parameters](#configuration-parameters)
---



## SUE-Sim Overview

SUE-Sim is an end-to-end, high-precision network simulation platform for the [Scale-Up Ethernet(SUE) framework](https://docs.broadcom.com/docs/scale-up-ethernet-framework).

Broadcom's SUE provides a low-latency, high-bandwidth interconnect framework for Ethernet-based XPU scale-up networks, supporting efficient interconnection of large-scale XPU clusters at rack-level and even multi-rack-level. It aims to address the increasingly prominent network bottleneck issues caused by the growing complexity of AI and machine learning workloads.

SUE-Sim serves two primary objectives:

- **Network Configuration and Performance Evaluation**: SUE-Sim provides a Scale-Up Ethernet simulation platform for GPU manufacturers, AI computing center operators, and other users. It supports constructing topologies of various scales, configuring parameters, and evaluating network performance under different workloads.

- **SUE Framework Specification Optimization**: The platform enables researchers to optimize SUE framework specifications through advanced algorithms and protocol validation. 

**Current Version**: SUE-Sim v1.0

**Implementation Scope**: This platform implements partial core functions of the Scale-Up Ethernet framework. Other features and details are being continuously improved.

## System Architecture
<p align="center">
  <img src="images/DisplayDiagram/Architecture.png" alt="XPU Internal Architecture Diagram" width="90%">
</p>


### Core Components

- **Traffic Generator**
  - Continuously generates transactions according to configured data rates
- **Load Balancer**
  - Balances traffic across SUE instances
- **Credit-Based Flow Controller (CBFC)**
  - Maintains credit usage for each VC to control flow
- **SUE Instance Packer**
  - Opportunistically packs transactions up to preconfigured size limits
  - Schedules destination queues to send transactions
- **Link-Level Retransmission Module (LLR)**
  - Retransmits corrupted packets between peer devices
- **Switch**
  - Implements a basic Layer 2 switch, supporting MAC address table lookup and frame forwarding


## Project Structure

```
SUE-Sim/
├── scratch/                        # Simulation scripts
│   └── SUE-Sim/                    # Main simulation script
│       └── SUE-Sim.cc              # Entry point for SUE simulation
│
├── src/                              # ns-3 source code
│   └── point-to-point-sue/           # SUE module
│       ├── model/                    # Core models
│       │   ├── simulation-config/                      # Simulation framework
│       │   │   ├── application-deployer.cc/.h          # Application deployment
│       │   │   ├── parameter-config.cc/.h              # Configuration parameters
│       │   │   ├── sue-utils.cc/.h                     # Utility functions
│       │   │   └── topology-builder.cc/.h              # Network topology builder
│       │   ├── point-to-point-sue-net-device.cc/.h     # Net device core
│       │   ├── point-to-point-sue-channel.cc/.h        # P2P channel
│       │   ├── sue-cbfc-header.cc/.h                   # CBFC header
│       │   ├── sue-header.cc/.h                        # SUE header
│       │   ├── sue-client.cc/.h                        # Multi-port client
│       │   ├── sue-server.cc/.h                        # Unpack server
│       │   ├── performance-logger.cc/.h                # Performance logger
│       │   ├── traffic-generator.cc/.h                 # Traffic generator
│       │   ├── load-balancer.cc/.h                     # Load balancer
│       │   └── xpu-delay-tag.cc/.h                     # XPU delay tag
│       ├── helper/                  # Helper classes
│       │   └── point-to-point-sue-helper.cc/.h
│       ├── examples/                # Example code
│       ├── test/                    # Unit tests
│       │   └── point-to-point-sue-test.cc
│       └── CMakeLists.txt           # Build configuration
│
├── performance-data/                  # Performance analysis results

├── log/                              # Simulation logs

└── README.md                         # Project documentation
```

### ns-3 Version Support

**🔄 SUE-Sim supports multiple ns-3 versions**

- **ns-3.36**  - `master` branch
- **ns-3.44**  - `ns3-44` branch

**Modular Design Features**: The point-to-point-sue module adopts a design architecture independent of the native point-to-point, with complete modular characteristics and plug-and-play capabilities. Through interface design and standardized implementation, this module can be quickly migrated to different versions of the ns-3 environment.


## Getting Started

### Environment Requirements

- **Operating System**: Linux (Ubuntu 20.04.6 LTS)
- **Compilers**:
  - **GCC**: 10.1.0+
  - **Clang**: 11.0.0+
  - **AppleClang**: 13.1.6+ (macOS)
- **Build Tools**:
  - **CMake**: 3.13.0+ (Required)

### Installation

#### Step 1: Install System Dependencies
First, install the essential build tools and libraries:

```bash
sudo apt update
sudo apt install build-essential cmake git software-properties-common
```

#### Step 2: Check and Upgrade GCC Version
Check your current GCC version:

```bash
gcc --version
```

**If your GCC version is 10.1.0 or higher**, proceed to [Step 3](#step-3-clone-and-configure-sue-sim).

**If your GCC version is below 10.1.0** (Ubuntu 20.04 default is 9.3.0), upgrade it:

```bash
# Add Ubuntu Toolchain PPA for newer GCC versions
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
# Install GCC 10 and G++ 10
sudo apt install gcc-10 g++-10
# Set GCC 10 as the default compiler
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10
```

#### Step 3: Clone and Configure SUE-Sim

> **Important Note**: [This version of ns3 does not allow direct execution as root user](https://groups.google.com/g/ns-3-users/c/xDtfcaUrCwg?pli=1), please run the following commands as a regular user.

```bash
# Clone the project
git clone https://github.com/kaima2022/SUE-Sim.git
cd SUE-Sim

# Configure ns-3 environment
./ns3 configure --enable-examples --enable-tests
```

#### Step 4: Build and Verify
```bash
# Build the project
./ns3 build

# Verify installation
./ns3 run "scratch/SUE-Sim/SUE-Sim --help"
```

### Usage
The following command demonstrates a 4-node XPU test scenario:

#### Topology Composition
- **XPU Nodes**: 4 XPUs, each XPU contains 16 ports
- **SUE Units**: Each SUE manages 4 ports

<p align="center">
  <img src="images/DisplayDiagram/Topology.png" alt="XPU Internal Architecture Diagram" width="80%">
</p>

```bash
# Run 4-node XPU test scenario
./ns3 run "scratch/SUE-Sim/SUE-Sim --nXpus=4 --portsPerXpu=16 --portsPerSue=4 --threadRate=3500000 --totalBytesToSend=50" > log/sue-sim.log 2>&1
```

NS3 logging is disabled by default, and collected data is stored in performance-data/data. For details, see [Performance Analysis Platform](#performance-analysis-platform).

> **Note**: Users are advised to configure parameters according to actual test scenarios.


## Configuration Parameters

SUE-Sim supports configuration parameters, covering network topology, traffic generation, link layer, CBFC flow control, and other aspects. For complete parameter descriptions, please refer to:

📋 **[Detailed Configuration Parameters Documentation](configuration.md)**

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request. For major changes, please open an issue first to discuss what you would like to change.

## Contact us

For questions, suggestions, or bug reports, please feel free to contact us:

- **Kai Ma**: chasermakai@gmail.com
- **GitHub Issues**: [Submit an issue](https://github.com/kaima2022/SUE-Sim/issues)

We look forward to hearing from you and appreciate your feedback!

---

## Acknowledgments

We sincerely thank the following projects and organizations:

- **[ns-3](https://www.nsnam.org/)** - Excellent network simulation framework, the foundation of this simulation platform
- **[Broadcom](https://docs.broadcom.com/docs/scale-up-ethernet-framework)** - Providing Scale-Up Ethernet framework specifications

## Citation

If you find this project useful for your research, please consider citing it in the following format:

```bibtex
@software{SUESimulator,
  month = {10},
  title = {{SUE-Sim: End-to-End Scale-Up Ethernet Simulation Platform}},
  url = {https://github.com/kaima2022/SUE-Sim},
  version = {1.0.0},
  year = {2025}
}
```

---

<div align="center">

If you find this project helpful, please consider giving it a ⭐ star! Your support is greatly appreciated.

Made by the SUE-Sim Project Team

</div>