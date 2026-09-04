#include "TransferRateTracker.h"

#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace network
{

namespace
{
constexpr double kMinElapsedSeconds = 1e-9;
}

std::string TransferRateTracker::makeKey(const Connection& connection)
{
    // Same identity key as ConnectionTracker / UploadTracker / DownloadTracker.
    std::ostringstream key;
    key << static_cast<int>(connection.protocol) << "|"
        << connection.processId << "|"
        << connection.localAddress << "|"
        << connection.localPort << "|"
        << connection.remoteAddress << "|"
        << connection.remotePort;
    return key.str();
}

uint64_t TransferRateTracker::computeDelta(
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

double TransferRateTracker::computeRate(uint64_t byteDelta, double elapsedSeconds)
{
    if (elapsedSeconds <= kMinElapsedSeconds)
    {
        return 0.0;
    }

    return static_cast<double>(byteDelta) / elapsedSeconds;
}

ConnectionBandwidthStats TransferRateTracker::toConnectionStats(
    const TrackedTransfer& tracked,
    bool isActive) const
{
    return ConnectionBandwidthStats(
        tracked.connection.processId,
        tracked.processName,
        tracked.connection.remoteAddress,
        tracked.connection.remotePort,
        tracked.connection.protocol,
        tracked.cumulativeUploadedBytes,
        tracked.cumulativeDownloadedBytes,
        isActive ? tracked.uploadRateBps : 0.0,
        isActive ? tracked.downloadRateBps : 0.0,
        tracked.firstObserved,
        tracked.lastUpdated,
        isActive);
}

void TransferRateTracker::rebuildProcessStats()
{
    struct Aggregate
    {
        std::string processName;
        uint64_t uploadedBytes = 0;
        uint64_t downloadedBytes = 0;
        double uploadRateBps = 0.0;
        double downloadRateBps = 0.0;
        std::chrono::system_clock::time_point firstObserved;
        std::chrono::system_clock::time_point lastUpdated;
        std::size_t activeConnectionCount = 0;
        bool hasFirst = false;
    };

    std::unordered_map<uint32_t, Aggregate> aggregates;

    for (const auto& [key, tracked] : activeTransfers_)
    {
        (void)key;
        Aggregate& agg = aggregates[tracked.connection.processId];
        if (agg.processName.empty() && !tracked.processName.empty())
        {
            agg.processName = tracked.processName;
        }

        agg.uploadedBytes += tracked.cumulativeUploadedBytes;
        agg.downloadedBytes += tracked.cumulativeDownloadedBytes;
        agg.uploadRateBps += tracked.uploadRateBps;
        agg.downloadRateBps += tracked.downloadRateBps;
        ++agg.activeConnectionCount;

        if (!agg.hasFirst || tracked.firstObserved < agg.firstObserved)
        {
            agg.firstObserved = tracked.firstObserved;
            agg.hasFirst = true;
        }
        if (tracked.lastUpdated > agg.lastUpdated)
        {
            agg.lastUpdated = tracked.lastUpdated;
        }
    }

    // Fold preserved closed-connection totals into still-active processes.
    for (auto& [pid, agg] : aggregates)
    {
        const auto closedUp = closedUploadedByProcess_.find(pid);
        if (closedUp != closedUploadedByProcess_.end())
        {
            agg.uploadedBytes += closedUp->second;
        }

        const auto closedDown = closedDownloadedByProcess_.find(pid);
        if (closedDown != closedDownloadedByProcess_.end())
        {
            agg.downloadedBytes += closedDown->second;
        }

        const auto named = processNames_.find(pid);
        if (agg.processName.empty() && named != processNames_.end())
        {
            agg.processName = named->second;
        }

        const auto firstSeen = processFirstObserved_.find(pid);
        if (firstSeen != processFirstObserved_.end())
        {
            if (!agg.hasFirst || firstSeen->second < agg.firstObserved)
            {
                agg.firstObserved = firstSeen->second;
                agg.hasFirst = true;
            }
        }
    }

    // Detect processes that lost their last active connection.
    std::unordered_set<uint32_t> stillActive;
    stillActive.reserve(aggregates.size());
    for (const auto& [pid, agg] : aggregates)
    {
        (void)agg;
        stillActive.insert(pid);
        closedProcessStats_.erase(pid);
    }

    for (const auto& [pid, previous] : activeProcessStats_)
    {
        if (stillActive.find(pid) != stillActive.end())
        {
            continue;
        }

        const auto closedUp = closedUploadedByProcess_.find(pid);
        const auto closedDown = closedDownloadedByProcess_.find(pid);

        const uint64_t uploaded = closedUp != closedUploadedByProcess_.end()
                                      ? closedUp->second
                                      : previous.uploadedBytes;
        const uint64_t downloaded = closedDown != closedDownloadedByProcess_.end()
                                        ? closedDown->second
                                        : previous.downloadedBytes;

        closedProcessStats_[pid] = ProcessBandwidthStats(
            pid,
            previous.processName,
            uploaded,
            downloaded,
            0.0,
            0.0,
            previous.firstObserved,
            previous.lastUpdated,
            0);
    }

    activeProcessStats_.clear();
    for (const auto& [pid, agg] : aggregates)
    {
        processNames_[pid] = agg.processName;
        if (agg.hasFirst)
        {
            const auto existing = processFirstObserved_.find(pid);
            if (existing == processFirstObserved_.end() ||
                agg.firstObserved < existing->second)
            {
                processFirstObserved_[pid] = agg.firstObserved;
            }
        }

        activeProcessStats_.emplace(
            pid,
            ProcessBandwidthStats(
                pid,
                agg.processName,
                agg.uploadedBytes,
                agg.downloadedBytes,
                agg.uploadRateBps,
                agg.downloadRateBps,
                agg.firstObserved,
                agg.lastUpdated,
                agg.activeConnectionCount));
    }
}

std::vector<ConnectionBandwidthStats> TransferRateTracker::update(
    const std::vector<ConnectionTransferSnapshot>& snapshots)
{
    const auto now = std::chrono::system_clock::now();
    std::vector<ConnectionBandwidthStats> results;
    results.reserve(snapshots.size());

    std::unordered_map<std::string, TrackedTransfer> nextActive;
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

        TrackedTransfer tracked;
        tracked.connection = snapshot.connection;
        tracked.processName = snapshot.processName;
        tracked.lastUpdated = snapshot.connection.timestamp != std::chrono::system_clock::time_point{}
                                  ? snapshot.connection.timestamp
                                  : now;

        const auto existing = activeTransfers_.find(key);
        if (existing != activeTransfers_.end())
        {
            tracked.firstObserved = existing->second.firstObserved;
            tracked.baselineBytesSent = existing->second.baselineBytesSent;
            tracked.baselineBytesReceived = existing->second.baselineBytesReceived;
            tracked.cumulativeUploadedBytes = existing->second.cumulativeUploadedBytes;
            tracked.cumulativeDownloadedBytes = existing->second.cumulativeDownloadedBytes;

            if (seeded_)
            {
                const uint64_t uploadDelta = computeDelta(
                    tracked.baselineBytesSent,
                    snapshot.bytesSent);
                const uint64_t downloadDelta = computeDelta(
                    tracked.baselineBytesReceived,
                    snapshot.bytesReceived);

                tracked.cumulativeUploadedBytes += uploadDelta;
                tracked.cumulativeDownloadedBytes += downloadDelta;

                const double elapsedSeconds = std::chrono::duration<double>(
                                                  tracked.lastUpdated - existing->second.lastUpdated)
                                                  .count();

                // Guard against clock skew / identical timestamps.
                const double safeElapsed = elapsedSeconds > kMinElapsedSeconds
                                               ? elapsedSeconds
                                               : 0.0;

                tracked.uploadRateBps = computeRate(uploadDelta, safeElapsed);
                tracked.downloadRateBps = computeRate(downloadDelta, safeElapsed);
            }

            tracked.baselineBytesSent = snapshot.bytesSent;
            tracked.baselineBytesReceived = snapshot.bytesReceived;

            if (tracked.processName.empty())
            {
                tracked.processName = existing->second.processName;
            }
        }
        else
        {
            tracked.firstObserved = tracked.lastUpdated;
            tracked.baselineBytesSent = snapshot.bytesSent;
            tracked.baselineBytesReceived = snapshot.bytesReceived;
            tracked.cumulativeUploadedBytes = 0;
            tracked.cumulativeDownloadedBytes = 0;
            tracked.uploadRateBps = 0.0;
            tracked.downloadRateBps = 0.0;
        }

        if (!tracked.processName.empty())
        {
            processNames_[tracked.connection.processId] = tracked.processName;
        }

        const auto firstSeen = processFirstObserved_.find(tracked.connection.processId);
        if (firstSeen == processFirstObserved_.end() ||
            tracked.firstObserved < firstSeen->second)
        {
            processFirstObserved_[tracked.connection.processId] = tracked.firstObserved;
        }

        nextActive.emplace(key, std::move(tracked));
    }

    if (!seeded_)
    {
        activeTransfers_ = std::move(nextActive);
        seeded_ = true;

        activeConnectionStats_.clear();
        for (const auto& [key, tracked] : activeTransfers_)
        {
            activeConnectionStats_.emplace(key, toConnectionStats(tracked, true));
            results.push_back(activeConnectionStats_.at(key));
        }

        rebuildProcessStats();
        return results;
    }

    for (const auto& [key, previous] : activeTransfers_)
    {
        if (nextActive.find(key) == nextActive.end())
        {
            closedConnectionStats_[key] = toConnectionStats(previous, false);

            closedUploadedByProcess_[previous.connection.processId] +=
                previous.cumulativeUploadedBytes;
            closedDownloadedByProcess_[previous.connection.processId] +=
                previous.cumulativeDownloadedBytes;

            if (!previous.processName.empty())
            {
                processNames_[previous.connection.processId] = previous.processName;
            }
        }
    }

    activeTransfers_ = std::move(nextActive);

    activeConnectionStats_.clear();
    for (const auto& [key, tracked] : activeTransfers_)
    {
        activeConnectionStats_.emplace(key, toConnectionStats(tracked, true));
        results.push_back(activeConnectionStats_.at(key));
    }

    rebuildProcessStats();
    return results;
}

const std::unordered_map<std::string, ConnectionBandwidthStats>&
TransferRateTracker::getActiveConnectionStats() const
{
    return activeConnectionStats_;
}

const std::unordered_map<std::string, ConnectionBandwidthStats>&
TransferRateTracker::getClosedConnectionStats() const
{
    return closedConnectionStats_;
}

std::vector<ProcessBandwidthStats> TransferRateTracker::getProcessBandwidthStats() const
{
    std::vector<ProcessBandwidthStats> results;
    results.reserve(activeProcessStats_.size());
    for (const auto& [pid, stats] : activeProcessStats_)
    {
        (void)pid;
        results.push_back(stats);
    }

    std::sort(
        results.begin(),
        results.end(),
        [](const ProcessBandwidthStats& a, const ProcessBandwidthStats& b)
        {
            return a.processId < b.processId;
        });

    return results;
}

const std::unordered_map<uint32_t, ProcessBandwidthStats>&
TransferRateTracker::getClosedProcessStats() const
{
    return closedProcessStats_;
}

void TransferRateTracker::reset()
{
    activeTransfers_.clear();
    activeConnectionStats_.clear();
    closedConnectionStats_.clear();
    activeProcessStats_.clear();
    closedProcessStats_.clear();
    closedUploadedByProcess_.clear();
    closedDownloadedByProcess_.clear();
    processNames_.clear();
    processFirstObserved_.clear();
    seeded_ = false;
}

} // namespace network
