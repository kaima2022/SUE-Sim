# SUE-Sim Configuration Parameters Documentation

## Overview

SUE-Sim supports rich configuration parameters, covering network topology, traffic generation, link layer, CBFC flow control, and other aspects. Users can adjust these parameters according to specific needs to conduct simulation experiments in different scenarios.

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
| `--threadRate` | 3500000 | Traffic generation rate per thread (Mbps) |
| `--totalBytesToSend` | 50 | Total bytes to send per XPU (MB) |
| `--enableTraceMode` | false | Enable trace-based traffic generation |
| `--traceFilePath` | "" | Path to trace file for trace-based generation |
| `--enableFineGrainedMode` | false | Enable configuration-based traffic generation |
| `--fineGrainedConfigFile` | "" | Path to configuration file for fine-grained traffic generation |

### Link Layer Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--numVcs` | 4 | Number of link layer virtual channels |
| `--LinkDataRate` | "200Gbps" | Link data rate |
| `--ProcessingRate` | "200Gbps" | Processing rate |
| `--LinkDelay` | "10ns" | Link propagation delay |
| `--errorRate` | 0.00 | Error rate |
| `--ErrorRateStopAfter` | "" | Disable receiver error model after a specified duration |
| `--ErrorModelApplyToControlPackets` | true | Apply error model to CBFC and LLR control packets |
| `--ErrorModelApplyToSyncPackets` | true | Apply error model to CBFC_SYNC credit synchronization packets |
| `--processingDelay` | "10ns" | Packet processing delay |

### CBFC Credit Flow Control Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--EnableLinkCBFC` | true | Enable link layer CBFC flow control |
| `--LinkCredits` | 0 | Initial link credits. `0` means auto-calibrate from processing queue size and `BytesPerCredit` |
| `--CreditBatchSize` | 1 | Credit accumulation threshold |
| `--SwitchCredits` | 0 | Switch egress credit budget. `0` means auto-calibrate from VC queue size and VC count |
| `--HeaderSize` | 52 | Header size 
| `--BaseCredit` | 1 | Base credit value for minimum packet |
| `--BytesPerCredit` | 32 | Bytes per credit for linear mapping (bytes) |
| `--SwitchEgressOverflowPolicy` | 0 | Switch egress overflow policy: `0=retry`, `1=drop` |
| `--EnableCreditSync` | false | Enable periodic credit synchronization |
| `--CreditSyncInterval` | "0ns" | Periodic credit synchronization interval |
| `--LinkCreditMode` | 0 | Link credit allocation mode: `0=SHARED`, `1=EXCLUSIVE` |

### Queue Buffer Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--vcQueueMaxKB` | 30.0 | VC queue maximum capacity (KB) |
| `--processingQueueMaxKB` | 30.0 | Processing queue maximum capacity (KB) |
| `--destQueueMaxKB` | 30.0 | Destination queue maximum capacity (KB) |

### Load Balancing Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--loadBalanceAlgorithm` | 3 | Load balancing algorithm (0-5) |
| `--hashSeed` | 12345 | Hash seed |
| `--prime1` | 7919 | First prime number for hash algorithm |
| `--prime2` | 9973 | Second prime number for enhanced hash |
| `--useVcInHash` | true | Include VC ID in hash calculation |
| `--enableBitOperations` | true | Enable bit mixing operations |
| `--enableAlternativePath` | false | Enable alternative SUE path search when target is full |


### Delay Parameters

**Application Layer Delay Parameters**
| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--SchedulingInterval` | "100ns" | Transmitter scheduling polling interval |
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
| `--processingQueueScheduleDelay` | "5ns" | Processing queue scheduling delay |

### Logging Configuration Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--logLevel` | "LOG_LEVEL_INFO" | Log level for all SUE simulation components |
| `--enableAllComponents` | true | Enable logging for all SUE simulation components |

**Supported Log Levels:**
- `LOG_LEVEL_DEBUG` - Most detailed debugging information (includes FUNCTION level logging)
- `LOG_LEVEL_INFO` - General information messages (default)
- `LOG_LEVEL_WARN` - Warning messages
- `LOG_LEVEL_ERROR` - Error messages only
- `LOG_LEVEL_FUNCTION` - Function entry/exit tracing
- `LOG_LEVEL_LOGIC` - Logic flow information
- `LOG_LEVEL_ALL` - All log messages

### Statistics Monitoring Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--StatLoggingEnabled` | true | Enable link layer statistics collection |
| `--ClientStatInterval` | "10us" | Client statistics interval |

### LLR (Link-Level Retransmission) Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--EnableLLR` | false | Enable Link Layer Reliability |
| `--LlrProtectCbfcUpdates` | true | Protect CBFC update packets with LLR sequencing and retransmission |
| `--LlrTimeout` | "10000ns" | LLR timeout value |
| `--LlrWindowSize` | 10 | LLR window size |
| `--AckAddHeaderDelay` | "10ns" | ACK/NACK header addition delay |
| `--AckProcessDelay` | "10ns" | ACK/NACK processing delay |

### Migration Update Notes

The main repository now includes the migration-tested feature set that was previously validated in the SUE-Sim test repository. The most important operational implications are:

- `portsPerXpu=1` is required for real `N -> 1` incast and backpressure tests because the migrated routing path preserves port identity.
- `LinkCreditMode` allows direct comparison between shared-pool and per-VC exclusive credit allocation.
- `EnableCreditSync` is intended for targeted experiments and should normally remain disabled unless credit-drift behavior is being studied.
- When `SwitchEgressOverflowPolicy=1`, `EnableLLR=true` is strongly recommended to avoid silent loss-induced deadlock scenarios.
- `LinkCredits=0` and `SwitchCredits=0` are the recommended defaults because they track queue-capacity-based automatic calibration.

### Capacity Reservation Parameters

| Parameter | Default Value | Description |
|-----------|---------------|-------------|
| `--additionalHeaderSize` | 44 | Capacity reservation additional header size (bytes) |

---
