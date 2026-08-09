#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include <vector>

#include "../models/Connection.h"

namespace network
{

class NetworkMonitor
{
public:
    // Collects active IPv4/IPv6 TCP connections and UDP endpoints.
    std::vector<Connection> getActiveConnections();
};

} // namespace network

#endif
