#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace network
{

/**
 * @brief Per-process aggregated bandwidth and transfer-rate statistics.
 *
 * Reusable Data Transfer Analysis record (Milestone M4) that rolls up
 * connection-level upload/download totals and short-window rates for a
 * single owning process. Stores metrics only — no OS networking logic.
 */
class ProcessBandwidthStats
{
public:
    ProcessBandwidthStats() = default;

    ProcessBandwidthStats(
        uint32_t processId,
        const std::string& processName,
        uint64_t uploadedBytes,
        uint64_t downloadedBytes,
        double uploadRateBps,
        double downloadRateBps,
        std::chrono::system_clock::time_point firstObserved,
        std::chrono::system_clock::time_point lastUpdated,
        std::size_t activeConnectionCount);

    /// Process ID that owns the aggregated connections.
    uint32_t processId = 0;

    /// Executable name when known (may be empty).
    std::string processName;

    /// Cumulative outbound bytes across tracked connections.
    uint64_t uploadedBytes = 0;

    /// Cumulative inbound bytes across tracked connections.
    uint64_t downloadedBytes = 0;

    /// Aggregated short-window upload rate in bytes per second.
    double uploadRateBps = 0.0;

    /// Aggregated short-window download rate in bytes per second.
    double downloadRateBps = 0.0;

    /// Earliest first-observed timestamp among contributing connections.
    std::chrono::system_clock::time_point firstObserved;

    /// Latest last-updated timestamp among contributing connections.
    std::chrono::system_clock::time_point lastUpdated;

    /// Number of currently active connections for this process.
    std::size_t activeConnectionCount = 0;

    /// Total bytes transferred (upload + download).
    uint64_t totalTransferredBytes() const;

    /**
     * @brief Formats per-process bandwidth statistics for inspection.
     *
     * Example:
     *   Process:
     *   python.exe
     *
     *   Upload Rate:
     *   1.8 MB/s
     *
     *   Download Rate:
     *   320.0 KB/s
     *
     *   Total Transfer:
     *   42.6 MB
     */
    std::string toString() const;
};

} // namespace network
