# Domain Resolution

## Purpose

Resolve validated remote IP addresses into human-readable domain names using reverse DNS, with caching, for Connection Intelligence (Milestone M3).

## Responsibilities

- Perform reverse DNS lookups for IPv4 and IPv6 remotes
- Cache resolved hostnames (TTL-based) to avoid duplicate queries
- Support forced cache refresh
- Handle missing PTR records, timeouts, and DNS failures gracefully
- Return an empty hostname when no domain is available
- Leave existing connection collection unaffected

## Data Model

```text
ResolvedEndpoint
├── process_id
├── process_name
├── remote_ip
├── resolved_domain
├── address_family
├── resolution_status
├── cache_hit
└── timestamp
```

`resolution_status` values: `Resolved`, `NotFound`, `Failed`, `InvalidIp`, `Cached`.

## Components

| Component | Location | Role |
|-----------|----------|------|
| `DnsCache` | `src/network/cache/` | TTL hostname cache with invalidate / purge |
| `DomainResolver` | `src/network/intelligence/` | Reverse DNS + cache orchestration |
| `ResolvedEndpoint` | `src/network/models/` | Output record for Connection Intelligence |
| `RemoteEndpoint` | `src/network/models/` | Input from remote IP identification |

## Usage

```cpp
network::DnsCache cache(std::chrono::seconds(300));
network::DomainResolver resolver(cache);

auto remotes = identifier.identify(connections);
auto resolved = resolver.resolveAll(remotes);

for (const auto& endpoint : resolved)
{
    std::cout << endpoint.toString() << "\n";
}

// Force a fresh lookup for one IP:
resolver.refresh("104.18.32.45");
auto updated = resolver.resolveIp("104.18.32.45", network::AddressFamily::IPv4, true);
```

## Expected Output

```text
Remote IP:
104.18.32.45

Resolved Domain:
api.example.com
```

## Edge Cases

| Case | Behavior |
|------|----------|
| IPv4 with PTR | `RESOLVED`, hostname stored + cached |
| IPv6 with PTR | `RESOLVED`, hostname stored + cached |
| No PTR record | Empty hostname, `NOT_FOUND`, negative cache |
| DNS failure / timeout | Empty hostname, `FAILED`, negative cache |
| Invalid IP | `INVALID_IP`, no lookup performed |
| Cache hit | `CACHED`, `cache_hit = true`, no live DNS |
| Cache expiration | Entry ignored; next resolve hits DNS |
| Force refresh | Cache invalidated; live lookup performed |

## Tests

```bash
c++ -std=c++17 -o domain_resolver_tests tests/network/DomainResolverTests.cpp
./domain_resolver_tests
```

Tests inject a mock lookup function so no live DNS is required.
