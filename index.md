---
layout: default
title: SUE-Sim
description: SUE-Sim is an end-to-end Scale-Up Ethernet network simulation platform for AI and HPC workloads.
---

# SUE-Sim

End-to-End Scale-Up Ethernet Simulation Platform based on ns-3.

[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE)
[![ns-3](https://img.shields.io/badge/ns--3-3.44%2F3.36-green.svg)](VERSION)
[![Platform](https://img.shields.io/badge/Platform-Ubuntu%2020.04+-lightgrey.svg)](README.md#environment-requirements)
[![Language](https://img.shields.io/badge/Language-C%2B%2B-orange.svg)](https://isocpp.org/)

[GitHub Repository](https://github.com/kaima2022/SUE-Sim) | [README](README.md)

SUE-Sim is a Scale-Up Ethernet simulator for AI/HPC data center network research, protocol validation, and performance benchmarking.

## Highlights

- End-to-end simulation for Scale-Up Ethernet protocol workflows.
- Built on ns-3 with reproducible experiment workflows.
- Supports topology construction, parameter tuning, and workload evaluation.
- Targets AI and HPC network scenarios.

## Architecture

![SUE-Sim Architecture](images/DisplayDiagram/Architecture.png)

## Quick Start

```bash
git clone https://github.com/kaima2022/SUE-Sim.git
cd SUE-Sim
./ns3 configure
./ns3 build
```

For full setup and usage details, see [Getting Started in README](README.md#getting-started).

## Documentation

- [Project README](README.md)
- [Configuration Parameters](configuration.md)
- [Examples](examples/README.md)
