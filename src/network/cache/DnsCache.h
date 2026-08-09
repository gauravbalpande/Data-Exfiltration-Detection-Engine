#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

namespace network
{

/**
 * @brief Cached reverse-DNS hostname for a remote IP.
 */
struct DnsCacheEntry
{
    std::string hostname;
    std::chrono::system_clock::time_point storedAt;
    std::chrono::system_clock::time_point expiresAt;
};

/**
 * @brief In-memory TTL cache for reverse DNS results.
 *
 * Avoids duplicate lookups for the same remote IP. Negative results
 * (empty hostnames) may also be cached so repeated failures do not
 * hammer the DNS resolver.
 */
class DnsCache
{
public:
    explicit DnsCache(
        std::chrono::seconds ttl = std::chrono::seconds(300));

    /// Look up a non-expired cache entry. Returns false on miss / expiry.
    bool get(const std::string& ip, DnsCacheEntry& entry) const;

    /// Store (or refresh) a hostname for @p ip using the configured TTL.
    void put(const std::string& ip, const std::string& hostname);

    /// Force-remove a single IP so the next resolve performs a live lookup.
    void invalidate(const std::string& ip);

    /// Drop every cached entry.
    void clear();

    /// Remove expired entries; returns how many were erased.
    std::size_t purgeExpired();

    /// Number of entries currently stored (including expired until purged).
    std::size_t size() const;

    /// Update the TTL applied to future put() calls.
    void setTtl(std::chrono::seconds ttl);

    std::chrono::seconds ttl() const;

private:
    bool isExpired(const DnsCacheEntry& entry,
                   std::chrono::system_clock::time_point now) const;

    std::chrono::seconds ttl_;
    mutable std::unordered_map<std::string, DnsCacheEntry> entries_;
};

} // namespace network
