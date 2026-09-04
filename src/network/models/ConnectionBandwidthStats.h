#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "Connection.h"

namespace network
{

/**
 * @brief Per-connection bandwidth and transfer-rate statistics.
 *
 * Reusable Data Transfer Analysis record (Milestone M4) that associates
 * cumulative byte totals and short-window transfer rates with a single
 * connection identity. Stores metrics only — no OS networking logic.
 */
class ConnectionBandwidthStats
{
public:
    ConnectionBandwidthStats() = default;

    ConnectionBandwidthStats(
        uint32_t processId,
        const std::string& processName,
        const std::string& remoteAddress,
        uint16_t remotePort,
        ProtocolType protocol,
        uint64_t uploadedBytes,
        uint64_t downloadedBytes,
        double uploadRateBps,
        double downloadRateBps,
        std::chrono::system_clock::time_point firstObserved,
        std::chrono::system_clock::time_point lastUpdated,
        bool isActive);

    /// Process ID that owns the connection.
    uint32_t processId = 0;

    /// Executable name when known (may be empty).
    std::string processName;

    /// Remote peer IP address.
    std::string remoteAddress;

    /// Remote peer port.
    uint16_t remotePort = 0;

    /// Transport protocol (TCP / UDP / UNKNOWN).
    ProtocolType protocol = ProtocolType::UNKNOWN;

    /// Cumulative outbound bytes transferred for this connection.
    uint64_t uploadedBytes = 0;

    /// Cumulative inbound bytes transferred for this connection.
    uint64_t downloadedBytes = 0;

    /// Short-window upload rate in bytes per second.
    double uploadRateBps = 0.0;

    /// Short-window download rate in bytes per second.
    double downloadRateBps = 0.0;

    /// When bandwidth tracking started for this connection.
    std::chrono::system_clock::time_point firstObserved;

    /// When bandwidth counters were last updated.
    std::chrono::system_clock::time_point lastUpdated;

    /// True while the connection is still active.
    bool isActive = true;

    /// Total bytes transferred (upload + download).
    uint64_t totalTransferredBytes() const;

    /**
     * @brief Formats per-connection bandwidth statistics for inspection.
     *
     * Example:
     *   Process:
     *   python.exe
     *
     *   Remote:
     *   104.18.32.45:443
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
