#pragma once

#include <string>

#include "../models/Connection.h"
#include "../../process/models/ProcessInfo.h"

namespace network
{

/**
 * @brief Links a network connection to the process that owns it.
 *
 * Keeps the raw connection unchanged while attaching the best available
 * process metadata from active or last-known process snapshots.
 */
class AttributedConnection
{
public:
    AttributedConnection() = default;

    AttributedConnection(
        const Connection& connection,
        const process::ProcessInfo& processInfo,
        bool hasProcessInfo,
        bool processTerminated);

    /// Original network connection snapshot.
    Connection connection;

    /// Last-known metadata for the owning PID.
    process::ProcessInfo processInfo;

    /// True when processInfo contains resolved ownership data.
    bool hasProcessInfo = false;

    /// True when ownership metadata came from a terminated process record.
    bool processTerminated = false;

    /**
     * @brief Format connection ownership in the requested reporting shape.
     */
    std::string toString() const;
};

} // namespace network
