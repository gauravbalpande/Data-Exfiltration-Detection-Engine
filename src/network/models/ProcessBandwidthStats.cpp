#include "ProcessBandwidthStats.h"

#include <sstream>

#include "../utils/NetworkUtils.h"

namespace network
{

ProcessBandwidthStats::ProcessBandwidthStats(
    uint32_t processId,
    const std::string& processName,
    uint64_t uploadedBytes,
    uint64_t downloadedBytes,
    double uploadRateBps,
    double downloadRateBps,
    std::chrono::system_clock::time_point firstObserved,
    std::chrono::system_clock::time_point lastUpdated,
    std::size_t activeConnectionCount)
    : processId(processId),
      processName(processName),
      uploadedBytes(uploadedBytes),
      downloadedBytes(downloadedBytes),
      uploadRateBps(uploadRateBps),
      downloadRateBps(downloadRateBps),
      firstObserved(firstObserved),
      lastUpdated(lastUpdated),
      activeConnectionCount(activeConnectionCount)
{
}

uint64_t ProcessBandwidthStats::totalTransferredBytes() const
{
    return uploadedBytes + downloadedBytes;
}

std::string ProcessBandwidthStats::toString() const
{
    std::ostringstream out;
    out << "Process:\n"
        << (processName.empty() ? "Unknown" : processName)
        << "\n\n"
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
