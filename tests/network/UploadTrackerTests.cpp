#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "../../src/network/utils/NetworkUtils.h"
#include "../../src/network/utils/NetworkUtils.cpp"
#include "../../src/network/models/Connection.h"
#include "../../src/network/models/Connection.cpp"
#include "../../src/network/models/ConnectionUploadStats.h"
#include "../../src/network/models/ConnectionUploadStats.cpp"
#include "../../src/network/tracker/UploadTracker.h"
#include "../../src/network/tracker/UploadTracker.cpp"

using namespace network;

static int g_failures = 0;

#define EXPECT_TRUE(expr)                                                      \
    do                                                                         \
    {                                                                          \
        if (!(expr))                                                           \
        {                                                                      \
            std::cerr << "FAIL: " << #expr << " at " << __FILE__ << ":"        \
                      << __LINE__ << "\n";                                     \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

#define EXPECT_EQ(a, b)                                                        \
    do                                                                         \
    {                                                                          \
        if (!((a) == (b)))                                                     \
        {                                                                      \
            std::cerr << "FAIL: " << #a << " == " << #b << " at " << __FILE__  \
                      << ":" << __LINE__ << "\n";                              \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static ConnectionTransferSnapshot makeSnapshot(
    uint32_t pid,
    const std::string& remoteIp,
    uint16_t remotePort,
    uint64_t bytesSent,
    const std::string& processName = "python.exe")
{
    ConnectionTransferSnapshot snapshot;
    snapshot.connection = Connection(
        pid,
        "192.168.1.10",
        53142,
        remoteIp,
        remotePort,
        ProtocolType::TCP,
        ConnectionState::ESTABLISHED,
        std::chrono::system_clock::now());
    snapshot.bytesSent = bytesSent;
    snapshot.processName = processName;
    return snapshot;
}

static void testFormatBytes()
{
    EXPECT_EQ(NetworkUtils::formatBytes(512), std::string("512 B"));
    EXPECT_EQ(NetworkUtils::formatBytes(1024), std::string("1.0 KB"));
    EXPECT_EQ(NetworkUtils::formatBytes(2516582), std::string("2.4 MB"));
    EXPECT_EQ(NetworkUtils::formatBytes(891289600), std::string("850.0 MB"));
}

static void testFormattedOutput()
{
    ConnectionUploadStats stats(
        4120,
        "python.exe",
        "104.18.32.45",
        443,
        ProtocolType::TCP,
        2516582,
        std::chrono::system_clock::now(),
        std::chrono::system_clock::now(),
        true);

    const std::string expected =
        "Process:\n"
        "python.exe\n"
        "\n"
        "Remote:\n"
        "104.18.32.45:443\n"
        "\n"
        "Uploaded:\n"
        "2.4 MB";

    EXPECT_EQ(stats.toString(), expected);
}

static void testFirstSnapshotSeedsBaseline()
{
    UploadTracker tracker;
    const auto stats = tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 4096)});

    EXPECT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats[0].uploadedBytes, 0u);
    EXPECT_EQ(stats[0].processName, std::string("python.exe"));
    EXPECT_TRUE(stats[0].isActive);
}

static void testAccumulatesUploadDelta()
{
    UploadTracker tracker;
    tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 1000)});

    const auto stats = tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 3500)});

    EXPECT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats[0].uploadedBytes, 2500u);
}

static void testMultipleConnectionsTrackedIndependently()
{
    UploadTracker tracker;
    tracker.update({
        makeSnapshot(4120, "104.18.32.45", 443, 1000),
        makeSnapshot(4120, "93.184.216.34", 80, 500),
    });

    const auto stats = tracker.update({
        makeSnapshot(4120, "104.18.32.45", 443, 4000),
        makeSnapshot(4120, "93.184.216.34", 80, 900),
    });

    EXPECT_EQ(stats.size(), 2u);

    uint64_t firstUpload = 0;
    uint64_t secondUpload = 0;
    for (const ConnectionUploadStats& entry : stats)
    {
        if (entry.remoteAddress == "104.18.32.45")
        {
            firstUpload = entry.uploadedBytes;
        }
        else if (entry.remoteAddress == "93.184.216.34")
        {
            secondUpload = entry.uploadedBytes;
        }
    }

    EXPECT_EQ(firstUpload, 3000u);
    EXPECT_EQ(secondUpload, 400u);
}

static void testClosedConnectionPreservesStats()
{
    UploadTracker tracker;
    tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 1000)});
    tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 5000)});

    const auto activeAfterClose = tracker.update({});

    EXPECT_EQ(activeAfterClose.size(), 0u);
    EXPECT_EQ(tracker.getActiveUploadStats().size(), 0u);
    EXPECT_EQ(tracker.getClosedUploadStats().size(), 1u);

    const ConnectionUploadStats& closed =
        tracker.getClosedUploadStats().begin()->second;

    EXPECT_EQ(closed.uploadedBytes, 4000u);
    EXPECT_TRUE(!closed.isActive);
    EXPECT_EQ(closed.processName, std::string("python.exe"));
}

static void testCounterResetDoesNotCorruptTotals()
{
    UploadTracker tracker;
    tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 5000)});

    const auto stats = tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 1200)});

    EXPECT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats[0].uploadedBytes, 6200u);
}

static void testPreservedTimestamps()
{
    const auto firstSeen = std::chrono::system_clock::now() - std::chrono::seconds(30);
    const auto updated = std::chrono::system_clock::now();

    ConnectionTransferSnapshot snapshot;
    snapshot.connection = Connection(
        4120,
        "192.168.1.10",
        53142,
        "104.18.32.45",
        443,
        ProtocolType::TCP,
        ConnectionState::ESTABLISHED,
        updated);
    snapshot.bytesSent = 2048;
    snapshot.processName = "python.exe";

    UploadTracker tracker;

    ConnectionTransferSnapshot firstSnapshot = snapshot;
    firstSnapshot.connection.timestamp = firstSeen;
    firstSnapshot.bytesSent = 1024;
    tracker.update({firstSnapshot});

    const auto stats = tracker.update({snapshot});

    EXPECT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats[0].firstObserved, firstSeen);
    EXPECT_EQ(stats[0].lastUpdated, updated);
}

int main()
{
    testFormatBytes();
    testFormattedOutput();
    testFirstSnapshotSeedsBaseline();
    testAccumulatesUploadDelta();
    testMultipleConnectionsTrackedIndependently();
    testClosedConnectionPreservesStats();
    testCounterResetDoesNotCorruptTotals();
    testPreservedTimestamps();

    if (g_failures == 0)
    {
        std::cout << "All UploadTracker tests passed.\n";
        return 0;
    }

    std::cerr << g_failures << " test(s) failed.\n";
    return 1;
}
