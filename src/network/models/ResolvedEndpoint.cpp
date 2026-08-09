#include "ResolvedEndpoint.h"

#include <sstream>

namespace network
{

ResolvedEndpoint::ResolvedEndpoint(
    uint32_t processId,
    const std::string& processName,
    const std::string& remoteIp,
    const std::string& resolvedDomain,
    AddressFamily addressFamily,
    ResolutionStatus resolutionStatus,
    bool cacheHit,
    std::chrono::system_clock::time_point timestamp)
    : processId(processId),
      processName(processName),
      remoteIp(remoteIp),
      resolvedDomain(resolvedDomain),
      addressFamily(addressFamily),
      resolutionStatus(resolutionStatus),
      cacheHit(cacheHit),
      timestamp(timestamp)
{
}

std::string ResolvedEndpoint::statusToString(ResolutionStatus status)
{
    switch (status)
    {
    case ResolutionStatus::RESOLVED:
        return "Resolved";
    case ResolutionStatus::NOT_FOUND:
        return "NotFound";
    case ResolutionStatus::FAILED:
        return "Failed";
    case ResolutionStatus::INVALID_IP:
        return "InvalidIp";
    case ResolutionStatus::CACHED:
        return "Cached";
    default:
        return "Unknown";
    }
}

std::string ResolvedEndpoint::toString() const
{
    std::ostringstream out;
    out << "Remote IP:\n"
        << remoteIp
        << "\n\n"
        << "Resolved Domain:\n"
        << (!resolvedDomain.empty() ? resolvedDomain : "");
    return out.str();
}

} // namespace network
