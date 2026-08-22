#include "ConnectionUploadStats.h"

#include <sstream>

#include "../utils/NetworkUtils.h"

namespace network
{

ConnectionUploadStats::ConnectionUploadStats(
    uint32_t processId,
    const std::string& processName,
    const std::string& remoteAddress,
    uint16_t remotePort,
    ProtocolType protocol,
    uint64_t uploadedBytes,
    std::chrono::system_clock::time_point firstObserved,
    std::chrono::system_clock::time_point lastUpdated,
    bool isActive)
    : processId(processId),
      processName(processName),
      remoteAddress(remoteAddress),
      remotePort(remotePort),
      protocol(protocol),
      uploadedBytes(uploadedBytes),
      firstObserved(firstObserved),
      lastUpdated(lastUpdated),
      isActive(isActive)
{
}

ConnectionUploadStats ConnectionUploadStats::fromSnapshot(
    const Connection& connection,
    uint64_t uploadedBytes,
    const std::string& processName,
    std::chrono::system_clock::time_point firstObserved,
    bool isActive)
{
    const auto observed = firstObserved == std::chrono::system_clock::time_point{}
                              ? connection.timestamp
                              : firstObserved;

    return ConnectionUploadStats(
        connection.processId,
        processName,
        connection.remoteAddress,
        connection.remotePort,
        connection.protocol,
        uploadedBytes,
        observed,
        connection.timestamp,
        isActive);
}

std::string ConnectionUploadStats::toString() const
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
        << "Uploaded:\n"
        << NetworkUtils::formatBytes(uploadedBytes);

    return out.str();
}

} // namespace network
