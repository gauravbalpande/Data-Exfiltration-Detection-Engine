#include "AttributedConnection.h"

#include <sstream>

namespace network
{

namespace
{
std::string protocolToString(ProtocolType protocol)
{
    switch (protocol)
    {
    case ProtocolType::TCP:
        return "TCP";
    case ProtocolType::UDP:
        return "UDP";
    default:
        return "UNKNOWN";
    }
}
} // namespace

AttributedConnection::AttributedConnection(
    const Connection& connection,
    const process::ProcessInfo& processInfo,
    bool hasProcessInfo,
    bool processTerminated)
    : connection(connection),
      processInfo(processInfo),
      hasProcessInfo(hasProcessInfo),
      processTerminated(processTerminated)
{
}

std::string AttributedConnection::toString() const
{
    std::ostringstream out;
    out << "Process:\n"
        << (hasProcessInfo && !processInfo.processName.empty()
                ? processInfo.processName
                : "Unknown")
        << "\n\n"
        << "PID:\n"
        << connection.processId
        << "\n\n"
        << "Connection:\n"
        << protocolToString(connection.protocol)
        << "\n\n"
        << "Remote Address:\n"
        << (!connection.remoteAddress.empty()
                ? connection.remoteAddress
                : connection.localAddress)
        << "\n\n"
        << "Port:\n"
        << (connection.remotePort != 0
                ? connection.remotePort
                : connection.localPort);

    if (processTerminated)
    {
        out << "\n\n"
            << "Process Status:\n"
            << "Terminated";
    }

    return out.str();
}

} // namespace network
