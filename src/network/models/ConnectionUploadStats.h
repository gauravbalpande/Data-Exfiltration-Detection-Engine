#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "Connection.h"

namespace network
{

/**
 * @brief Outbound upload statistics for an active or closed connection.
 *
 * Reusable Data Transfer Analysis record (Milestone M4) that associates
 * cumulative uploaded bytes with the owning process and remote endpoint.
 * Stores metrics only — no OS networking logic.
 */
class ConnectionUploadStats
{
public:
    ConnectionUploadStats() = default;

    ConnectionUploadStats(
        uint32_t processId,
        const std::string& processName,
        const std::string& remoteAddress,
        uint16_t remotePort,
        ProtocolType protocol,
        uint64_t uploadedBytes,
        std::chrono::system_clock::time_point firstObserved,
        std::chrono::system_clock::time_point lastUpdated,
        bool isActive);

    /**
     * @brief Build upload stats from a connection snapshot and measured bytes.
     */
    static ConnectionUploadStats fromSnapshot(
        const Connection& connection,
        uint64_t uploadedBytes,
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

    /// Cumulative outbound bytes transferred for this connection.
    uint64_t uploadedBytes = 0;

    /// When upload tracking started for this connection.
    std::chrono::system_clock::time_point firstObserved;

    /// When upload counters were last updated.
    std::chrono::system_clock::time_point lastUpdated;

    /// True while the connection is still active.
    bool isActive = true;

    /**
     * @brief Formats upload statistics for logging / inspection.
     *
     * Example:
     *   Process:
     *   python.exe
     *
     *   Remote:
     *   104.18.32.45:443
     *
     *   Uploaded:
     *   2.4 MB
     */
    std::string toString() const;
};

} // namespace network
