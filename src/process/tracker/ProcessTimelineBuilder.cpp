#include "ProcessTimelineBuilder.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "../../network/utils/NetworkUtils.h"

namespace process
{

void ProcessTimelineBuilder::build(const TimelineEventCollector& collector)
{
    build(collector.getAllEvents());
}

void ProcessTimelineBuilder::build(const std::vector<ProcessActivityEvent>& events)
{
    reset();

    if (events.empty())
    {
        return;
    }

    globalTimeline_ = events;
    std::stable_sort(globalTimeline_.begin(), globalTimeline_.end(), earlierThan);

    for (const ProcessActivityEvent& event : globalTimeline_)
    {
        processTimelines_[event.processId].push_back(event);
    }
}

const std::vector<ProcessActivityEvent>&
ProcessTimelineBuilder::getGlobalTimeline() const
{
    return globalTimeline_;
}

std::vector<ProcessActivityEvent> ProcessTimelineBuilder::getProcessTimeline(
    uint32_t processId) const
{
    const auto it = processTimelines_.find(processId);
    if (it == processTimelines_.end())
    {
        return {};
    }
    return it->second;
}

const std::unordered_map<uint32_t, std::vector<ProcessActivityEvent>>&
ProcessTimelineBuilder::getProcessTimelines() const
{
    return processTimelines_;
}

std::string ProcessTimelineBuilder::formatEntry(const ProcessActivityEvent& event)
{
    const std::string processLabel =
        event.processName.empty() ? "Unknown" : event.processName;

    std::ostringstream out;
    out << formatTimestampWithSeconds(event.timestamp) << " | "
        << processLabel << " | "
        << formatSummary(event);
    return out.str();
}

std::string ProcessTimelineBuilder::formatGlobalTimeline() const
{
    return formatLines(globalTimeline_);
}

std::string ProcessTimelineBuilder::formatProcessTimeline(uint32_t processId) const
{
    return formatLines(getProcessTimeline(processId));
}

void ProcessTimelineBuilder::reset()
{
    globalTimeline_.clear();
    processTimelines_.clear();
}

bool ProcessTimelineBuilder::earlierThan(
    const ProcessActivityEvent& left,
    const ProcessActivityEvent& right)
{
    if (left.timestamp != right.timestamp)
    {
        return left.timestamp < right.timestamp;
    }

    // Stable secondary ordering when timestamps match.
    if (left.processId != right.processId)
    {
        return left.processId < right.processId;
    }

    return static_cast<int>(left.eventType) < static_cast<int>(right.eventType);
}

std::string ProcessTimelineBuilder::formatTimestampWithSeconds(
    std::chrono::system_clock::time_point timestamp)
{
    const std::time_t timeValue = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tm{};

#if defined(_WIN32)
    gmtime_s(&tm, &timeValue);
#else
    gmtime_r(&timeValue, &tm);
#endif

    std::ostringstream out;
    out << std::put_time(&tm, "%H:%M:%S");
    return out.str();
}

std::string ProcessTimelineBuilder::formatSummary(const ProcessActivityEvent& event)
{
    switch (event.eventType)
    {
    case ProcessActivityEventType::PROCESS_SEEN:
        return "Process Seen";

    case ProcessActivityEventType::CONNECTION_OPENED:
    {
        if (!event.remoteAddress.empty())
        {
            return "Connected to " + event.remoteAddress;
        }
        return "Connected";
    }

    case ProcessActivityEventType::UPLOAD_ACTIVITY:
        return "Upload Activity: " +
               network::NetworkUtils::formatBytes(event.uploadedBytes);

    case ProcessActivityEventType::DOWNLOAD_ACTIVITY:
        return "Download Activity: " +
               network::NetworkUtils::formatBytes(event.downloadedBytes);

    case ProcessActivityEventType::CONNECTION_CLOSED:
    {
        if (!event.remoteAddress.empty())
        {
            return "Disconnected from " + event.remoteAddress;
        }
        return "Connection Closed";
    }

    case ProcessActivityEventType::SUSPICIOUS_ACTIVITY:
        if (!event.description.empty())
        {
            return event.description;
        }
        return "Suspicious Activity";

    default:
        return event.description.empty()
                   ? ProcessActivityEvent::defaultDescription(event.eventType)
                   : event.description;
    }
}

std::string ProcessTimelineBuilder::formatLines(
    const std::vector<ProcessActivityEvent>& events)
{
    if (events.empty())
    {
        return {};
    }

    std::ostringstream out;
    for (std::size_t i = 0; i < events.size(); ++i)
    {
        if (i > 0)
        {
            out << "\n";
        }
        out << formatEntry(events[i]);
    }
    return out.str();
}

} // namespace process
