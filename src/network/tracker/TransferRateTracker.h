#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../models/Connection.h"
#include "../models/ConnectionBandwidthStats.h"
#include "../models/ConnectionTransferSnapshot.h"
#include "../models/ProcessBandwidthStats.h"

namespace network
{

/**
 * @brief Calculates short-window transfer rates from successive byte counters.
 *
 * Consumes OS-reported upload/download snapshots, accumulates session totals,
 * derives per-connection and per-process bandwidth statistics, and preserves
 * final metrics when connections close. Handles counter resets safely.
 */
class TransferRateTracker
{
public:
    /**
     * @brief Update bandwidth counters from the latest transfer snapshots.
     *
     * @param snapshots Current connections with OS-reported byte counters.
     * @return Bandwidth statistics for all active connections after this update.
     */
    std::vector<ConnectionBandwidthStats> update(
        const std::vector<ConnectionTransferSnapshot>& snapshots);

    /// Currently active per-connection bandwidth statistics.
    const std::unordered_map<std::string, ConnectionBandwidthStats>&
    getActiveConnectionStats() const;

    /// Final bandwidth statistics for connections that have closed.
    const std::unordered_map<std::string, ConnectionBandwidthStats>&
    getClosedConnectionStats() const;

    /**
     * @brief Aggregated short-window rates and totals for each active process.
     *
     * Includes cumulative bytes from closed connections that belong to the
     * same process when that process still has active connections; closed-only
     * processes are exposed via getClosedProcessStats().
     */
    std::vector<ProcessBandwidthStats> getProcessBandwidthStats() const;

    /// Final aggregated statistics for processes with no remaining active connections.
    const std::unordered_map<uint32_t, ProcessBandwidthStats>&
    getClosedProcessStats() const;

    /// Clears all tracked bandwidth state.
    void reset();

private:
    struct TrackedTransfer
    {
        Connection connection;
        std::string processName;
        uint64_t baselineBytesSent = 0;
        uint64_t baselineBytesReceived = 0;
        uint64_t cumulativeUploadedBytes = 0;
        uint64_t cumulativeDownloadedBytes = 0;
        double uploadRateBps = 0.0;
        double downloadRateBps = 0.0;
        std::chrono::system_clock::time_point firstObserved;
        std::chrono::system_clock::time_point lastUpdated;
    };

    static std::string makeKey(const Connection& connection);

    static uint64_t computeDelta(uint64_t previousCounter, uint64_t currentCounter);

    static double computeRate(uint64_t byteDelta, double elapsedSeconds);

    ConnectionBandwidthStats toConnectionStats(
        const TrackedTransfer& tracked,
        bool isActive) const;

    void rebuildProcessStats();

    std::unordered_map<std::string, TrackedTransfer> activeTransfers_;
    std::unordered_map<std::string, ConnectionBandwidthStats> activeConnectionStats_;
    std::unordered_map<std::string, ConnectionBandwidthStats> closedConnectionStats_;
    std::unordered_map<uint32_t, ProcessBandwidthStats> activeProcessStats_;
    std::unordered_map<uint32_t, ProcessBandwidthStats> closedProcessStats_;
    /// Preserved cumulative totals for closed connections, keyed by process id.
    std::unordered_map<uint32_t, uint64_t> closedUploadedByProcess_;
    std::unordered_map<uint32_t, uint64_t> closedDownloadedByProcess_;
    std::unordered_map<uint32_t, std::string> processNames_;
    std::unordered_map<uint32_t, std::chrono::system_clock::time_point> processFirstObserved_;
    bool seeded_ = false;
};

} // namespace network
