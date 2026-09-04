# Transfer Rate Calculation

## Purpose

Calculate short-window upload and download transfer rates from collected byte-counter measurements, and maintain per-connection and per-process bandwidth statistics as part of Data Transfer Analysis (Milestone M4).

## Responsibilities

- Calculate upload transfer rate (bytes/second)
- Calculate download transfer rate (bytes/second)
- Calculate total bandwidth usage (upload + download bytes)
- Maintain per-process bandwidth statistics
- Maintain per-connection bandwidth statistics
- Derive short-window rates from successive snapshots
- Handle OS counter resets without producing invalid rates
- Handle closed connections safely
- Preserve first-seen and last-updated measurement timestamps
- Remain reusable for future detection modules

## Data Model

```text
ConnectionBandwidthStats
├── process_id
├── process_name
├── remote_address
├── remote_port
├── protocol
├── uploaded_bytes
├── downloaded_bytes
├── upload_rate_bps
├── download_rate_bps
├── first_observed
├── last_updated
└── is_active

ProcessBandwidthStats
├── process_id
├── process_name
├── uploaded_bytes
├── downloaded_bytes
├── upload_rate_bps
├── download_rate_bps
├── first_observed
├── last_updated
└── active_connection_count
```

Input snapshots pair a `Connection` with OS-reported cumulative byte counters:

```text
ConnectionTransferSnapshot
├── connection
├── bytes_sent
├── bytes_received
└── process_name
```

## Components

| Component | Location | Role |
|-----------|----------|------|
| `ConnectionBandwidthStats` | `src/network/models/` | Per-connection rate + total record |
| `ProcessBandwidthStats` | `src/network/models/` | Per-process aggregated bandwidth record |
| `ConnectionTransferSnapshot` | `src/network/models/` | Shared OS byte-counter snapshot |
| `TransferRateTracker` | `src/network/tracker/` | Computes short-window rates and aggregates |
| `NetworkUtils::formatRate` | `src/network/utils/` | Human-readable rate formatting |
| `NetworkUtils::formatBytes` | `src/network/utils/` | Human-readable byte formatting |
| `NetworkMonitor::getConnectionTransferSnapshots` | `src/network/collectors/` | Reads TCP ESTATS counters |

## Runtime Integration

```text
NetworkMonitor::getConnectionTransferSnapshots()
        │  (TCP ESTATS DataBytesIn / DataBytesOut)
        ▼
ConnectionTransferSnapshot[]
        │
        ├──────────────┬──────────────────┐
        ▼              ▼                  ▼
  UploadTracker   DownloadTracker   TransferRateTracker
        │              │                  │
        ▼              ▼                  ├──────────────┐
ConnectionUploadStats  ConnectionDownloadStats          ▼
                                         ConnectionBandwidthStats
                                                        │
                                                        ▼
                                              ProcessBandwidthStats
```

`TransferRateTracker` consumes both `bytesSent` and `bytesReceived`. Short-window rates are `delta_bytes / elapsed_seconds` between successive observations of the same connection identity.

## Usage

```cpp
network::NetworkMonitor monitor;
network::TransferRateTracker tracker;

auto snapshots = monitor.getConnectionTransferSnapshots();
// Optionally fill snapshot.processName via process attribution.

auto connectionStats = tracker.update(snapshots);

for (const auto& process : tracker.getProcessBandwidthStats())
{
    std::cout << process.toString() << "\n";
}
```

When a connection disappears from later snapshots, its final cumulative totals are preserved in `tracker.getClosedConnectionStats()` with rates cleared to zero. Process-level aggregates remain available via `getProcessBandwidthStats()` while the process still has active connections; fully idle processes move to `getClosedProcessStats()`.

## Expected Output

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

## Edge Cases

- **First snapshot:** Establishes the OS byte baseline without counting existing traffic as new activity; rates remain `0`.
- **Short window:** Rates use only the bytes transferred since the previous snapshot for that connection.
- **Zero / identical timestamps:** Elapsed time of zero yields a `0` rate (avoids divide-by-zero).
- **Counter reset:** If an OS counter decreases, only the new counter value is treated as fresh progress; rates stay non-negative.
- **Duplicate rows:** Duplicate connection rows within one snapshot are ignored.
- **Closed connections:** Final totals move to closed maps; short-window rates are cleared.
- **UDP / no ESTATS:** Endpoints without per-connection counters report zero byte values so lifecycle tracking still works.

## Build and Test

```bash
c++ -std=c++17 -o transfer_rate_tracker_tests tests/network/TransferRateTrackerTests.cpp
./transfer_rate_tracker_tests
```

## Acceptance Criteria

- Upload rate is calculated correctly from successive measurements.
- Download rate is calculated correctly from successive measurements.
- Total transfer values (upload + download) are accurate.
- Per-process statistics are available.
- Per-connection statistics are available.
- Counter resets do not produce invalid (negative / infinite) rates.
- Closed connections are handled safely.
- Measurement timestamps are preserved.
- Unit tests cover rates, aggregation, closure, and counter-reset behavior.
- Documentation is updated.
