# Remote Endpoint Identification

## Purpose

Identify remote IP addresses associated with active network connections and expose them as structured `RemoteEndpoint` records for Connection Intelligence (Milestone M3).

## Responsibilities

- Enumerate remote IP addresses from active TCP/UDP connections
- Support IPv4 and IPv6
- Distinguish local endpoints from remote peers
- Validate IP address formatting before storage
- Skip or handle connections without a usable remote endpoint

## Data Model

```text
RemoteEndpoint
├── process_id
├── process_name
├── protocol
├── local_ip
├── local_port
├── remote_ip
├── remote_port
├── address_family
└── timestamp
```

## Components

| Component | Location | Role |
|-----------|----------|------|
| `NetworkUtils` | `src/network/utils/` | Validate IPs, detect address family, loopback/unspecified checks |
| `RemoteEndpoint` | `src/network/models/` | Portable remote peer record |
| `RemoteEndpointIdentifier` | `src/network/intelligence/` | Extract validated remotes from connections |
| `NetworkMonitor` | `src/network/collectors/` | Enumerate IPv4 + IPv6 TCP/UDP tables |

## Usage

```cpp
network::NetworkMonitor monitor;
network::RemoteEndpointIdentifier identifier;

auto connections = monitor.getActiveConnections();
auto remotes = identifier.identify(connections);

for (const auto& endpoint : remotes)
{
    std::cout << endpoint.toString() << "\n";
}
```

With process attribution:

```cpp
auto attributed = attributor.attribute(connections);
auto remotes = identifier.identify(attributed);
```

## Expected Output

```text
Process:
python.exe

Remote IP:
104.18.32.45

Address Family:
IPv4
```

## Edge Cases

| Case | Behavior |
|------|----------|
| IPv4 remote | Stored with `AddressFamily::IPv4` |
| IPv6 remote | Stored with `AddressFamily::IPv6` |
| Loopback (`127.0.0.1` / `::1`) | Accepted as a valid remote peer |
| Listening socket / empty remote | Skipped |
| Unspecified (`0.0.0.0` / `::`) | Skipped |
| Malformed address | Skipped |
| One address-family table fails | Other family still enumerated |

## Tests

Run the standalone unit tests (no Windows APIs required):

```bash
c++ -std=c++17 -o remote_endpoint_tests tests/network/RemoteEndpointTests.cpp
./remote_endpoint_tests
```
