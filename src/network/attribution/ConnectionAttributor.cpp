#include "ConnectionAttributor.h"

#include <sstream>
#include <unordered_set>

namespace network
{

void ConnectionAttributor::syncProcesses(
    const std::unordered_map<uint32_t, process::ProcessInfo>& activeProcesses,
    const std::vector<process::ProcessEvent>& processEvents)
{
    for (const auto& [pid, processInfo] : activeProcesses)
    {
        process::ProcessInfo runningProcess = processInfo;
        runningProcess.status = process::ProcessStatus::RUNNING;
        processCache_[pid] = std::move(runningProcess);
    }

    for (const process::ProcessEvent& event : processEvents)
    {
        process::ProcessInfo cachedProcess = event.processInfo;
        if (event.type == process::ProcessEventType::TERMINATED)
        {
            cachedProcess.status = process::ProcessStatus::TERMINATED;
        }

        processCache_[cachedProcess.processId] = std::move(cachedProcess);
    }
}

std::vector<AttributedConnection> ConnectionAttributor::attribute(
    const std::vector<Connection>& connections)
{
    std::vector<AttributedConnection> attributed;
    attributed.reserve(connections.size());

    std::unordered_map<uint32_t, std::vector<AttributedConnection>> nextConnectionsByProcess;
    std::unordered_map<std::string, AttributedConnection> nextAttributedConnections;
    std::unordered_set<std::string> seen;
    seen.reserve(connections.size());

    for (const Connection& connection : connections)
    {
        const std::string key = makeConnectionKey(connection);
        if (!seen.insert(key).second)
        {
            continue; // Skip duplicate rows within the same snapshot.
        }

        AttributedConnection attributedConnection = buildAttributedConnection(connection);
        nextConnectionsByProcess[connection.processId].push_back(attributedConnection);
        nextAttributedConnections.emplace(key, attributedConnection);
        attributed.push_back(std::move(attributedConnection));
    }

    connectionsByProcess_ = std::move(nextConnectionsByProcess);
    attributedConnections_ = std::move(nextAttributedConnections);
    return attributed;
}

const std::unordered_map<uint32_t, std::vector<AttributedConnection>>&
ConnectionAttributor::getConnectionsByProcess() const
{
    return connectionsByProcess_;
}

const process::ProcessInfo* ConnectionAttributor::getLastKnownProcess(
    uint32_t processId) const
{
    const auto it = processCache_.find(processId);
    if (it == processCache_.end())
    {
        return nullptr;
    }

    return &it->second;
}

void ConnectionAttributor::reset()
{
    processCache_.clear();
    connectionsByProcess_.clear();
    attributedConnections_.clear();
}

std::string ConnectionAttributor::makeConnectionKey(const Connection& connection)
{
    std::ostringstream key;
    key << static_cast<int>(connection.protocol) << "|"
        << connection.processId << "|"
        << connection.localAddress << "|"
        << connection.localPort << "|"
        << connection.remoteAddress << "|"
        << connection.remotePort;
    return key.str();
}

AttributedConnection ConnectionAttributor::buildAttributedConnection(
    const Connection& connection) const
{
    const process::ProcessInfo* processInfo = getLastKnownProcess(connection.processId);
    if (processInfo == nullptr)
    {
        return AttributedConnection(connection, process::ProcessInfo{}, false, false);
    }

    return AttributedConnection(
        connection,
        *processInfo,
        true,
        processInfo->status == process::ProcessStatus::TERMINATED);
}

} // namespace network
