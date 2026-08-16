#include "ProtocolPortProfile.h"

#include <sstream>

namespace network
{

ProtocolPortProfile::ProtocolPortProfile(
    uint32_t processId,
    const std::string& processName,
    ProtocolType protocol,
    uint16_t localPort,
    uint16_t remotePort,
    ConnectionState connectionState,
    AddressFamily addressFamily,
    bool hasRemotePort,
    std::chrono::system_clock::time_point timestamp)
    : processId(processId),
      processName(processName),
      protocol(protocol),
      localPort(localPort),
      remotePort(remotePort),
      connectionState(connectionState),
      addressFamily(addressFamily),
      hasRemotePort(hasRemotePort),
      timestamp(timestamp)
{
}

std::string ProtocolPortProfile::toString() const
{
    std::ostringstream out;
    out << "Protocol:\n"
        << NetworkUtils::protocolToString(protocol)
        << "\n\n"
        << "Local Port:\n"
        << localPort;

    if (hasRemotePort)
    {
        out << "\n\n"
            << "Remote Port:\n"
            << remotePort;
    }

    return out.str();
}

} // namespace network
