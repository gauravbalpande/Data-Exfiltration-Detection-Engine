#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../models/Connection.h"
#include "../models/ConnectionTransferSnapshot.h"
#include "../models/ConnectionUploadStats.h"

namespace network
{

/**
 * @brief Tracks cumulative outbound bytes for active network connections.
 *
 * Compares successive OS byte counters per connection identity, accumulates
 * upload deltas safely across snapshots, and preserves final statistics when
 * connections close.
 */
class UploadTracker
{
public:
    /**
     * @brief Update upload counters from the latest transfer snapshots.
     *
     * @param snapshots Current connections with OS-reported bytes sent.
     * @return Upload statistics for all active connections after this update.
     */
    std::vector<ConnectionUploadStats> update(
        const std::vector<ConnectionTransferSnapshot>& snapshots);

    /// Currently active connection upload statistics keyed by connection identity.
    const std::unordered_map<std::string, ConnectionUploadStats>&
    getActiveUploadStats() const;

    /// Final upload statistics for connections that have closed.
    const std::unordered_map<std::string, ConnectionUploadStats>&
    getClosedUploadStats() const;

    /// Clears all tracked upload state.
    void reset();

private:
    struct TrackedUpload
    {
        Connection connection;
        std::string processName;
        uint64_t baselineBytesSent = 0;
        uint64_t cumulativeUploadedBytes = 0;
        std::chrono::system_clock::time_point firstObserved;
        std::chrono::system_clock::time_point lastUpdated;
    };

    static std::string makeKey(const Connection& connection);

    static uint64_t computeDelta(uint64_t previousCounter, uint64_t currentCounter);

    ConnectionUploadStats toStats(const TrackedUpload& tracked, bool isActive) const;

    std::unordered_map<std::string, TrackedUpload> activeUploads_;
    std::unordered_map<std::string, ConnectionUploadStats> activeUploadStats_;
    std::unordered_map<std::string, ConnectionUploadStats> closedUploadStats_;
    bool seeded_ = false;
};

} // namespace network
