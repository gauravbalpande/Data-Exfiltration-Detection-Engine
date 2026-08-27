# Upload Tracking

## Purpose

Track outbound data transferred by active network connections and attribute uploaded bytes to the owning process as part of Data Transfer Analysis (Milestone M4).

## Responsibilities

- Track bytes sent by active network connections
- Associate uploaded bytes with the owning PID and process name
- Maintain cumulative upload counters across snapshots
- Support multiple simultaneous connections independently
- Handle connection creation and closure safely
- Preserve first-seen and last-updated timestamps
- Remain reusable for future detection modules

## Data Model

```text
ConnectionUploadStats
├── process_id
├── process_name
├── remote_address
├── remote_port
├── protocol
├── uploaded_bytes
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

`UploadTracker` consumes `bytes_sent`. The shared snapshot is produced by
`NetworkMonitor::getConnectionTransferSnapshots()` via TCP ESTATS.

## Components

| Component | Location | Role |
|-----------|----------|------|
| `ConnectionUploadStats` | `src/network/models/` | Portable upload metrics record |
| `ConnectionTransferSnapshot` | `src/network/models/` | Shared OS byte-counter snapshot |
| `UploadTracker` | `src/network/tracker/` | Accumulates outbound bytes across snapshots |
| `NetworkMonitor::getConnectionTransferSnapshots` | `src/network/collectors/` | Reads TCP ESTATS `DataBytesOut` / `DataBytesIn` |
| `NetworkUtils::formatBytes` | `src/network/utils/` | Human-readable byte formatting |
| `Connection` | `src/network/models/` | Connection identity and timestamps |

## Usage

```cpp
network::NetworkMonitor monitor;
network::UploadTracker tracker;

auto snapshots = monitor.getConnectionTransferSnapshots();
// Optionally fill snapshot.processName via process attribution.

auto stats = tracker.update(snapshots);

for (const auto& entry : stats)
{
    std::cout << entry.toString() << "\n";
}
```

When a connection disappears from later snapshots, its final cumulative upload total is preserved in `tracker.getClosedUploadStats()` without affecting active counters.

## Expected Output

```text
Process:
python.exe

Remote:
104.18.32.45:443

Uploaded:
2.4 MB
```

## Edge Cases

- **First snapshot:** Establishes the OS byte baseline without counting existing traffic as new upload activity.
- **Counter reset:** If an OS counter decreases, only the new counter value is treated as fresh progress (earlier progress already lives in the cumulative total).
- **Duplicate rows:** Duplicate connection rows within one snapshot are ignored, matching `ConnectionTracker` behavior.
- **Closed connections:** Final upload totals move to `getClosedUploadStats()` and are marked inactive.

## Build and Test

```bash
c++ -std=c++17 -o upload_tracker_tests tests/network/UploadTrackerTests.cpp
./upload_tracker_tests
```

## Acceptance Criteria

- Outbound transferred bytes are captured correctly from successive OS counters.
- Upload data is associated with the correct process and remote endpoint.
- Multiple connections can be tracked independently.
- Closed connections do not corrupt transfer statistics.
- Upload counters remain consistent across measurements.
- Unit tests cover accumulation, closure, and counter-reset behavior.
