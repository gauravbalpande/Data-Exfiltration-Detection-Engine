# Port and Protocol Analysis

## Purpose

Collect protocol and port information for active network connections as part of Connection Intelligence (Milestone M3).

## Responsibilities

- Detect TCP and UDP connections
- Track local and remote ports
- Preserve protocol metadata from the shared `Connection` model
- Support IPv4 and IPv6 connections
- Skip invalid rows (missing local port, unknown protocol) gracefully
- Leave existing connection collection and domain resolution unaffected

## Data Model

```text
ProtocolPortProfile
├── process_id
├── process_name
├── protocol
├── local_port
├── remote_port
├── connection_state
├── address_family
├── has_remote_port
└── timestamp
```

The underlying `Connection` model continues to store raw protocol/port fields from the collector.

## Components

| Component | Location | Role |
|-----------|----------|------|
| `ProtocolPortProfile` | `src/network/models/` | Structured protocol/port record |
| `ProtocolPortAnalyzer` | `src/network/intelligence/` | Extract and validate metadata from connections |
| `NetworkUtils` | `src/network/utils/` | Shared protocol/port helpers |
| `Connection` | `src/network/models/` | Source model populated by `NetworkMonitor` |

## Usage

```cpp
network::NetworkMonitor monitor;
network::ProtocolPortAnalyzer analyzer;

auto connections = monitor.getActiveConnections();
auto profiles = analyzer.analyze(connections);

for (const auto& profile : profiles)
{
    std::cout << profile.toString() << "\n";
}
```

With process attribution:

```cpp
auto attributed = attributor.attribute(connections);
auto profiles = analyzer.analyze(attributed);
```

## Expected Output

```text
Protocol:
TCP

Local Port:
53142

Remote Port:
443
```

Listening / UDP bind-only rows omit the remote port section when unavailable.

## Edge Cases

| Case | Behavior |
|------|----------|
| TCP with remote port | Protocol, local, and remote ports recorded |
| UDP bind (no remote) | Local port recorded; `has_remote_port = false` |
| Listening socket | Local port recorded; remote port omitted |
| IPv4 / IPv6 local address | Address family inferred from local IP |
| Invalid local port (0) | Row skipped |
| Unknown protocol | Row skipped |

## Tests

```bash
c++ -std=c++17 -o protocol_port_tests tests/network/ProtocolPortAnalyzerTests.cpp
./protocol_port_tests
```
