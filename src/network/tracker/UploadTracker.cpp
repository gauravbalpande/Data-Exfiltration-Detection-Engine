#include "UploadTracker.h"

#include <sstream>
#include <unordered_set>

namespace network
{

std::string UploadTracker::makeKey(const Connection& connection)
{
    // Same identity key as ConnectionTracker: protocol + PID + 4-tuple.
    std::ostringstream key;
    key << static_cast<int>(connection.protocol) << "|"
        << connection.processId << "|"
        << connection.localAddress << "|"
        << connection.localPort << "|"
        << connection.remoteAddress << "|"
        << connection.remotePort;
    return key.str();
}

uint64_t UploadTracker::computeDelta(
    uint64_t previousCounter,
    uint64_t currentCounter)
{
    if (currentCounter >= previousCounter)
    {
        return currentCounter - previousCounter;
    }

    // OS counter reset — preserve bytes observed at the previous baseline
    // and treat the new reading as fresh progress on top.
    return previousCounter + currentCounter;
}

ConnectionUploadStats UploadTracker::toStats(
    const TrackedUpload& tracked,
    bool isActive) const
{
    return ConnectionUploadStats(
        tracked.connection.processId,
        tracked.processName,
        tracked.connection.remoteAddress,
        tracked.connection.remotePort,
        tracked.connection.protocol,
        tracked.cumulativeUploadedBytes,
        tracked.firstObserved,
        tracked.lastUpdated,
        isActive);
}

std::vector<ConnectionUploadStats> UploadTracker::update(
    const std::vector<ConnectionTransferSnapshot>& snapshots)
{
    const auto now = std::chrono::system_clock::now();
    std::vector<ConnectionUploadStats> results;
    results.reserve(snapshots.size());

    std::unordered_map<std::string, TrackedUpload> nextActive;
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

        TrackedUpload tracked;
        tracked.connection = snapshot.connection;
        tracked.processName = snapshot.processName;
        tracked.lastUpdated = snapshot.connection.timestamp != std::chrono::system_clock::time_point{}
                                  ? snapshot.connection.timestamp
                                  : now;

        const auto existing = activeUploads_.find(key);
        if (existing != activeUploads_.end())
        {
            tracked.firstObserved = existing->second.firstObserved;
            tracked.baselineBytesSent = existing->second.baselineBytesSent;
            tracked.cumulativeUploadedBytes = existing->second.cumulativeUploadedBytes;

            if (seeded_)
            {
                const uint64_t delta = computeDelta(
                    tracked.baselineBytesSent,
                    snapshot.bytesSent);
                tracked.cumulativeUploadedBytes += delta;
            }

            tracked.baselineBytesSent = snapshot.bytesSent;

            if (tracked.processName.empty())
            {
                tracked.processName = existing->second.processName;
            }
        }
        else
        {
            tracked.firstObserved = tracked.lastUpdated;
            tracked.baselineBytesSent = snapshot.bytesSent;
            tracked.cumulativeUploadedBytes = 0;
        }

        nextActive.emplace(key, std::move(tracked));
    }

    if (!seeded_)
    {
        activeUploads_ = std::move(nextActive);
        seeded_ = true;

        activeUploadStats_.clear();
        for (const auto& [key, tracked] : activeUploads_)
        {
            activeUploadStats_.emplace(key, toStats(tracked, true));
            results.push_back(activeUploadStats_.at(key));
        }
        return results;
    }

    for (const auto& [key, previous] : activeUploads_)
    {
        if (nextActive.find(key) == nextActive.end())
        {
            closedUploadStats_[key] = toStats(previous, false);
        }
    }

    activeUploads_ = std::move(nextActive);

    activeUploadStats_.clear();
    for (const auto& [key, tracked] : activeUploads_)
    {
        activeUploadStats_.emplace(key, toStats(tracked, true));
        results.push_back(activeUploadStats_.at(key));
    }

    return results;
}

const std::unordered_map<std::string, ConnectionUploadStats>&
UploadTracker::getActiveUploadStats() const
{
    return activeUploadStats_;
}

const std::unordered_map<std::string, ConnectionUploadStats>&
UploadTracker::getClosedUploadStats() const
{
    return closedUploadStats_;
}

void UploadTracker::reset()
{
    activeUploads_.clear();
    activeUploadStats_.clear();
    closedUploadStats_.clear();
    seeded_ = false;
}

} // namespace network
