#include "DownloadTracker.h"

#include <sstream>
#include <unordered_set>

namespace network
{

std::string DownloadTracker::makeKey(const Connection& connection)
{
    // Same identity key as ConnectionTracker / UploadTracker: protocol + PID + 4-tuple.
    std::ostringstream key;
    key << static_cast<int>(connection.protocol) << "|"
        << connection.processId << "|"
        << connection.localAddress << "|"
        << connection.localPort << "|"
        << connection.remoteAddress << "|"
        << connection.remotePort;
    return key.str();
}

uint64_t DownloadTracker::computeDelta(
    uint64_t previousCounter,
    uint64_t currentCounter)
{
    if (currentCounter >= previousCounter)
    {
        return currentCounter - previousCounter;
    }

    // OS counter reset — earlier progress is already in cumulative totals.
    // Only the post-reset counter value is safely observable fresh progress.
    return currentCounter;
}

ConnectionDownloadStats DownloadTracker::toStats(
    const TrackedDownload& tracked,
    bool isActive) const
{
    return ConnectionDownloadStats(
        tracked.connection.processId,
        tracked.processName,
        tracked.connection.remoteAddress,
        tracked.connection.remotePort,
        tracked.connection.protocol,
        tracked.cumulativeDownloadedBytes,
        tracked.firstObserved,
        tracked.lastUpdated,
        isActive);
}

std::vector<ConnectionDownloadStats> DownloadTracker::update(
    const std::vector<ConnectionTransferSnapshot>& snapshots)
{
    const auto now = std::chrono::system_clock::now();
    std::vector<ConnectionDownloadStats> results;
    results.reserve(snapshots.size());

    std::unordered_map<std::string, TrackedDownload> nextActive;
    nextActive.reserve(snapshots.size());

    std::unordered_set<std::string> seen;
    seen.reserve(snapshots.size());

    for (const ConnectionTransferSnapshot& snapshot : snapshots)
    {
        const std::string key = makeKey(snapshot.connection);
        if (!seen.insert(key).second)
        {
            continue;
        }

        TrackedDownload tracked;
        tracked.connection = snapshot.connection;
        tracked.processName = snapshot.processName;
        tracked.lastUpdated = snapshot.connection.timestamp != std::chrono::system_clock::time_point{}
                                  ? snapshot.connection.timestamp
                                  : now;

        const auto existing = activeDownloads_.find(key);
        if (existing != activeDownloads_.end())
        {
            tracked.firstObserved = existing->second.firstObserved;
            tracked.baselineBytesReceived = existing->second.baselineBytesReceived;
            tracked.cumulativeDownloadedBytes = existing->second.cumulativeDownloadedBytes;

            if (seeded_)
            {
                const uint64_t delta = computeDelta(
                    tracked.baselineBytesReceived,
                    snapshot.bytesReceived);
                tracked.cumulativeDownloadedBytes += delta;
            }

            tracked.baselineBytesReceived = snapshot.bytesReceived;

            if (tracked.processName.empty())
            {
                tracked.processName = existing->second.processName;
            }
        }
        else
        {
            tracked.firstObserved = tracked.lastUpdated;
            tracked.baselineBytesReceived = snapshot.bytesReceived;
            tracked.cumulativeDownloadedBytes = 0;
        }

        nextActive.emplace(key, std::move(tracked));
    }

    if (!seeded_)
    {
        activeDownloads_ = std::move(nextActive);
        seeded_ = true;

        activeDownloadStats_.clear();
        for (const auto& [key, tracked] : activeDownloads_)
        {
            activeDownloadStats_.emplace(key, toStats(tracked, true));
            results.push_back(activeDownloadStats_.at(key));
        }
        return results;
    }

    for (const auto& [key, previous] : activeDownloads_)
    {
        if (nextActive.find(key) == nextActive.end())
        {
            closedDownloadStats_[key] = toStats(previous, false);
        }
    }

    activeDownloads_ = std::move(nextActive);

    activeDownloadStats_.clear();
    for (const auto& [key, tracked] : activeDownloads_)
    {
        activeDownloadStats_.emplace(key, toStats(tracked, true));
        results.push_back(activeDownloadStats_.at(key));
    }

    return results;
}

const std::unordered_map<std::string, ConnectionDownloadStats>&
DownloadTracker::getActiveDownloadStats() const
{
    return activeDownloadStats_;
}

const std::unordered_map<std::string, ConnectionDownloadStats>&
DownloadTracker::getClosedDownloadStats() const
{
    return closedDownloadStats_;
}

void DownloadTracker::reset()
{
    activeDownloads_.clear();
    activeDownloadStats_.clear();
    closedDownloadStats_.clear();
    seeded_ = false;
}

} // namespace network
