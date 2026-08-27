#include "ConnectionDownloadStats.h"

#include <sstream>

#include "../utils/NetworkUtils.h"

namespace network
{

ConnectionDownloadStats::ConnectionDownloadStats(
    uint32_t processId,
    const std::string& processName,
    const std::string& remoteAddress,
    uint16_t remotePort,
    ProtocolType protocol,
    uint64_t downloadedBytes,
    std::chrono::system_clock::time_point firstObserved,
    std::chrono::system_clock::time_point lastUpdated,
    bool isActive)
    : processId(processId),
      processName(processName),
      remoteAddress(remoteAddress),
      remotePort(remotePort),
      protocol(protocol),
      downloadedBytes(downloadedBytes),
      firstObserved(firstObserved),
      lastUpdated(lastUpdated),
      isActive(isActive)
{
}

ConnectionDownloadStats ConnectionDownloadStats::fromSnapshot(
    const Connection& connection,
    uint64_t downloadedBytes,
    const std::string& processName,
    std::chrono::system_clock::time_point firstObserved,
    bool isActive)
{
    const auto observed = firstObserved == std::chrono::system_clock::time_point{}
                              ? connection.timestamp
                              : firstObserved;

    return ConnectionDownloadStats(
        connection.processId,
        processName,
        connection.remoteAddress,
        connection.remotePort,
        connection.protocol,
        downloadedBytes,
        observed,
        connection.timestamp,
        isActive);
}

std::string ConnectionDownloadStats::toString() const
{
    std::ostringstream out;
    out << "Process:\n"
        << (processName.empty() ? "Unknown" : processName)
        << "\n\n"
        << "Remote:\n"
        << remoteAddress;

    if (NetworkUtils::hasRemotePort(remotePort))
    {
        out << ":" << remotePort;
    }

    out << "\n\n"
        << "Downloaded:\n"
        << NetworkUtils::formatBytes(downloadedBytes);

    return out.str();
}

} // namespace network
