#include "DomainResolver.h"

#include <chrono>
#include <cstring>

#include "../utils/NetworkUtils.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace network
{

namespace
{
#if defined(_WIN32)
struct WinsockLifetime
{
    WinsockLifetime()
    {
        WSADATA data{};
        ready_ = (WSAStartup(MAKEWORD(2, 2), &data) == 0);
    }

    ~WinsockLifetime()
    {
        if (ready_)
        {
            WSACleanup();
        }
    }

    bool ready_ = false;
};

void ensureWinsock()
{
    static WinsockLifetime init;
    (void)init;
}
#endif
} // namespace

LookupResult DomainResolver::defaultReverseLookup(
    const std::string& ip,
    std::string& hostname)
{
    hostname.clear();

#if defined(_WIN32)
    ensureWinsock();
#endif

    sockaddr_storage storage{};
    socklen_t storageLen = 0;

    if (NetworkUtils::isValidIpv4(ip))
    {
        auto* addr4 = reinterpret_cast<sockaddr_in*>(&storage);
        addr4->sin_family = AF_INET;
        if (inet_pton(AF_INET, ip.c_str(), &addr4->sin_addr) != 1)
        {
            return LookupResult::Failed;
        }
        storageLen = sizeof(sockaddr_in);
    }
    else if (NetworkUtils::isValidIpv6(ip))
    {
        auto* addr6 = reinterpret_cast<sockaddr_in6*>(&storage);
        addr6->sin6_family = AF_INET6;
        if (inet_pton(AF_INET6, ip.c_str(), &addr6->sin6_addr) != 1)
        {
            return LookupResult::Failed;
        }
        storageLen = sizeof(sockaddr_in6);
    }
    else
    {
        return LookupResult::Failed;
    }

    char hostBuffer[NI_MAXHOST] = {};
    const int rc = getnameinfo(
        reinterpret_cast<sockaddr*>(&storage),
        storageLen,
        hostBuffer,
        static_cast<socklen_t>(sizeof(hostBuffer)),
        nullptr,
        0,
        NI_NAMEREQD);

    if (rc != 0)
    {
        if (rc == EAI_NONAME
#ifdef EAI_NODATA
            || rc == EAI_NODATA
#endif
        )
        {
            return LookupResult::NotFound;
        }
        return LookupResult::Failed;
    }

    hostname.assign(hostBuffer);
    if (hostname.empty() || hostname == ip)
    {
        hostname.clear();
        return LookupResult::NotFound;
    }

    return LookupResult::Success;
}

DomainResolver::DomainResolver(DnsCache& cache, LookupFn lookup)
    : cache_(cache),
      lookup_(lookup ? std::move(lookup) : LookupFn(defaultReverseLookup))
{
}

ResolvedEndpoint DomainResolver::buildResult(
    const RemoteEndpoint& endpoint,
    const std::string& hostname,
    ResolutionStatus status,
    bool cacheHit) const
{
    return ResolvedEndpoint(
        endpoint.processId,
        endpoint.processName,
        endpoint.remoteIp,
        hostname,
        endpoint.addressFamily,
        status,
        cacheHit,
        endpoint.timestamp);
}

ResolvedEndpoint DomainResolver::resolve(
    const RemoteEndpoint& endpoint,
    bool forceRefresh)
{
    if (!NetworkUtils::isValidIpAddress(endpoint.remoteIp))
    {
        return buildResult(endpoint, std::string{}, ResolutionStatus::INVALID_IP, false);
    }

    RemoteEndpoint normalized = endpoint;
    if (normalized.addressFamily == AddressFamily::UNKNOWN)
    {
        normalized.addressFamily =
            NetworkUtils::detectAddressFamily(endpoint.remoteIp);
    }

    if (forceRefresh)
    {
        cache_.invalidate(endpoint.remoteIp);
    }
    else
    {
        DnsCacheEntry cached;
        if (cache_.get(endpoint.remoteIp, cached))
        {
            const ResolutionStatus status =
                cached.hostname.empty() ? ResolutionStatus::NOT_FOUND
                                        : ResolutionStatus::CACHED;
            return buildResult(normalized, cached.hostname, status, true);
        }
    }

    std::string hostname;
    LookupResult lookupResult = LookupResult::Failed;

    try
    {
        lookupResult = lookup_(endpoint.remoteIp, hostname);
    }
    catch (...)
    {
        hostname.clear();
        cache_.put(endpoint.remoteIp, std::string{});
        return buildResult(normalized, std::string{}, ResolutionStatus::FAILED, false);
    }

    if (lookupResult == LookupResult::Success && !hostname.empty() && hostname != endpoint.remoteIp)
    {
        cache_.put(endpoint.remoteIp, hostname);
        return buildResult(normalized, hostname, ResolutionStatus::RESOLVED, false);
    }

    hostname.clear();
    cache_.put(endpoint.remoteIp, std::string{});

    if (lookupResult == LookupResult::Failed)
    {
        return buildResult(normalized, std::string{}, ResolutionStatus::FAILED, false);
    }

    return buildResult(normalized, std::string{}, ResolutionStatus::NOT_FOUND, false);
}

std::vector<ResolvedEndpoint> DomainResolver::resolveAll(
    const std::vector<RemoteEndpoint>& endpoints,
    bool forceRefresh)
{
    std::vector<ResolvedEndpoint> results;
    results.reserve(endpoints.size());

    for (const RemoteEndpoint& endpoint : endpoints)
    {
        results.push_back(resolve(endpoint, forceRefresh));
    }

    return results;
}

ResolvedEndpoint DomainResolver::resolveIp(
    const std::string& remoteIp,
    AddressFamily addressFamily,
    bool forceRefresh)
{
    RemoteEndpoint endpoint;
    endpoint.remoteIp = remoteIp;
    endpoint.addressFamily =
        (addressFamily == AddressFamily::UNKNOWN)
            ? NetworkUtils::detectAddressFamily(remoteIp)
            : addressFamily;
    endpoint.timestamp = std::chrono::system_clock::now();
    return resolve(endpoint, forceRefresh);
}

void DomainResolver::refresh(const std::string& remoteIp)
{
    cache_.invalidate(remoteIp);
}

DnsCache& DomainResolver::cache()
{
    return cache_;
}

const DnsCache& DomainResolver::cache() const
{
    return cache_;
}

} // namespace network
