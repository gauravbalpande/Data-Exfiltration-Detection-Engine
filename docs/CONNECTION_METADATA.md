# Connection Metadata

## Purpose

Provide a reusable metadata model for network endpoint information as part of Connection Intelligence (Milestone M3).

## Responsibilities

- Store remote IP address
- Store resolved domain name
- Store transport protocol information
- Store local and remote ports
- Provide formatted output for logging and inspection
- Remain free of OS networking and DNS logic

## Data Model

```text
ConnectionMetadata
├── process_id
├── process_name
├── remote_ip
├── domain
├── protocol
├── local_port
├── remote_port
├── address_family
├── has_remote_port
└── timestamp
```

## Components

| Component | Location | Role |
|-----------|----------|------|
| `ConnectionMetadata` | `src/network/models/` | Portable endpoint metadata record |
| `Connection` | `src/network/models/` | Source snapshot for `fromConnection` |
| `NetworkUtils` | `src/network/utils/` | Protocol labels, port checks, address family |

## Usage

```cpp
network::Connection connection(
    4120,
    "192.168.1.10",
    53142,
    "104.18.32.45",
    443,
    network::ProtocolType::TCP,
    network::ConnectionState::ESTABLISHED,
    std::chrono::system_clock::now());

auto metadata = network::ConnectionMetadata::fromConnection(
    connection, "api.example.com", "python.exe");

std::cout << metadata.toString() << "\n";
```

Or construct directly:

```cpp
network::ConnectionMetadata metadata(
    4120,
    "python.exe",
    "104.18.32.45",
    "api.example.com",
    network::ProtocolType::TCP,
    53142,
    443,
    network::AddressFamily::IPv4,
    true,
    std::chrono::system_clock::now());
```

## Expected Output

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

Listening / bind-only rows omit the remote port section when unavailable.

## Edge Cases

| Case | Behavior |
|------|----------|
| TCP with domain and remote port | All fields stored; full formatted output |
| Missing domain | `domain` empty; Domain section still rendered empty |
| UDP / TCP | Protocol stored via `ProtocolType` |
| Listening socket (remote port 0) | `has_remote_port = false`; Remote Port omitted |
| IPv4 / IPv6 remote | Address family inferred in `fromConnection` |

## Tests

```bash
c++ -std=c++17 -o connection_metadata_tests tests/network/ConnectionMetadataTests.cpp
./connection_metadata_tests
```
