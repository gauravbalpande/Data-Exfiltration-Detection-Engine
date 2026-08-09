#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../cache/DnsCache.h"
#include "../models/RemoteEndpoint.h"
#include "../models/ResolvedEndpoint.h"

namespace network
{

/**
 * @brief Result of a single reverse-DNS attempt (before caching).
 */
enum class LookupResult
{
    Success,  ///< Hostname written to the out-parameter.
    NotFound, ///< No PTR / hostname for this IP.
    Failed    ///< Transient/hard lookup failure (timeout, DNS down).
};

/**
 * @brief Resolves validated remote IPs to hostnames via reverse DNS.
 *
 * Modular Connection Intelligence component:
 * - Supports IPv4 and IPv6
 * - Caches hostnames through DnsCache
 * - Handles lookup failures without affecting connection collection
 * - Returns an empty hostname when no domain is available
 *
 * A custom lookup function may be injected for unit tests.
 */
class DomainResolver
{
public:
    /**
     * @brief Reverse-DNS lookup callback.
     *
     * Must not throw. On Success, @p hostname is populated; otherwise cleared.
     */
    using LookupFn =
        std::function<LookupResult(const std::string& ip, std::string& hostname)>;

    /**
     * @brief Construct a resolver backed by @p cache.
     *
     * @param cache Shared DNS cache instance.
     * @param lookup Optional override; defaults to OS reverse DNS (getnameinfo).
     */
    explicit DomainResolver(DnsCache& cache, LookupFn lookup = {});

    /**
     * @brief Resolve a single remote endpoint, preferring cache when valid.
     */
    ResolvedEndpoint resolve(const RemoteEndpoint& endpoint, bool forceRefresh = false);

    /**
     * @brief Resolve a batch of remote endpoints.
     */
    std::vector<ResolvedEndpoint> resolveAll(
        const std::vector<RemoteEndpoint>& endpoints,
        bool forceRefresh = false);

    /**
     * @brief Resolve a raw IP (without process context).
     */
    ResolvedEndpoint resolveIp(
        const std::string& remoteIp,
        AddressFamily addressFamily = AddressFamily::UNKNOWN,
        bool forceRefresh = false);

    /// Invalidate cache for @p remoteIp so the next resolve hits DNS.
    void refresh(const std::string& remoteIp);

    DnsCache& cache();
    const DnsCache& cache() const;

    /// Platform reverse-DNS helper used as the default LookupFn.
    static LookupResult defaultReverseLookup(const std::string& ip, std::string& hostname);

private:
    ResolvedEndpoint buildResult(
        const RemoteEndpoint& endpoint,
        const std::string& hostname,
        ResolutionStatus status,
        bool cacheHit) const;

    DnsCache& cache_;
    LookupFn lookup_;
};

} // namespace network
