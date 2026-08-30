#pragma once

#include <cstdint>
#include <string>

#include "Connection.h"

namespace network
{

/**
 * @brief OS-reported byte counters for a single connection snapshot.
 *
 * Produced by the network collector (TCP ESTATS) and consumed by
 * UploadTracker / DownloadTracker. Counters are cumulative from the OS;
 * trackers convert successive readings into session deltas.
 */
struct ConnectionTransferSnapshot
{
    Connection connection;
    uint64_t bytesSent = 0;
    uint64_t bytesReceived = 0;
    std::string processName;
};

} // namespace network
