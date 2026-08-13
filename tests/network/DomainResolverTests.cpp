#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../../src/network/utils/NetworkUtils.h"
#include "../../src/network/utils/NetworkUtils.cpp"
#include "../../src/network/models/Connection.h"
#include "../../src/network/models/Connection.cpp"
#include "../../src/network/models/RemoteEndpoint.h"
#include "../../src/network/models/RemoteEndpoint.cpp"
#include "../../src/network/models/ResolvedEndpoint.h"
#include "../../src/network/models/ResolvedEndpoint.cpp"
#include "../../src/network/cache/DnsCache.h"
#include "../../src/network/cache/DnsCache.cpp"
#include "../../src/network/intelligence/DomainResolver.h"
#include "../../src/network/intelligence/DomainResolver.cpp"

using namespace network;

static int g_failures = 0;
static int g_lookupCalls = 0;

#define EXPECT_TRUE(expr)                                                      \
    do                                                                         \
    {                                                                          \
        if (!(expr))                                                           \
        {                                                                      \
            std::cerr << "FAIL: " << #expr << " at " << __FILE__ << ":"        \
                      << __LINE__ << "\n";                                     \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define EXPECT_EQ(a, b)                                                        \
    do                                                                         \
    {                                                                          \
        if (!((a) == (b)))                                                     \
        {                                                                      \
            std::cerr << "FAIL: " << #a << " == " << #b << " at " << __FILE__  \
                      << ":" << __LINE__ << "\n";                              \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static RemoteEndpoint makeRemote(
    const std::string& ip,
    AddressFamily family,
    const std::string& processName = "python.exe",
    uint32_t pid = 4120)
{
    return RemoteEndpoint(
        pid,
        processName,
        ProtocolType::TCP,
        family == AddressFamily::IPv6 ? "fe80::1" : "192.168.1.10",
        51544,
        ip,
        443,
        family,
        std::chrono::system_clock::now());
}

static LookupResult mockLookup(const std::string& ip, std::string& hostname)
{
    ++g_lookupCalls;
    hostname.clear();

    static const std::unordered_map<std::string, std::string> table = {
        {"104.18.32.45", "api.example.com"},
        {"2001:db8::53", "ipv6.example.com"},
        {"203.0.113.10", ""}, // explicit not found
    };

    const auto it = table.find(ip);
    if (it == table.end())
    {
        if (ip == "198.51.100.1")
        {
            return LookupResult::Failed;
        }
        return LookupResult::NotFound;
    }

    if (it->second.empty())
    {
        return LookupResult::NotFound;
    }

    hostname = it->second;
    return LookupResult::Success;
}

static void testDnsCache()
{
    DnsCache cache(std::chrono::seconds(1));
    EXPECT_EQ(cache.size(), static_cast<std::size_t>(0));

    DnsCacheEntry entry;
    EXPECT_TRUE(!cache.get("104.18.32.45", entry));

    cache.put("104.18.32.45", "api.example.com");
    EXPECT_EQ(cache.size(), static_cast<std::size_t>(1));
    EXPECT_TRUE(cache.get("104.18.32.45", entry));
    EXPECT_EQ(entry.hostname, std::string("api.example.com"));

    cache.invalidate("104.18.32.45");
    EXPECT_TRUE(!cache.get("104.18.32.45", entry));

    cache.put("104.18.32.45", "api.example.com");
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    EXPECT_TRUE(!cache.get("104.18.32.45", entry)); // expired
    EXPECT_EQ(cache.purgeExpired(), static_cast<std::size_t>(1));
    EXPECT_EQ(cache.size(), static_cast<std::size_t>(0));
}

static void testIpv4AndIpv6Resolution()
{
    g_lookupCalls = 0;
    DnsCache cache(std::chrono::seconds(300));
    DomainResolver resolver(cache, mockLookup);

    const ResolvedEndpoint ipv4 =
        resolver.resolve(makeRemote("104.18.32.45", AddressFamily::IPv4));
    EXPECT_EQ(ipv4.remoteIp, std::string("104.18.32.45"));
    EXPECT_EQ(ipv4.resolvedDomain, std::string("api.example.com"));
    EXPECT_EQ(ipv4.addressFamily, AddressFamily::IPv4);
    EXPECT_EQ(ipv4.resolutionStatus, ResolutionStatus::RESOLVED);
    EXPECT_TRUE(!ipv4.cacheHit);
    EXPECT_EQ(ipv4.processName, std::string("python.exe"));

    const std::string formatted = ipv4.toString();
    EXPECT_TRUE(formatted.find("Remote IP:") != std::string::npos);
    EXPECT_TRUE(formatted.find("104.18.32.45") != std::string::npos);
    EXPECT_TRUE(formatted.find("Resolved Domain:") != std::string::npos);
    EXPECT_TRUE(formatted.find("api.example.com") != std::string::npos);

    const ResolvedEndpoint ipv6 =
        resolver.resolve(makeRemote("2001:db8::53", AddressFamily::IPv6));
    EXPECT_EQ(ipv6.resolvedDomain, std::string("ipv6.example.com"));
    EXPECT_EQ(ipv6.addressFamily, AddressFamily::IPv6);
    EXPECT_EQ(ipv6.resolutionStatus, ResolutionStatus::RESOLVED);
}

static void testCacheHitAvoidsDuplicateLookup()
{
    g_lookupCalls = 0;
    DnsCache cache(std::chrono::seconds(300));
    DomainResolver resolver(cache, mockLookup);

    const auto first =
        resolver.resolve(makeRemote("104.18.32.45", AddressFamily::IPv4));
    EXPECT_EQ(first.resolutionStatus, ResolutionStatus::RESOLVED);
    EXPECT_EQ(g_lookupCalls, 1);

    const auto second =
        resolver.resolve(makeRemote("104.18.32.45", AddressFamily::IPv4));
    EXPECT_EQ(second.resolutionStatus, ResolutionStatus::CACHED);
    EXPECT_TRUE(second.cacheHit);
    EXPECT_EQ(second.resolvedDomain, std::string("api.example.com"));
    EXPECT_EQ(g_lookupCalls, 1); // no second live lookup
}

static void testLookupFailuresAreGraceful()
{
    g_lookupCalls = 0;
    DnsCache cache(std::chrono::seconds(300));
    DomainResolver resolver(cache, mockLookup);

    const auto missing =
        resolver.resolve(makeRemote("203.0.113.10", AddressFamily::IPv4));
    EXPECT_EQ(missing.resolutionStatus, ResolutionStatus::NOT_FOUND);
    EXPECT_TRUE(missing.resolvedDomain.empty());

    const auto failed =
        resolver.resolve(makeRemote("198.51.100.1", AddressFamily::IPv4));
    EXPECT_EQ(failed.resolutionStatus, ResolutionStatus::FAILED);
    EXPECT_TRUE(failed.resolvedDomain.empty());

    const auto invalid = resolver.resolveIp("not-an-ip");
    EXPECT_EQ(invalid.resolutionStatus, ResolutionStatus::INVALID_IP);
    EXPECT_TRUE(invalid.resolvedDomain.empty());
    EXPECT_EQ(g_lookupCalls, 2); // invalid never calls lookup
}

static void testForceRefreshUpdatesHostname()
{
    g_lookupCalls = 0;
    DnsCache cache(std::chrono::seconds(300));

    int generation = 0;
    DomainResolver resolver(
        cache,
        [&generation](const std::string& ip, std::string& hostname) -> LookupResult {
            ++g_lookupCalls;
            (void)ip;
            ++generation;
            hostname = (generation == 1) ? "old.example.com" : "api.example.com";
            return LookupResult::Success;
        });

    const auto first =
        resolver.resolve(makeRemote("104.18.32.45", AddressFamily::IPv4));
    EXPECT_EQ(first.resolvedDomain, std::string("old.example.com"));

    const auto cached =
        resolver.resolve(makeRemote("104.18.32.45", AddressFamily::IPv4));
    EXPECT_EQ(cached.resolutionStatus, ResolutionStatus::CACHED);
    EXPECT_EQ(g_lookupCalls, 1);

    resolver.refresh("104.18.32.45");
    const auto refreshed =
        resolver.resolve(makeRemote("104.18.32.45", AddressFamily::IPv4), true);
    EXPECT_EQ(refreshed.resolutionStatus, ResolutionStatus::RESOLVED);
    EXPECT_EQ(refreshed.resolvedDomain, std::string("api.example.com"));
    EXPECT_TRUE(!refreshed.cacheHit);
    EXPECT_EQ(g_lookupCalls, 2);
}

static void testResolveAllBatch()
{
    g_lookupCalls = 0;
    DnsCache cache(std::chrono::seconds(300));
    DomainResolver resolver(cache, mockLookup);

    std::vector<RemoteEndpoint> endpoints = {
        makeRemote("104.18.32.45", AddressFamily::IPv4),
        makeRemote("2001:db8::53", AddressFamily::IPv6),
        makeRemote("203.0.113.10", AddressFamily::IPv4),
    };

    const auto results = resolver.resolveAll(endpoints);
    EXPECT_EQ(results.size(), static_cast<std::size_t>(3));
    EXPECT_EQ(results[0].resolvedDomain, std::string("api.example.com"));
    EXPECT_EQ(results[1].resolvedDomain, std::string("ipv6.example.com"));
    EXPECT_TRUE(results[2].resolvedDomain.empty());
}

int main()
{
    testDnsCache();
    testIpv4AndIpv6Resolution();
    testCacheHitAvoidsDuplicateLookup();
    testLookupFailuresAreGraceful();
    testForceRefreshUpdatesHostname();
    testResolveAllBatch();

    if (g_failures == 0)
    {
        std::cout << "All domain resolution tests passed.\n";
        return 0;
    }

    std::cerr << g_failures << " assertion(s) failed.\n";
    return 1;
}
