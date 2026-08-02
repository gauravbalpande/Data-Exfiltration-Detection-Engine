#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "AttributedConnection.h"
#include "../models/Connection.h"
#include "../../process/events/ProcessEvent.h"
#include "../../process/models/ProcessInfo.h"

namespace network
{

/**
 * @brief Associates active network connections with owning processes by PID.
 *
 * This layer is additive to the existing ConnectionTracker/EventDispatcher
 * pipeline. It keeps its own ownership cache so attribution can survive after
 * the process exits and only the last-known metadata remains available.
 */
class ConnectionAttributor
{
public:
    /**
     * @brief Refresh the process cache used for later PID attribution.
     *
     * Pass active process snapshots and any process lifecycle events from
     * ProcessTracker so terminated processes remain attributable.
     */
    void syncProcesses(
        const std::unordered_map<uint32_t, process::ProcessInfo>& activeProcesses,
        const std::vector<process::ProcessEvent>& processEvents = {});

    /**
     * @brief Associate a connection snapshot with process metadata by PID.
     *
     * @return Enriched connection records in the same order they were observed.
     */
    std::vector<AttributedConnection> attribute(
        const std::vector<Connection>& connections);

    /// Active attributed connections grouped by owning PID.
    const std::unordered_map<uint32_t, std::vector<AttributedConnection>>&
    getConnectionsByProcess() const;

    /// Last-known process metadata for a PID, including terminated processes.
    const process::ProcessInfo* getLastKnownProcess(uint32_t processId) const;

    /// Clear all cached ownership and relationship state.
    void reset();

private:
    static std::string makeConnectionKey(const Connection& connection);
    AttributedConnection buildAttributedConnection(const Connection& connection) const;

    std::unordered_map<uint32_t, process::ProcessInfo> processCache_;
    std::unordered_map<uint32_t, std::vector<AttributedConnection>> connectionsByProcess_;
    std::unordered_map<std::string, AttributedConnection> attributedConnections_;
};

} // namespace network
