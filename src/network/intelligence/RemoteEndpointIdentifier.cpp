#include "RemoteEndpointIdentifier.h"

#include "../utils/NetworkUtils.h"

namespace network
{

bool RemoteEndpointIdentifier::tryIdentify(
    const Connection& connection,
    const std::string& processName,
    RemoteEndpoint& out) const
{
    // Local endpoint is always recorded separately from the remote peer.
    const std::string& localIp = connection.localAddress;
    const std::string& remoteIp = connection.remoteAddress;

    // Listening sockets and closed rows often have an empty / unspecified remote.
    if (!NetworkUtils::hasRemoteEndpoint(remoteIp))
    {
        return false;
    }

    // Reject malformed remotes even if present.
    if (!NetworkUtils::isValidIpAddress(remoteIp))
    {
        return false;
    }

    // Prefer validating local too; fall back to empty local rather than aborting
    // remote identification when only the local side is malformed.
    std::string validatedLocal = localIp;
    if (!NetworkUtils::isValidIpAddress(localIp) && !NetworkUtils::isUnspecified(localIp))
    {
        validatedLocal.clear();
    }

    const AddressFamily family = NetworkUtils::detectAddressFamily(remoteIp);

    out = RemoteEndpoint(
        connection.processId,
        processName,
        connection.protocol,
        validatedLocal,
        connection.localPort,
        remoteIp,
        connection.remotePort,
        family,
        connection.timestamp);

    return true;
}

std::vector<RemoteEndpoint> RemoteEndpointIdentifier::identify(
    const std::vector<Connection>& connections) const
{
    std::vector<RemoteEndpoint> endpoints;
    endpoints.reserve(connections.size());

    for (const Connection& connection : connections)
    {
        RemoteEndpoint endpoint;
        if (tryIdentify(connection, std::string{}, endpoint))
        {
            endpoints.push_back(std::move(endpoint));
        }
    }

    return endpoints;
}

std::vector<RemoteEndpoint> RemoteEndpointIdentifier::identify(
    const std::vector<AttributedConnection>& attributedConnections) const
{
    std::vector<RemoteEndpoint> endpoints;
    endpoints.reserve(attributedConnections.size());

    for (const AttributedConnection& attributed : attributedConnections)
    {
        const std::string processName =
            attributed.hasProcessInfo ? attributed.processInfo.processName
                                      : std::string{};

        RemoteEndpoint endpoint;
        if (tryIdentify(attributed.connection, processName, endpoint))
        {
            endpoints.push_back(std::move(endpoint));
        }
    }

    return endpoints;
}

} // namespace network
