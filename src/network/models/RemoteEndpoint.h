#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "../utils/NetworkUtils.h"
#include "../models/Connection.h"

namespace network
{

/**
 * @brief Identified remote endpoint for an active network connection.
 *
 * Distinguishes local vs remote addressing and records the address family
 * of the remote peer. Reusable by Connection Intelligence components.
 */
class RemoteEndpoint
{
public:
    RemoteEndpoint() = default;

    RemoteEndpoint(
        uint32_t processId,
        const std::string& processName,
        ProtocolType protocol,
        const std::string& localIp,
        uint16_t localPort,
        const std::string& remoteIp,
        uint16_t remotePort,
        AddressFamily addressFamily,
        std::chrono::system_clock::time_point timestamp);

    /// Process ID that owns the connection.
    uint32_t processId = 0;

    /// Executable name when known (may be empty).
    std::string processName;

    /// Transport protocol (TCP / UDP / UNKNOWN).
    ProtocolType protocol = ProtocolType::UNKNOWN;

    /// Local endpoint IP address.
    std::string localIp;

    /// Local endpoint port.
    uint16_t localPort = 0;

    /// Remote peer IP address (validated before storage).
    std::string remoteIp;

    /// Remote peer port.
    uint16_t remotePort = 0;

    /// Address family of the remote IP.
    AddressFamily addressFamily = AddressFamily::UNKNOWN;

    /// Observation timestamp.
    std::chrono::system_clock::time_point timestamp;

    /**
     * @brief Formats remote endpoint details for logging / inspection.
     *
     * Example:
     *   Process:
     *   python.exe
     *
     *   Remote IP:
     *   104.18.32.45
     *
     *   Address Family:
     *   IPv4
     */
    std::string toString() const;
};

} // namespace network
