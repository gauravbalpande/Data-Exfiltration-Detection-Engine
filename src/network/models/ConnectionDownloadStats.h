#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "Connection.h"

namespace network
{

/**
 * @brief Inbound download statistics for an active or closed connection.
 *
 * Reusable Data Transfer Analysis record (Milestone M4) that associates
 * cumulative downloaded bytes with the owning process and remote endpoint.
 * Stores metrics only — no OS networking logic.
 */
class ConnectionDownloadStats
{
public:
    ConnectionDownloadStats() = default;

    ConnectionDownloadStats(
        uint32_t processId,
        const std::string& processName,
        const std::string& remoteAddress,
        uint16_t remotePort,
        ProtocolType protocol,
        uint64_t downloadedBytes,
        std::chrono::system_clock::time_point firstObserved,
        std::chrono::system_clock::time_point lastUpdated,
        bool isActive);

    /**
     * @brief Build download stats from a connection snapshot and measured bytes.
     */
    static ConnectionDownloadStats fromSnapshot(
        const Connection& connection,
        uint64_t downloadedBytes,
        const std::string& processName = "",
        std::chrono::system_clock::time_point firstObserved = {},
        bool isActive = true);

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

    /// Cumulative inbound bytes transferred for this connection.
    uint64_t downloadedBytes = 0;

    /// When download tracking started for this connection.
    std::chrono::system_clock::time_point firstObserved;

    /// When download counters were last updated.
    std::chrono::system_clock::time_point lastUpdated;

    /// True while the connection is still active.
    bool isActive = true;

    /**
     * @brief Formats download statistics for logging / inspection.
     *
     * Example:
     *   Process:
     *   chrome.exe
     *
     *   Remote:
     *   142.250.190.78:443
     *
     *   Downloaded:
     *   18.7 MB
     */
    std::string toString() const;
};

} // namespace network
