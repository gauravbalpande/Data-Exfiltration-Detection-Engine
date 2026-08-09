#pragma once

#include <string>
#include <vector>

#include "../models/Connection.h"
#include "../models/RemoteEndpoint.h"
#include "../attribution/AttributedConnection.h"

namespace network
{

/**
 * @brief Extracts and validates remote IP endpoints from active connections.
 *
 * Modular Connection Intelligence helper:
 * - Distinguishes local vs remote endpoints
 * - Detects IPv4 / IPv6 address family
 * - Validates IP formatting before storing a RemoteEndpoint
 * - Skips listening sockets and malformed remotes gracefully
 */
class RemoteEndpointIdentifier
{
public:
    /**
     * @brief Identify remote endpoints from raw connection snapshots.
     *
     * Connections without a valid remote peer (listening / unspecified /
     * malformed) are skipped.
     */
    std::vector<RemoteEndpoint> identify(
        const std::vector<Connection>& connections) const;

    /**
     * @brief Identify remote endpoints using attributed process names when available.
     */
    std::vector<RemoteEndpoint> identify(
        const std::vector<AttributedConnection>& attributedConnections) const;

    /**
     * @brief Build a single RemoteEndpoint when the remote IP is valid.
     *
     * @return true when @p out was populated with a valid remote endpoint.
     */
    bool tryIdentify(
        const Connection& connection,
        const std::string& processName,
        RemoteEndpoint& out) const;
};

} // namespace network
