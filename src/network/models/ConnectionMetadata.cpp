#include "ConnectionMetadata.h"

#include <sstream>

namespace network
{

ConnectionMetadata::ConnectionMetadata(
    uint32_t processId,
    const std::string& processName,
    const std::string& remoteIp,
    const std::string& domain,
    ProtocolType protocol,
    uint16_t localPort,
    uint16_t remotePort,
    AddressFamily addressFamily,
    bool hasRemotePort,
    std::chrono::system_clock::time_point timestamp)
    : processId(processId),
      processName(processName),
      remoteIp(remoteIp),
      domain(domain),
      protocol(protocol),
      localPort(localPort),
      remotePort(remotePort),
      addressFamily(addressFamily),
      hasRemotePort(hasRemotePort),
      timestamp(timestamp)
{
}

ConnectionMetadata ConnectionMetadata::fromConnection(
    const Connection& connection,
    const std::string& domain,
    const std::string& processName)
{
    const AddressFamily family =
        NetworkUtils::detectAddressFamily(connection.remoteAddress);

    return ConnectionMetadata(
        connection.processId,
        processName,
        connection.remoteAddress,
        domain,
        connection.protocol,
        connection.localPort,
        connection.remotePort,
        family,
        NetworkUtils::hasRemotePort(connection.remotePort),
        connection.timestamp);
}

std::string ConnectionMetadata::toString() const
{
    std::ostringstream out;
    out << "Remote IP:\n"
        << remoteIp
        << "\n\n"
        << "Domain:\n"
        << domain
        << "\n\n"
        << "Protocol:\n"
        << NetworkUtils::protocolToString(protocol);

    if (hasRemotePort)
    {
        out << "\n\n"
            << "Remote Port:\n"
            << remotePort;
    }

    return out.str();
}

} // namespace network
