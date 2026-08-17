#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "Connection.h"
#include "../utils/NetworkUtils.h"

namespace network
{

/**
 * @brief Reusable connection metadata for network endpoint intelligence.
 *
 * Combines remote IP, resolved domain, transport protocol, and local/remote
 * port information into a single portable record for Connection Intelligence
 * (Milestone M3). Stores metadata only — no OS or DNS logic.
 */
class ConnectionMetadata
{
public:
    ConnectionMetadata() = default;

    ConnectionMetadata(
        uint32_t processId,
        const std::string& processName,
        const std::string& remoteIp,
        const std::string& domain,
        ProtocolType protocol,
        uint16_t localPort,
        uint16_t remotePort,
        AddressFamily addressFamily,
        bool hasRemotePort,
        std::chrono::system_clock::time_point timestamp);

    /**
     * @brief Build metadata from a Connection snapshot and optional domain.
     *
     * @param connection Source connection (IP, ports, protocol, PID).
     * @param domain Resolved hostname when available (may be empty).
     * @param processName Executable name when known (may be empty).
     */
    static ConnectionMetadata fromConnection(
        const Connection& connection,
        const std::string& domain = "",
        const std::string& processName = "");

    /// Process ID that owns the connection.
    uint32_t processId = 0;

    /// Executable name when known (may be empty).
    std::string processName;

    /// Remote peer IP address.
    std::string remoteIp;

    /// Resolved domain name for the remote IP (may be empty).
    std::string domain;

    /// Transport protocol (TCP / UDP / UNKNOWN).
    ProtocolType protocol = ProtocolType::UNKNOWN;

    /// Local endpoint port.
    uint16_t localPort = 0;

    /// Remote peer port (0 when unavailable).
    uint16_t remotePort = 0;

    /// Address family of the remote IP.
    AddressFamily addressFamily = AddressFamily::UNKNOWN;

    /// True when remotePort is a valid assigned port.
    bool hasRemotePort = false;

    /// Observation timestamp.
    std::chrono::system_clock::time_point timestamp;

    /**
     * @brief Formats connection metadata for logging / inspection.
     *
     * Example:
     *   Remote IP:
     *   104.18.32.45
     *
     *   Domain:
     *   api.example.com
     *
     *   Protocol:
     *   TCP
     *
     *   Remote Port:
     *   443
     */
    std::string toString() const;
};

} // namespace network
