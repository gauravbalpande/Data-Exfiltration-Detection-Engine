#include "DnsCache.h"

namespace network
{

DnsCache::DnsCache(std::chrono::seconds ttl)
    : ttl_(ttl)
{
}

bool DnsCache::isExpired(
    const DnsCacheEntry& entry,
    std::chrono::system_clock::time_point now) const
{
    return now >= entry.expiresAt;
}

bool DnsCache::get(const std::string& ip, DnsCacheEntry& entry) const
{
    const auto it = entries_.find(ip);
    if (it == entries_.end())
    {
        return false;
    }

    const auto now = std::chrono::system_clock::now();
    if (isExpired(it->second, now))
    {
        return false;
    }

    entry = it->second;
    return true;
}

void DnsCache::put(const std::string& ip, const std::string& hostname)
{
    const auto now = std::chrono::system_clock::now();
    DnsCacheEntry entry;
    entry.hostname = hostname;
    entry.storedAt = now;
    entry.expiresAt = now + ttl_;
    entries_[ip] = std::move(entry);
}

void DnsCache::invalidate(const std::string& ip)
{
    entries_.erase(ip);
}

void DnsCache::clear()
{
    entries_.clear();
}

std::size_t DnsCache::purgeExpired()
{
    const auto now = std::chrono::system_clock::now();
    std::size_t removed = 0;

    for (auto it = entries_.begin(); it != entries_.end();)
    {
        if (isExpired(it->second, now))
        {
            it = entries_.erase(it);
            ++removed;
        }
        else
        {
            ++it;
        }
    }

    return removed;
}

std::size_t DnsCache::size() const
{
    return entries_.size();
}

void DnsCache::setTtl(std::chrono::seconds ttl)
{
    ttl_ = ttl;
}

std::chrono::seconds DnsCache::ttl() const
{
    return ttl_;
}

} // namespace network
