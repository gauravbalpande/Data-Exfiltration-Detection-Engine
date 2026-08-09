#pragma once

#include <string>

namespace network
{

/**
 * @brief Address family for a validated IP address.
 */
enum class AddressFamily
{
    IPv4,
    IPv6,
    UNKNOWN
};

/**
 * @brief Shared IP address helpers for Connection Intelligence.
 *
 * Pure string-level validation — no OS networking APIs — so these
 * utilities are reusable from collectors, intelligence modules, and tests.
 */
class NetworkUtils
{
public:
    /// Returns true when @p address is a syntactically valid IPv4 address.
    static bool isValidIpv4(const std::string& address);

    /// Returns true when @p address is a syntactically valid IPv6 address.
    static bool isValidIpv6(const std::string& address);

    /// Returns true when @p address is a valid IPv4 or IPv6 address.
    static bool isValidIpAddress(const std::string& address);

    /// Detects the address family of a validated IP, or UNKNOWN if invalid.
    static AddressFamily detectAddressFamily(const std::string& address);

    /// Human-readable address family label (IPv4 / IPv6 / Unknown).
    static std::string addressFamilyToString(AddressFamily family);

    /// True for loopback addresses (127.0.0.0/8 or ::1).
    static bool isLoopback(const std::string& address);

    /**
     * @brief True for unspecified / empty remote placeholders.
     *
     * Covers empty strings, 0.0.0.0, and :: — typical for listening
     * sockets that have no remote peer yet.
     */
    static bool isUnspecified(const std::string& address);

    /**
     * @brief True when @p address is a usable remote peer endpoint.
     *
     * Requires a valid IP that is not unspecified. Loopback remotes
     * are considered valid (local services communicating with itself).
     */
    static bool hasRemoteEndpoint(const std::string& address);
};

} // namespace network
