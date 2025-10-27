# SUE-Sim Configuration Parameters Documentation

## Overview

SUE-Sim supports rich configuration parameters, covering network topology, traffic generation, link layer, CBFC flow control, and other aspects. Users can adjust these parameters according to specific needs to conduct simulation experiments in different scenarios.

## Network Topology Structure

SUE-Sim adopts an XPU-Switch topology structure based on the SUE (Scale-Up Ethernet) framework specification:

### Topology Composition
- **XPU Nodes**: Compute Processing Units, each XPU contains multiple ports
- **SUE Units**: Each SUE manages 1/2/4 ports


## Parameter Categories

### Time Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--simulationTime` | 3.00 | Total simulation time (seconds) |
| `--serverStart` | 1.0 | Server start time (seconds) |
| `--clientStart` | 2.0 | Client start time (seconds) |
| `--clientStopOffset` | 0.1 | Client stop time offset (seconds) |
| `--serverStopOffset` | 0.01 | Server stop time offset (seconds) |
| `--threadStartInterval` | 0.1 | Thread start interval (seconds) |

### Network Topology Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--nXpus` | 4 | Number of XPU nodes |
| `--portsPerXpu` | 8 | Number of ports per XPU |
| `--portsPerSue` | 2 | Number of ports managed per SUE (1/2/4) |

### Traffic Generation Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--transactionSize` | 256 | Transaction size (bytes) |
| `--maxBurstSize` | 2048 | Maximum burst size (bytes) |
| `--Mtu` | 2500 | Maximum Transmission Unit (bytes) |
| `--vcNum` | 4 | Number of virtual channels |
| `--threadRate` | 3500000 | Thread data rate (Mbps) |
| `--totalBytesToSend` | 50 | Total data to send (MB) |

### Link Layer Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--numVcs` | 4 | Number of link layer virtual channels |
| `--LinkDataRate` | "200Gbps" | Link data rate |
| `--ProcessingRate` | "200Gbps" | Processing rate |
| `--LinkDelay` | "10ns" | Link propagation delay |
| `--errorRate` | 0.00 | Error rate |
| `--processingDelay` | "10ns" | Packet processing delay |

### CBFC Credit Flow Control Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--EnableLinkCBFC` | true | Enable link layer CBFC flow control |
| `--LinkCredits` | 85 | Initial CBFC credits for link layer |
| `--CreditBatchSize` | 1 | Credit accumulation threshold |

### Queue Buffer Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--vcQueueMaxMB` | 0.3 | VC queue maximum capacity (MB) |
| `--processingQueueMaxMB` | 0.3 | Processing queue maximum capacity (MB) |
| `--destQueueMaxMB` | 0.03 | Destination queue maximum capacity (MB) |

### Load Balancing Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--loadBalanceAlgorithm` | 3 | Load balancing algorithm (0-5) |
| `--hashSeed` | 12345 | Hash seed |
| `--prime1` | 7919 | First prime number for hash algorithm |
| `--prime2` | 9973 | Second prime number for enhanced hash |
| `--useVcInHash` | true | Include VC ID in hash calculation |
| `--enableBitOperations` | true | Enable bit mixing operations |


### Delay Parameters

**Application Layer Delay Parameters**
| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--SchedulingInterval` | "5ns" | Transmitter scheduling polling interval |
| `--PackingDelayPerPacket` | "3ns" | Packet packing processing time |
| `--destQueueSchedulingDelay` | "5ns" | Destination queue scheduling delay |
| `--transactionClassificationDelay` | "0ns" | Transaction classification delay |
| `--packetCombinationDelay` | "12ns" | Packet combination delay |
| `--ackProcessingDelay` | "15ns" | ACK processing delay |

**Link Layer Delay Parameters**
| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--vcSchedulingDelay` | "8ns" | VC queue scheduling delay |
| `--DataAddHeadDelay` | "5ns" | Data packet header addition delay |
| `--creditGenerateDelay` | "10ns" | Credit packet generation delay |
| `--CreUpdateAddHeadDelay` | "3ns" | Credit packet header addition delay |
| `--creditReturnProcessingDelay` | "8ns" | Credit return processing delay |
| `--batchCreditAggregationDelay` | "5ns" | Batch credit aggregation delay |
| `--switchForwardDelay` | "130ns" | Switch internal forwarding delay |

### Statistics Monitoring Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--StatLoggingEnabled` | true | Enable link layer statistics collection |
| `--ClientStatInterval` | "10us" | Client statistics interval |
| `--LinkStatInterval` | "10us" | Link statistics interval |

### LLR (Link-Level Retransmission) Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--EnableLLR` | false | Enable Link Layer Reliability |
| `--LlrTimeout` | "10000ns" | LLR timeout value |
| `--LlrWindowSize` | 10 | LLR window size |
| `--AckAddHeaderDelay` | "10ns" | ACK/NACK header addition delay |
| `--AckProcessDelay` | "10ns" | ACK/NACK processing delay |

### Capacity Reservation Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--additionalHeaderSize` | 52 | Capacity reservation additional header size (bytes) |

## Usage Examples

### Basic Run
```bash
./ns3 run "scratch/SUE-Sim/SUE-Sim"
```

### Custom Parameters Run
```bash
./ns3 run "scratch/SUE-Sim/SUE-Sim --nXpus=4 --portsPerXpu=16 --portsPerSue=4 --threadRate=3500000 --totalBytesToSend=50"
```

---
