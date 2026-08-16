#pragma once

#include <string>
#include <vector>

#include "../models/Connection.h"
#include "../models/ProtocolPortProfile.h"
#include "../attribution/AttributedConnection.h"

namespace network
{

/**
 * @brief Extracts protocol and port metadata from active connections.
 *
 * Modular Connection Intelligence helper:
 * - Detects TCP and UDP connections
 * - Tracks local and remote ports
 * - Preserves protocol metadata from the shared Connection model
 * - Supports IPv4 and IPv6 (via local address family inference)
 * - Skips rows with invalid local ports gracefully
 */
class ProtocolPortAnalyzer
{
public:
    /**
     * @brief Analyze protocol/port metadata from raw connection snapshots.
     */
    std::vector<ProtocolPortProfile> analyze(
        const std::vector<Connection>& connections) const;

    /**
     * @brief Analyze connections with attributed process names when available.
     */
    std::vector<ProtocolPortProfile> analyze(
        const std::vector<AttributedConnection>& attributedConnections) const;

    /**
     * @brief Build a single profile when the connection has a valid local port.
     *
     * @return true when @p out was populated.
     */
    bool tryAnalyze(
        const Connection& connection,
        const std::string& processName,
        ProtocolPortProfile& out) const;
};

} // namespace network
