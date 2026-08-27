#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../models/Connection.h"
#include "../models/ConnectionDownloadStats.h"
#include "../models/ConnectionTransferSnapshot.h"

namespace network
{

/**
 * @brief Tracks cumulative inbound bytes for active network connections.
 *
 * Compares successive OS byte counters per connection identity, accumulates
 * download deltas safely across snapshots, and preserves final statistics when
 * connections close.
 */
class DownloadTracker
{
public:
    /**
     * @brief Update download counters from the latest transfer snapshots.
     *
     * @param snapshots Current connections with OS-reported bytes received.
     * @return Download statistics for all active connections after this update.
     */
    std::vector<ConnectionDownloadStats> update(
        const std::vector<ConnectionTransferSnapshot>& snapshots);

    /// Currently active connection download statistics keyed by connection identity.
    const std::unordered_map<std::string, ConnectionDownloadStats>&
    getActiveDownloadStats() const;

    /// Final download statistics for connections that have closed.
    const std::unordered_map<std::string, ConnectionDownloadStats>&
    getClosedDownloadStats() const;

    /// Clears all tracked download state.
    void reset();

private:
    struct TrackedDownload
    {
        Connection connection;
        std::string processName;
        uint64_t baselineBytesReceived = 0;
        uint64_t cumulativeDownloadedBytes = 0;
        std::chrono::system_clock::time_point firstObserved;
        std::chrono::system_clock::time_point lastUpdated;
    };

    static std::string makeKey(const Connection& connection);

    static uint64_t computeDelta(uint64_t previousCounter, uint64_t currentCounter);

    ConnectionDownloadStats toStats(const TrackedDownload& tracked, bool isActive) const;

    std::unordered_map<std::string, TrackedDownload> activeDownloads_;
    std::unordered_map<std::string, ConnectionDownloadStats> activeDownloadStats_;
    std::unordered_map<std::string, ConnectionDownloadStats> closedDownloadStats_;
    bool seeded_ = false;
};

} // namespace network
