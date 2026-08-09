#include "RemoteEndpoint.h"

#include <sstream>

namespace network
{

RemoteEndpoint::RemoteEndpoint(
    uint32_t processId,
    const std::string& processName,
    ProtocolType protocol,
    const std::string& localIp,
    uint16_t localPort,
    const std::string& remoteIp,
    uint16_t remotePort,
    AddressFamily addressFamily,
    std::chrono::system_clock::time_point timestamp)
    : processId(processId),
      processName(processName),
      protocol(protocol),
      localIp(localIp),
      localPort(localPort),
      remoteIp(remoteIp),
      remotePort(remotePort),
      addressFamily(addressFamily),
      timestamp(timestamp)
{
}

std::string RemoteEndpoint::toString() const
{
    std::ostringstream out;
    out << "Process:\n"
        << (!processName.empty() ? processName : "Unknown")
        << "\n\n"
        << "Remote IP:\n"
        << remoteIp
        << "\n\n"
        << "Address Family:\n"
        << NetworkUtils::addressFamilyToString(addressFamily);
    return out.str();
}

} // namespace network
