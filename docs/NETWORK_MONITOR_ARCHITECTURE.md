# Network Monitor Architecture

## Purpose

The Network Monitor is responsible for collecting and monitoring endpoint network activity. It gathers active network connections from the operating system and provides structured network telemetry to higher-level components such as the Correlation Engine and Detection Engine.

The module focuses solely on network data collection and connection lifecycle tracking. Detection logic, behavioral analysis, and alert generation are handled by separate modules to maintain a clear separation of responsibilities.

## Responsibilities

The Network Monitor is responsible for:

- Monitoring active network connections.
- Detecting newly established and terminated connections.
- Collecting connection metadata such as:
  - Process ID (PID)
  - Local IP Address
  - Remote IP Address
  - Port
  - Protocol
  - Connection State
  - Timestamp
- Maintaining an up-to-date view of endpoint network activity.
- Publishing connection events to downstream components.

---

## Proposed Module Structure

```text
network/
│
├── interfaces/
│     INetworkMonitor.h
│
├── models/
│     Connection.h
│     RemoteEndpoint.h
│     ResolvedEndpoint.h
│     ProtocolPortProfile.h
│     ConnectionMetadata.h
│     ConnectionUploadStats.h
│     ConnectionDownloadStats.h
│     ConnectionTransferSnapshot.h
│     ConnectionBandwidthStats.h
│     ProcessBandwidthStats.h
│
├── collectors/
│     NetworkMonitor.h
│     NetworkMonitor.cpp
│
├── tracker/
│     ConnectionTracker.h
│     ConnectionTracker.cpp
│     UploadTracker.h
│     UploadTracker.cpp
│     DownloadTracker.h
│     DownloadTracker.cpp
│     TransferRateTracker.h
│     TransferRateTracker.cpp
│
├── attribution/
│     AttributedConnection.h
│     ConnectionAttributor.h
│
├── intelligence/
│     RemoteEndpointIdentifier.h
│     RemoteEndpointIdentifier.cpp
│     DomainResolver.h
│     DomainResolver.cpp
│     ProtocolPortAnalyzer.h
│     ProtocolPortAnalyzer.cpp
│
├── cache/
│     DnsCache.h
│     DnsCache.cpp
│
├── events/
│     EventDispatcher.h
│     EventDispatcher.cpp
│
└── utils/
      NetworkUtils.h
      NetworkUtils.cpp
```

### Module Description

| Module | Responsibility |
|---------|----------------|
| interfaces | Defines contracts for network monitoring components. |
| models | Stores data structures representing network connections and intelligence records. |
| collectors | Collects network connection information from the operating system. |
| tracker | Tracks connection lifecycle, cumulative upload/download bytes, and transfer rates. |
| tracker | Tracks connection lifecycle and cumulative upload/download bytes. |
| attribution | Associates connections with owning processes by PID. |
| intelligence | Extracts reusable connection intelligence (remote IPs, domains, ports). |
| cache | Caches reverse-DNS hostnames to avoid duplicate lookups. |
| events | Publishes connection events to other system components. |
| utils | Shared helpers such as IP, protocol, and port validation. |

---

## Core Components

### NetworkMonitor

The main component responsible for collecting active network connections from the operating system.

Responsibilities:

- Query Windows networking APIs.
- Collect active IPv4 and IPv6 TCP/UDP connections.
- Collect per-connection TCP ESTATS byte counters (`DataBytesOut` / `DataBytesIn`).
- Pass collected data to the Connection Tracker and transfer trackers.

---

### Connection

Represents a single network connection.

Example fields:

- Process ID (PID)
- Process Name
- Local Address
- Remote Address
- Port
- Protocol
- Connection State
- Timestamp

---

### RemoteEndpoint

Represents an identified remote peer for an active connection.

Example fields:

- Process ID
- Process Name
- Protocol
- Local IP / Local Port
- Remote IP / Remote Port
- Address Family (IPv4 / IPv6)
- Timestamp

Example output:

```text
Process:
python.exe

Remote IP:
104.18.32.45

Address Family:
IPv4
```

---

### RemoteEndpointIdentifier

Connection Intelligence helper that:

- Enumerates remote IP addresses from active connections
- Distinguishes local vs remote endpoints
- Detects address family (IPv4 / IPv6)
- Validates IP formatting before storing a `RemoteEndpoint`
- Skips listening sockets and malformed remotes gracefully

---

### DomainResolver / DnsCache

Connection Intelligence helpers that:

- Perform reverse DNS lookups for validated remote IPs (IPv4 / IPv6)
- Cache hostnames (with TTL) to avoid duplicate queries
- Support forced cache refresh
- Handle missing PTR records and DNS failures gracefully
- Produce `ResolvedEndpoint` records without affecting connection collection

Example output:

```text
Remote IP:
104.18.32.45

Resolved Domain:
api.example.com
```

---

### ProtocolPortAnalyzer

Connection Intelligence helper that:

- Detects TCP and UDP connections
- Tracks local and remote ports from the shared `Connection` model
- Preserves protocol metadata for reporting
- Supports IPv4 and IPv6 local endpoints
- Skips invalid rows gracefully (no remote port on listening sockets)

Example output:

```text
Protocol:
TCP

Local Port:
53142

Remote Port:
443
```

---

### ConnectionMetadata

Reusable Connection Intelligence record that:

- Stores remote IP and resolved domain together
- Stores transport protocol plus local and remote ports
- Can be built from a `Connection` snapshot via `fromConnection`
- Provides formatted output for logging and inspection
- Contains metadata only (no OS or DNS logic)

Example output:

```text
Remote IP:
104.18.32.45

Domain:
api.example.com

Protocol:
TCP

Remote Port:
443
```

---

### NetworkUtils

