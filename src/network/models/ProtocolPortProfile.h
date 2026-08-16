#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "../models/Connection.h"
#include "../utils/NetworkUtils.h"

namespace network
{

/**
 * @brief Protocol and port metadata for an active network connection.
 *
 * Preserves transport protocol plus local/remote port numbers extracted
 * from the shared Connection model for Connection Intelligence reporting.
 */
class ProtocolPortProfile
{
public:
    ProtocolPortProfile() = default;

    ProtocolPortProfile(
        uint32_t processId,
        const std::string& processName,
        ProtocolType protocol,
        uint16_t localPort,
        uint16_t remotePort,
        ConnectionState connectionState,
        AddressFamily addressFamily,
        bool hasRemotePort,
        std::chrono::system_clock::time_point timestamp);

    /// Process ID that owns the connection.
    uint32_t processId = 0;

    /// Executable name when known (may be empty).
    std::string processName;

    /// Transport protocol (TCP / UDP / UNKNOWN).
    ProtocolType protocol = ProtocolType::UNKNOWN;

    /// Local endpoint port.
    uint16_t localPort = 0;

    /// Remote peer port (0 when unavailable).
    uint16_t remotePort = 0;

    /// Connection lifecycle state from the collector.
    ConnectionState connectionState = ConnectionState::UNKNOWN;

    /// Address family inferred from the local endpoint IP.
    AddressFamily addressFamily = AddressFamily::UNKNOWN;

    /// True when remotePort is a valid assigned port.
    bool hasRemotePort = false;

    /// Observation timestamp.
    std::chrono::system_clock::time_point timestamp;

    /**
     * @brief Formats protocol and port details for logging / inspection.
     *
     * Example:
     *   Protocol:
     *   TCP
     *
     *   Local Port:
     *   53142
     *
     *   Remote Port:
     *   443
     */
    std::string toString() const;
};

} // namespace network
