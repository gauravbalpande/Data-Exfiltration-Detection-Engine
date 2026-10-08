#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../events/ProcessActivityEvent.h"
#include "TimelineEventCollector.h"

namespace process
{

/**
 * @brief Builds chronological per-process and global activity timelines.
 *
 * Sorts collected ProcessActivityEvent records by timestamp and formats
 * readable timeline lines for reporting (Milestone M5).
 */
class ProcessTimelineBuilder
{
public:
    ProcessTimelineBuilder() = default;

    /**
     * @brief Build timelines from events already grouped by a collector.
     */
    void build(const TimelineEventCollector& collector);

    /**
     * @brief Build timelines from a flat list of activity events.
     *
     * Events are sorted by timestamp ascending. Empty input is a no-op.
     */
    void build(const std::vector<ProcessActivityEvent>& events);

    /// Chronologically sorted events across all processes.
    const std::vector<ProcessActivityEvent>& getGlobalTimeline() const;

    /// Chronologically sorted events for a single PID (empty when unknown).
    std::vector<ProcessActivityEvent> getProcessTimeline(uint32_t processId) const;

    /// All per-process timelines keyed by PID (each list sorted by time).
    const std::unordered_map<uint32_t, std::vector<ProcessActivityEvent>>&
    getProcessTimelines() const;

    /**
     * @brief Formats one readable timeline entry.
     *
     * Example:
     *   10:01:02 | python.exe | Process Seen
     *   10:01:10 | python.exe | Connected to 104.18.32.45:443
     *   10:01:15 | python.exe | Upload Activity: 12.5 MB
     */
    static std::string formatEntry(const ProcessActivityEvent& event);

    /// Formats the global timeline as newline-separated readable entries.
    std::string formatGlobalTimeline() const;

    /// Formats a single process timeline as newline-separated readable entries.
    std::string formatProcessTimeline(uint32_t processId) const;

    /// Clears built timeline state.
    void reset();

private:
    static bool earlierThan(
        const ProcessActivityEvent& left,
        const ProcessActivityEvent& right);

    static std::string formatTimestampWithSeconds(
        std::chrono::system_clock::time_point timestamp);

    static std::string formatSummary(const ProcessActivityEvent& event);

    static std::string formatLines(const std::vector<ProcessActivityEvent>& events);

    std::vector<ProcessActivityEvent> globalTimeline_;
    std::unordered_map<uint32_t, std::vector<ProcessActivityEvent>> processTimelines_;
};

} // namespace process