Shared IP helpers used by collectors and intelligence modules:

- IPv4 / IPv6 format validation
- Address family detection
- Loopback and unspecified address checks
- Protocol and port validation helpers

---

### ConnectionTracker

Maintains the current state of active network connections.

Responsibilities:

- Detect newly established connections.
- Detect closed connections.
- Prevent duplicate connection records.

---

### UploadTracker

Maintains cumulative outbound upload statistics for active connections.

Responsibilities:

- Compare OS-reported bytes-sent counters across snapshots.
- Accumulate upload deltas per connection identity.
- Associate upload totals with owning PID and process name.
- Preserve final statistics when connections close.
- Expose reusable `ConnectionUploadStats` records for detection modules.

Example output:

```text
Process:
python.exe

Remote:
104.18.32.45:443

Uploaded:
2.4 MB
```

---

### DownloadTracker

Maintains cumulative inbound download statistics for active connections.

Responsibilities:

- Compare OS-reported bytes-received counters across snapshots.
- Accumulate download deltas per connection identity.
- Associate download totals with owning PID and process name.
- Preserve final statistics when connections close.
- Expose reusable `ConnectionDownloadStats` records for detection modules.

Example output:

```text
Process:
chrome.exe

Remote:
142.250.190.78:443

Downloaded:
18.7 MB
```

---

### TransferRateTracker

Calculates short-window upload/download rates and total bandwidth usage from successive OS byte-counter snapshots.

Responsibilities:

- Derive per-connection upload and download rates (`delta_bytes / elapsed_seconds`).
- Accumulate session totals and expose total transfer (upload + download).
- Aggregate active connections into per-process bandwidth statistics.
- Preserve final totals when connections close; clear rates for inactive endpoints.
- Handle OS counter resets without producing negative or infinite rates.

Example output:

```text
Process:
python.exe

Upload Rate:
1.8 MB/s

Download Rate:
320.0 KB/s

Total Transfer:
42.6 MB
```

---

### EventDispatcher

Publishes connection events so downstream modules can consume them without directly depending on the Network Monitor.

Example consumers include:

- Correlation Engine
- Detection Engine
- Logging components

---

## High-Level Data Flow

```text
Windows Networking APIs (IPv4 + IPv6 + TCP ESTATS)
          │
          ▼
    Network Monitor
          │
          ├──────────────────┬──────────────────────────────┐
          ▼                  ▼                              ▼
   Connection Tracker  Transfer Snapshots        Remote Endpoint Identifier
          │                  │                              │
          ▼                  ├──────────────┬───────────────┤
    Event Dispatcher         ▼              ▼               ▼
          │            UploadTracker  DownloadTracker  TransferRateTracker
          │                  │              │               │
          ▼                  ├──────────────┐               ▼
    Event Dispatcher         ▼              ▼        RemoteEndpoint records
          │            UploadTracker  DownloadTracker       │
          ├──────────────┬───┴──────────────┴───────────────┤
          ▼              ▼               ▼                  ▼
  Correlation Engine  Domain Resolver  Protocol Port Analyzer
          │         (+ DnsCache)              │
          ▼              ▼                    ▼
   Detection Engine  ResolvedEndpoint  ProtocolPortProfile
                                               │
                                               ▼
                                      ConnectionMetadata
```

### Flow Description

1. The operating system exposes active IPv4/IPv6 network connection information.
2. The Network Monitor collects connection data (local and remote endpoints) and TCP ESTATS byte counters.
3. The Connection Tracker maintains the current connection state.
4. UploadTracker / DownloadTracker accumulate outbound and inbound transfer totals from successive snapshots.
5. TransferRateTracker derives short-window rates and per-process bandwidth aggregates from the same snapshots.
6. The Remote Endpoint Identifier validates remote IPs and records address family.
7. The Domain Resolver performs reverse DNS (with caching) for human-readable hostnames.
8. The Protocol Port Analyzer extracts transport protocol and port metadata.
9. `ConnectionMetadata` combines remote IP, domain, protocol, and ports into one reusable record.
10. Connection events are published through the Event Dispatcher.
11. Higher-level modules consume these events for correlation, behavioral analysis, and threat detection.
5. The Remote Endpoint Identifier validates remote IPs and records address family.
6. The Domain Resolver performs reverse DNS (with caching) for human-readable hostnames.
7. The Protocol Port Analyzer extracts transport protocol and port metadata.
8. `ConnectionMetadata` combines remote IP, domain, protocol, and ports into one reusable record.
9. Connection events are published through the Event Dispatcher.
10. Higher-level modules consume these events for correlation, behavioral analysis, and threat detection.

---

## Architecture Decisions

### 1. Separation of Responsibilities

The Network Monitor is responsible only for collecting network telemetry. Threat detection, behavioral analysis, and alert generation are handled by separate modules.

**Reason:**
Keeping responsibilities separate makes the system easier to maintain, test, and extend.

---

### 2. Event-Driven Communication

The Network Monitor publishes connection events instead of directly invoking downstream components.

**Reason:**
This reduces coupling between modules and allows multiple components (Correlation Engine, Detection Engine, Logging, etc.) to consume the same events independently.

---

### 3. Modular Design

The module is divided into logical components such as collectors, trackers, models, and events.

**Reason:**
This improves code organization, readability, and future scalability.

---

### 4. Platform Abstraction

Platform-specific network collection logic should remain isolated inside the Network Monitor implementation.

**Reason:**
Higher-level components should remain independent of operating system APIs, making future platform support easier.

---

## Future Enhancements

Future iterations of the Network Monitor may include:

- Real-time event streaming.
- Connection history caching.
- Download traffic statistics.
- Connection filtering.
- Performance optimizations for high connection volumes.