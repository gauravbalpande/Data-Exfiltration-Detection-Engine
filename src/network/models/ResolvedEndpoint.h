#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "../utils/NetworkUtils.h"
#include "RemoteEndpoint.h"

namespace network
{

/**
 * @brief Outcome of a reverse DNS attempt for a remote IP.
 */
enum class ResolutionStatus
{
    RESOLVED,   ///< Hostname obtained from DNS (or refreshed cache).
    NOT_FOUND,  ///< Lookup succeeded but no PTR / hostname available.
    FAILED,     ///< Lookup error (timeout, DNS unavailable, etc.).
    INVALID_IP, ///< Input remote IP failed validation.
    CACHED      ///< Hostname served from a non-expired cache entry.
};

/**
 * @brief Remote endpoint enriched with reverse-DNS hostname data.
 *
 * Built from a validated RemoteEndpoint so Connection Intelligence can
 * present human-readable destinations without changing collection code.
 */
class ResolvedEndpoint
{
public:
    ResolvedEndpoint() = default;

    ResolvedEndpoint(
        uint32_t processId,
        const std::string& processName,
        const std::string& remoteIp,
        const std::string& resolvedDomain,
        AddressFamily addressFamily,
        ResolutionStatus resolutionStatus,
        bool cacheHit,
        std::chrono::system_clock::time_point timestamp);

    /// Process ID that owns the connection.
    uint32_t processId = 0;

    /// Executable name when known (may be empty).
    std::string processName;

    /// Validated remote peer IP.
    std::string remoteIp;

    /// Reverse-DNS hostname, or empty when unavailable.
    std::string resolvedDomain;

    /// Address family of the remote IP.
    AddressFamily addressFamily = AddressFamily::UNKNOWN;

    /// How the hostname was obtained (or why it was not).
    ResolutionStatus resolutionStatus = ResolutionStatus::NOT_FOUND;

    /// True when the hostname came from DnsCache without a live lookup.
    bool cacheHit = false;

    /// Observation / resolution timestamp.
    std::chrono::system_clock::time_point timestamp;

    /// Human-readable resolution status label.
    static std::string statusToString(ResolutionStatus status);

    /**
     * @brief Formats resolved destination details for logging / inspection.
     *
     * Example:
     *   Remote IP:
     *   104.18.32.45
     *
     *   Resolved Domain:
     *   api.example.com
     */
    std::string toString() const;
};

} // namespace network
