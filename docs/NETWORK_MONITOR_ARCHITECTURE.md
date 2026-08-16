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
│
├── collectors/
│     NetworkMonitor.h
│     NetworkMonitor.cpp
│
├── tracker/
│     ConnectionTracker.h
│     ConnectionTracker.cpp
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
| tracker | Tracks connection lifecycle (new, active, closed). |
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
- Pass collected data to the Connection Tracker.

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

### EventDispatcher

Publishes connection events so downstream modules can consume them without directly depending on the Network Monitor.

Example consumers include:

- Correlation Engine
- Detection Engine
- Logging components

---

## High-Level Data Flow

```text
Windows Networking APIs (IPv4 + IPv6)
          │
          ▼
    Network Monitor
          │
          ├──────────────────────────────┐
          ▼                              ▼
   Connection Tracker          Remote Endpoint Identifier
          │                              │
          ▼                              ▼
    Event Dispatcher              RemoteEndpoint records
          │                              │
          ├──────────────┬───────────────┤
          ▼              ▼               ▼
  Correlation Engine  Domain Resolver  Protocol Port Analyzer
          │              │               │
          ▼              ▼               ▼
   Detection Engine  ResolvedEndpoint  ProtocolPortProfile
                           (+ DnsCache)
```

### Flow Description

1. The operating system exposes active IPv4/IPv6 network connection information.
2. The Network Monitor collects connection data (local and remote endpoints).
3. The Connection Tracker maintains the current connection state.
4. The Remote Endpoint Identifier validates remote IPs and records address family.
5. The Domain Resolver performs reverse DNS (with caching) for human-readable hostnames.
6. The Protocol Port Analyzer extracts transport protocol and port metadata.
7. Connection events are published through the Event Dispatcher.
8. Higher-level modules consume these events for correlation, behavioral analysis, and threat detection.

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
- Upload and download traffic statistics.
- Connection filtering.
- Performance optimizations for high connection volumes.