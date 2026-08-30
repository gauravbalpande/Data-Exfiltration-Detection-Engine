#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include <vector>

#include "../models/Connection.h"
#include "../models/ConnectionTransferSnapshot.h"

namespace network
{

class NetworkMonitor
{
public:
    // Collects active IPv4/IPv6 TCP connections and UDP endpoints.
    std::vector<Connection> getActiveConnections();

    /**
     * @brief Collect active connections with OS per-connection byte counters.
     *
     * For established TCP connections, enables and reads TCP ESTATS
     * (DataBytesOut / DataBytesIn). UDP endpoints and sockets without
     * ESTATS support are included with zero counters so trackers can
     * still observe lifecycle.
     */
    std::vector<ConnectionTransferSnapshot> getConnectionTransferSnapshots();
};

} // namespace network

#endif
