#include "ProtocolPortAnalyzer.h"

#include "../utils/NetworkUtils.h"

namespace network
{

bool ProtocolPortAnalyzer::tryAnalyze(
    const Connection& connection,
    const std::string& processName,
    ProtocolPortProfile& out) const
{
    // Every active row must expose a valid local port.
    if (!NetworkUtils::isValidPort(connection.localPort))
    {
        return false;
    }

    if (connection.protocol != ProtocolType::TCP
        && connection.protocol != ProtocolType::UDP)
    {
        return false;
    }

    AddressFamily family = AddressFamily::UNKNOWN;
    if (NetworkUtils::isValidIpAddress(connection.localAddress))
    {
        family = NetworkUtils::detectAddressFamily(connection.localAddress);
    }
    else if (NetworkUtils::isUnspecified(connection.localAddress))
    {
        // Listening on all interfaces — family unknown but still reportable.
        family = AddressFamily::UNKNOWN;
    }

    const bool remotePortAvailable = NetworkUtils::hasRemotePort(connection.remotePort);

    out = ProtocolPortProfile(
        connection.processId,
        processName,
        connection.protocol,
        connection.localPort,
        remotePortAvailable ? connection.remotePort : static_cast<uint16_t>(0),
        connection.state,
        family,
        remotePortAvailable,
        connection.timestamp);

    return true;
}

std::vector<ProtocolPortProfile> ProtocolPortAnalyzer::analyze(
    const std::vector<Connection>& connections) const
{
    std::vector<ProtocolPortProfile> profiles;
    profiles.reserve(connections.size());

    for (const Connection& connection : connections)
    {
        ProtocolPortProfile profile;
        if (tryAnalyze(connection, std::string{}, profile))
        {
            profiles.push_back(std::move(profile));
        }
    }

    return profiles;
}

std::vector<ProtocolPortProfile> ProtocolPortAnalyzer::analyze(
    const std::vector<AttributedConnection>& attributedConnections) const
{
    std::vector<ProtocolPortProfile> profiles;
    profiles.reserve(attributedConnections.size());

    for (const AttributedConnection& attributed : attributedConnections)
    {
        const std::string processName =
            attributed.hasProcessInfo ? attributed.processInfo.processName
                                      : std::string{};

        ProtocolPortProfile profile;
        if (tryAnalyze(attributed.connection, processName, profile))
        {
            profiles.push_back(std::move(profile));
        }
    }

    return profiles;
}

} // namespace network
