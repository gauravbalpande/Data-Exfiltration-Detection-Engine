#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "../../src/network/utils/NetworkUtils.h"
#include "../../src/network/utils/NetworkUtils.cpp"
#include "../../src/network/models/Connection.h"
#include "../../src/network/models/Connection.cpp"
#include "../../src/network/models/ConnectionTransferSnapshot.h"
#include "../../src/network/models/ConnectionDownloadStats.h"
#include "../../src/network/models/ConnectionDownloadStats.cpp"
#include "../../src/network/tracker/DownloadTracker.h"
#include "../../src/network/tracker/DownloadTracker.cpp"

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
    uint64_t bytesReceived,
    const std::string& processName = "chrome.exe",
    uint16_t localPort = 53142)
{
    ConnectionTransferSnapshot snapshot;
    snapshot.connection = Connection(
        pid,
        "192.168.1.10",
        localPort,
        remoteIp,
        remotePort,
        ProtocolType::TCP,
        ConnectionState::ESTABLISHED,
        std::chrono::system_clock::now());
    snapshot.bytesReceived = bytesReceived;
    snapshot.processName = processName;
    return snapshot;
}

static void testFormattedOutput()
{
    // 18.7 MB == 18.7 * 1024 * 1024
    const uint64_t bytes = static_cast<uint64_t>(18.7 * 1024.0 * 1024.0);

    ConnectionDownloadStats stats(
        8840,
        "chrome.exe",
        "142.250.190.78",
        443,
        ProtocolType::TCP,
        bytes,
        std::chrono::system_clock::now(),
        std::chrono::system_clock::now(),
        true);

    const std::string expected =
        "Process:\n"
        "chrome.exe\n"
        "\n"
        "Remote:\n"
        "142.250.190.78:443\n"
        "\n"
        "Downloaded:\n"
        "18.7 MB";

    EXPECT_EQ(stats.toString(), expected);
}

static void testFirstSnapshotSeedsBaseline()
{
    DownloadTracker tracker;
    const auto stats = tracker.update({makeSnapshot(8840, "142.250.190.78", 443, 8192)});

    EXPECT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats[0].downloadedBytes, 0u);
    EXPECT_EQ(stats[0].processName, std::string("chrome.exe"));
    EXPECT_TRUE(stats[0].isActive);
}

static void testAccumulatesDownloadDelta()
{
    DownloadTracker tracker;
    tracker.update({makeSnapshot(8840, "142.250.190.78", 443, 1000)});

    const auto stats = tracker.update({makeSnapshot(8840, "142.250.190.78", 443, 4500)});

    EXPECT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats[0].downloadedBytes, 3500u);
}

static void testMultipleConnectionsTrackedIndependently()
{
    DownloadTracker tracker;
    tracker.update({
        makeSnapshot(8840, "142.250.190.78", 443, 1000, "chrome.exe", 53142),
        makeSnapshot(8840, "93.184.216.34", 80, 500, "chrome.exe", 53143),
    });

    const auto stats = tracker.update({
        makeSnapshot(8840, "142.250.190.78", 443, 5000, "chrome.exe", 53142),
        makeSnapshot(8840, "93.184.216.34", 80, 1800, "chrome.exe", 53143),
    });

    EXPECT_EQ(stats.size(), 2u);

    uint64_t firstDownload = 0;
    uint64_t secondDownload = 0;
    for (const ConnectionDownloadStats& entry : stats)
    {
        if (entry.remoteAddress == "142.250.190.78")
        {
            firstDownload = entry.downloadedBytes;
        }
        else if (entry.remoteAddress == "93.184.216.34")
        {
            secondDownload = entry.downloadedBytes;
        }
    }

    EXPECT_EQ(firstDownload, 4000u);
    EXPECT_EQ(secondDownload, 1300u);
}

static void testClosedConnectionPreservesStats()
{
    DownloadTracker tracker;
    tracker.update({makeSnapshot(8840, "142.250.190.78", 443, 1000)});
    tracker.update({makeSnapshot(8840, "142.250.190.78", 443, 9000)});

    const auto activeAfterClose = tracker.update({});

    EXPECT_EQ(activeAfterClose.size(), 0u);
    EXPECT_EQ(tracker.getActiveDownloadStats().size(), 0u);
    EXPECT_EQ(tracker.getClosedDownloadStats().size(), 1u);

    const ConnectionDownloadStats& closed =
        tracker.getClosedDownloadStats().begin()->second;

    EXPECT_EQ(closed.downloadedBytes, 8000u);
    EXPECT_TRUE(!closed.isActive);
    EXPECT_EQ(closed.processName, std::string("chrome.exe"));
}

static void testCounterResetDoesNotCorruptTotals()
{
    DownloadTracker tracker;
    // Seed baseline at 1000, then grow to 5000 → cumulative = 4000.
    tracker.update({makeSnapshot(8840, "142.250.190.78", 443, 1000)});
    tracker.update({makeSnapshot(8840, "142.250.190.78", 443, 5000)});

    // OS counter resets to 1200 — only the fresh post-reset value is added.
    const auto stats = tracker.update({makeSnapshot(8840, "142.250.190.78", 443, 1200)});

    EXPECT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats[0].downloadedBytes, 5200u);
}

static void testPreservedTimestamps()
{
    const auto firstSeen = std::chrono::system_clock::now() - std::chrono::seconds(30);
    const auto updated = std::chrono::system_clock::now();

    ConnectionTransferSnapshot snapshot;
    snapshot.connection = Connection(
        8840,
        "192.168.1.10",
        53142,
        "142.250.190.78",
        443,
        ProtocolType::TCP,
        ConnectionState::ESTABLISHED,
        updated);
    snapshot.bytesReceived = 4096;
    snapshot.processName = "chrome.exe";

    DownloadTracker tracker;

    ConnectionTransferSnapshot firstSnapshot = snapshot;
    firstSnapshot.connection.timestamp = firstSeen;
    firstSnapshot.bytesReceived = 1024;
    tracker.update({firstSnapshot});

    const auto stats = tracker.update({snapshot});

    EXPECT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats[0].firstObserved, firstSeen);
    EXPECT_EQ(stats[0].lastUpdated, updated);
}

static void testAssociatedWithCorrectProcess()
{
    DownloadTracker tracker;
    tracker.update({makeSnapshot(8840, "142.250.190.78", 443, 1000, "chrome.exe")});

    const auto stats = tracker.update({makeSnapshot(8840, "142.250.190.78", 443, 3000, "chrome.exe")});

    EXPECT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats[0].processId, 8840u);
    EXPECT_EQ(stats[0].processName, std::string("chrome.exe"));
    EXPECT_EQ(stats[0].remoteAddress, std::string("142.250.190.78"));
    EXPECT_EQ(stats[0].remotePort, static_cast<uint16_t>(443));
    EXPECT_EQ(stats[0].downloadedBytes, 2000u);
}

int main()
{
    testFormattedOutput();
    testFirstSnapshotSeedsBaseline();
    testAccumulatesDownloadDelta();
    testMultipleConnectionsTrackedIndependently();
    testClosedConnectionPreservesStats();
    testCounterResetDoesNotCorruptTotals();
    testPreservedTimestamps();
    testAssociatedWithCorrectProcess();

    if (g_failures == 0)
    {
        std::cout << "All DownloadTracker tests passed.\n";
        return 0;
    }

    std::cerr << g_failures << " test(s) failed.\n";
    return 1;
}
