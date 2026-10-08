# Process Timeline Generation

## Purpose

Sort collected process activity events into chronological order and produce readable per-process and global timelines as part of Process Behavior Timeline (Milestone M5).

## Responsibilities

- Sort timeline events by timestamp
- Generate a per-process timeline for each PID
- Generate a global timeline across all processes
- Format readable timeline output for inspection and reporting
- Remain safe on empty input

## Components

| Component | Location | Role |
|-----------|----------|------|
| `ProcessTimelineBuilder` | `src/process/tracker/` | Sorts events and formats timelines |
| `TimelineEventCollector` | `src/process/tracker/` | Source of collected activity events |
| `ProcessActivityEvent` | `src/process/events/` | Individual timeline observations |

## Data Flow

```text
TimelineEventCollector
        │ ProcessActivityEvent[]
        ▼
ProcessTimelineBuilder
        │
        ├── getProcessTimeline(pid)   → sorted events for one process
        ├── getGlobalTimeline()       → sorted events across processes
        ├── formatProcessTimeline()   → readable lines for one process
        └── formatGlobalTimeline()    → readable lines for all processes
```

## Usage

```cpp
process::TimelineEventCollector collector;
// ... collect process / connection / transfer events ...

process::ProcessTimelineBuilder builder;
builder.build(collector);

std::cout << builder.formatProcessTimeline(4120) << "\n";
std::cout << builder.formatGlobalTimeline() << "\n";
```

Or build directly from a list of events:

```cpp
builder.build(events);
```

## Expected Output

```text
10:01:02 | python.exe | Process Seen
10:01:10 | python.exe | Connected to 104.18.32.45:443
10:01:15 | python.exe | Upload Activity: 12.5 MB
```

### Summary labels

| Event type | Summary |
|------------|---------|
| `process_seen` | `Process Seen` |
| `connection_opened` | `Connected to <remote>` |
| `upload_activity` | `Upload Activity: <bytes>` |
| `download_activity` | `Download Activity: <bytes>` |
| `connection_closed` | `Disconnected from <remote>` |
| `suspicious_activity` | Custom description when provided |

## Ordering

Events are sorted by UTC timestamp ascending. When timestamps match, ordering falls back to PID and then event type for deterministic output.

## Edge Cases

| Case | Behavior |
|------|----------|
| Empty input | Empty timelines; empty formatted strings |
| Multiple processes | Global timeline interleaves by time; per-process lists stay isolated |
| Unsorted input | Output is always chronological |
| Missing process name | Formatted as `Unknown` |

## Tests

```bash
c++ -std=c++17 -o process_timeline_builder_tests tests/process/ProcessTimelineBuilderTests.cpp
./process_timeline_builder_tests
```
