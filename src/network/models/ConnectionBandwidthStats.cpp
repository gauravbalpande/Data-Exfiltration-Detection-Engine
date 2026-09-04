#include "ConnectionBandwidthStats.h"

#include <sstream>

#include "../utils/NetworkUtils.h"

namespace network
{

ConnectionBandwidthStats::ConnectionBandwidthStats(
    uint32_t processId,
    const std::string& processName,
    const std::string& remoteAddress,
    uint16_t remotePort,
    ProtocolType protocol,
    uint64_t uploadedBytes,
    uint64_t downloadedBytes,
    double uploadRateBps,
    double downloadRateBps,
    std::chrono::system_clock::time_point firstObserved,
    std::chrono::system_clock::time_point lastUpdated,
    bool isActive)
    : processId(processId),
      processName(processName),
      remoteAddress(remoteAddress),
      remotePort(remotePort),
      protocol(protocol),
      uploadedBytes(uploadedBytes),
      downloadedBytes(downloadedBytes),
      uploadRateBps(uploadRateBps),
      downloadRateBps(downloadRateBps),
      firstObserved(firstObserved),
      lastUpdated(lastUpdated),
      isActive(isActive)
{
}

uint64_t ConnectionBandwidthStats::totalTransferredBytes() const
{
    return uploadedBytes + downloadedBytes;
}

std::string ConnectionBandwidthStats::toString() const
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
        << "Upload Rate:\n"
        << NetworkUtils::formatRate(uploadRateBps)
        << "\n\n"
        << "Download Rate:\n"
        << NetworkUtils::formatRate(downloadRateBps)
        << "\n\n"
        << "Total Transfer:\n"
        << NetworkUtils::formatBytes(totalTransferredBytes());

    return out.str();
}

} // namespace network
