# Download Tracking

## Purpose

Track inbound data received through active network connections and attribute downloaded bytes to the owning process as part of Data Transfer Analysis (Milestone M4).

## Responsibilities

- Track bytes received by active network connections
- Associate downloaded bytes with the owning PID and process name
- Maintain cumulative download counters across snapshots
- Track download data independently for each connection
- Handle connection creation and closure safely
- Preserve first-seen and last-updated timestamps
- Remain reusable for future analysis modules

## Data Model

```text
ConnectionDownloadStats
├── process_id
├── process_name
├── remote_address
├── remote_port
├── protocol
├── downloaded_bytes
├── first_observed
├── last_updated
└── is_active
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
| `ConnectionDownloadStats` | `src/network/models/` | Portable download metrics record |
| `ConnectionTransferSnapshot` | `src/network/models/` | Shared OS byte-counter snapshot |
| `DownloadTracker` | `src/network/tracker/` | Accumulates inbound bytes across snapshots |
| `NetworkMonitor::getConnectionTransferSnapshots` | `src/network/collectors/` | Reads TCP ESTATS `DataBytesIn` / `DataBytesOut` |
| `NetworkUtils::formatBytes` | `src/network/utils/` | Human-readable byte formatting |
| `Connection` | `src/network/models/` | Connection identity and timestamps |

## Runtime Integration

```text
NetworkMonitor::getConnectionTransferSnapshots()
        │  (TCP ESTATS DataBytesIn / DataBytesOut)
        ▼
ConnectionTransferSnapshot[]
        │
        ├──────────────┐
        ▼              ▼
  UploadTracker   DownloadTracker
        │              │
        ▼              ▼
ConnectionUploadStats  ConnectionDownloadStats
```

`NetworkMonitor` enables per-connection TCP extended statistics and populates both outbound and inbound counters. `DownloadTracker` consumes `bytesReceived`; `UploadTracker` consumes `bytesSent`.

## Usage

```cpp
network::NetworkMonitor monitor;
network::DownloadTracker tracker;

auto snapshots = monitor.getConnectionTransferSnapshots();
// Optionally fill snapshot.processName via process attribution.

auto stats = tracker.update(snapshots);

for (const auto& entry : stats)
{
    std::cout << entry.toString() << "\n";
}
```

When a connection disappears from later snapshots, its final cumulative download total is preserved in `tracker.getClosedDownloadStats()` without affecting active counters.

## Expected Output

```text
Process:
chrome.exe

Remote:
142.250.190.78:443

Downloaded:
18.7 MB
```

## Edge Cases

- **First snapshot:** Establishes the OS byte baseline without counting existing traffic as new download activity.
- **Counter reset:** If an OS counter decreases, only the new counter value is treated as fresh progress (earlier progress already lives in the cumulative total).
- **Duplicate rows:** Duplicate connection rows within one snapshot are ignored, matching `ConnectionTracker` behavior.
- **Closed connections:** Final download totals move to `getClosedDownloadStats()` and are marked inactive.
- **UDP / no ESTATS:** Endpoints without per-connection counters are reported with zero byte values so lifecycle tracking still works.

## Build and Test

```bash
c++ -std=c++17 -o download_tracker_tests tests/network/DownloadTrackerTests.cpp
./download_tracker_tests

c++ -std=c++17 -o upload_tracker_tests tests/network/UploadTrackerTests.cpp
./upload_tracker_tests
```

## Acceptance Criteria

- Inbound transferred bytes are captured correctly from successive OS counters.
- Download data is associated with the correct process and remote endpoint.
- Multiple connections can be tracked independently.
- Closed connections are handled safely.
- Download counters remain consistent across measurements.
- Unit tests cover accumulation, closure, attribution, and counter-reset behavior.
- Documentation is updated.
