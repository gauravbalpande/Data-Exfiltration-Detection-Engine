#include <chrono>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "../../src/network/utils/NetworkUtils.h"
#include "../../src/network/utils/NetworkUtils.cpp"
#include "../../src/network/models/Connection.h"
#include "../../src/network/models/Connection.cpp"
#include "../../src/network/models/ConnectionTransferSnapshot.h"
#include "../../src/network/models/ConnectionBandwidthStats.h"
#include "../../src/network/models/ConnectionBandwidthStats.cpp"
#include "../../src/network/models/ProcessBandwidthStats.h"
#include "../../src/network/models/ProcessBandwidthStats.cpp"
#include "../../src/network/tracker/TransferRateTracker.h"
#include "../../src/network/tracker/TransferRateTracker.cpp"

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

#define EXPECT_NEAR(a, b, tol)                                                 \
    do                                                                         \
    {                                                                          \
        if (std::fabs((a) - (b)) > (tol))                                      \
        {                                                                      \
            std::cerr << "FAIL: |" << #a << " - " << #b << "| <= " << #tol     \
                      << " at " << __FILE__ << ":" << __LINE__ << "\n";        \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static ConnectionTransferSnapshot makeSnapshot(
    uint32_t pid,
    const std::string& remoteIp,
    uint16_t remotePort,
    uint64_t bytesSent,
    uint64_t bytesReceived,
    const std::chrono::system_clock::time_point& timestamp,
    const std::string& processName = "python.exe",
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
        timestamp);
    snapshot.bytesSent = bytesSent;
    snapshot.bytesReceived = bytesReceived;
    snapshot.processName = processName;
    return snapshot;
}

static void testFormatRate()
{
    EXPECT_EQ(NetworkUtils::formatRate(512.0), std::string("512.0 B/s"));
    EXPECT_EQ(NetworkUtils::formatRate(1024.0), std::string("1.0 KB/s"));
    EXPECT_EQ(NetworkUtils::formatRate(1.8 * 1024.0 * 1024.0), std::string("1.8 MB/s"));
    EXPECT_EQ(NetworkUtils::formatRate(320.0 * 1024.0), std::string("320.0 KB/s"));
}

static void testProcessFormattedOutput()
{
    const uint64_t totalBytes = static_cast<uint64_t>(42.6 * 1024.0 * 1024.0);
    const uint64_t uploaded = static_cast<uint64_t>(30.0 * 1024.0 * 1024.0);
    const uint64_t downloaded = totalBytes - uploaded;

    ProcessBandwidthStats stats(
        4120,
        "python.exe",
        uploaded,
        downloaded,
        1.8 * 1024.0 * 1024.0,
        320.0 * 1024.0,
        std::chrono::system_clock::now(),
        std::chrono::system_clock::now(),
        1);

    const std::string expected =
        "Process:\n"
        "python.exe\n"
        "\n"
        "Upload Rate:\n"
        "1.8 MB/s\n"
        "\n"
        "Download Rate:\n"
        "320.0 KB/s\n"
        "\n"
        "Total Transfer:\n"
        "42.6 MB";

    EXPECT_EQ(stats.toString(), expected);
}

static void testFirstSnapshotSeedsBaseline()
{
    const auto t0 = std::chrono::system_clock::now();
    TransferRateTracker tracker;

    const auto stats = tracker.update({
        makeSnapshot(4120, "104.18.32.45", 443, 4096, 8192, t0),
    });

    EXPECT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats[0].uploadedBytes, 0u);
    EXPECT_EQ(stats[0].downloadedBytes, 0u);
    EXPECT_NEAR(stats[0].uploadRateBps, 0.0, 1e-9);
    EXPECT_NEAR(stats[0].downloadRateBps, 0.0, 1e-9);
    EXPECT_TRUE(stats[0].isActive);
}

static void testShortWindowRates()
{
    const auto t0 = std::chrono::system_clock::now();
    const auto t1 = t0 + std::chrono::seconds(2);

    TransferRateTracker tracker;
    tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 1000, 500, t0)});

    // +3600 sent and +640 received over 2 seconds
    // upload = 1800 B/s, download = 320 B/s
    const auto stats = tracker.update({
        makeSnapshot(4120, "104.18.32.45", 443, 4600, 1140, t1),
    });

    EXPECT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats[0].uploadedBytes, 3600u);
    EXPECT_EQ(stats[0].downloadedBytes, 640u);
    EXPECT_NEAR(stats[0].uploadRateBps, 1800.0, 1e-6);
    EXPECT_NEAR(stats[0].downloadRateBps, 320.0, 1e-6);
    EXPECT_EQ(stats[0].totalTransferredBytes(), 4240u);
}

static void testExpectedExampleRates()
{
    const auto t0 = std::chrono::system_clock::now();
    const auto t1 = t0 + std::chrono::seconds(5);
    const auto t2 = t1 + std::chrono::seconds(1);

    const uint64_t uploadDelta = static_cast<uint64_t>(1.8 * 1024.0 * 1024.0);
    const uint64_t downloadDelta = static_cast<uint64_t>(320.0 * 1024.0);
    const uint64_t targetTotal = static_cast<uint64_t>(42.6 * 1024.0 * 1024.0);
    const uint64_t priorTotal = targetTotal - uploadDelta - downloadDelta;
    const uint64_t priorUpload = priorTotal / 2;
    const uint64_t priorDownload = priorTotal - priorUpload;

    TransferRateTracker tracker;
    // Seed baselines, then accumulate prior session traffic.
    tracker.update({
        makeSnapshot(4120, "104.18.32.45", 443, 0, 0, t0),
    });
    tracker.update({
        makeSnapshot(4120, "104.18.32.45", 443, priorUpload, priorDownload, t1),
    });

    // Final one-second window produces the example short-window rates.
    tracker.update({
        makeSnapshot(
            4120,
            "104.18.32.45",
            443,
            priorUpload + uploadDelta,
            priorDownload + downloadDelta,
            t2),
    });

    const auto processes = tracker.getProcessBandwidthStats();
    EXPECT_EQ(processes.size(), 1u);
    EXPECT_EQ(processes[0].processName, std::string("python.exe"));
    EXPECT_NEAR(processes[0].uploadRateBps, 1.8 * 1024.0 * 1024.0, 1.0);
    EXPECT_NEAR(processes[0].downloadRateBps, 320.0 * 1024.0, 1.0);
    EXPECT_EQ(processes[0].totalTransferredBytes(), targetTotal);

    const std::string expected =
        "Process:\n"
        "python.exe\n"
        "\n"
        "Upload Rate:\n"
        "1.8 MB/s\n"
        "\n"
        "Download Rate:\n"
        "320.0 KB/s\n"
        "\n"
        "Total Transfer:\n"
        "42.6 MB";
    EXPECT_EQ(processes[0].toString(), expected);
}

static void testPerConnectionIndependence()
{
    const auto t0 = std::chrono::system_clock::now();
    const auto t1 = t0 + std::chrono::seconds(1);

    TransferRateTracker tracker;
    tracker.update({
        makeSnapshot(4120, "104.18.32.45", 443, 1000, 100, t0, "python.exe", 53142),
        makeSnapshot(4120, "93.184.216.34", 80, 500, 200, t0, "python.exe", 53143),
    });

    const auto stats = tracker.update({
        makeSnapshot(4120, "104.18.32.45", 443, 3000, 1100, t1, "python.exe", 53142),
        makeSnapshot(4120, "93.184.216.34", 80, 1500, 700, t1, "python.exe", 53143),
    });

    EXPECT_EQ(stats.size(), 2u);

    double firstUploadRate = -1.0;
    double secondUploadRate = -1.0;
    for (const ConnectionBandwidthStats& entry : stats)
    {
        if (entry.remoteAddress == "104.18.32.45")
        {
            EXPECT_EQ(entry.uploadedBytes, 2000u);
            EXPECT_EQ(entry.downloadedBytes, 1000u);
            firstUploadRate = entry.uploadRateBps;
        }
        else if (entry.remoteAddress == "93.184.216.34")
        {
            EXPECT_EQ(entry.uploadedBytes, 1000u);
            EXPECT_EQ(entry.downloadedBytes, 500u);
            secondUploadRate = entry.uploadRateBps;
        }
    }

    EXPECT_NEAR(firstUploadRate, 2000.0, 1e-6);
    EXPECT_NEAR(secondUploadRate, 1000.0, 1e-6);

    const auto processes = tracker.getProcessBandwidthStats();
    EXPECT_EQ(processes.size(), 1u);
    EXPECT_NEAR(processes[0].uploadRateBps, 3000.0, 1e-6);
    EXPECT_NEAR(processes[0].downloadRateBps, 1500.0, 1e-6);
    EXPECT_EQ(processes[0].totalTransferredBytes(), 4500u);
    EXPECT_EQ(processes[0].activeConnectionCount, 2u);
}

static void testClosedConnectionPreservesTotals()
{
    const auto t0 = std::chrono::system_clock::now();
    const auto t1 = t0 + std::chrono::seconds(1);
    const auto t2 = t1 + std::chrono::seconds(1);

    TransferRateTracker tracker;
    tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 1000, 500, t0)});
    tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 5000, 2500, t1)});

    const auto activeAfterClose = tracker.update({});

    EXPECT_EQ(activeAfterClose.size(), 0u);
    EXPECT_EQ(tracker.getActiveConnectionStats().size(), 0u);
    EXPECT_EQ(tracker.getClosedConnectionStats().size(), 1u);

    const ConnectionBandwidthStats& closed =
        tracker.getClosedConnectionStats().begin()->second;

    EXPECT_EQ(closed.uploadedBytes, 4000u);
    EXPECT_EQ(closed.downloadedBytes, 2000u);
    EXPECT_NEAR(closed.uploadRateBps, 0.0, 1e-9);
    EXPECT_NEAR(closed.downloadRateBps, 0.0, 1e-9);
    EXPECT_TRUE(!closed.isActive);

    EXPECT_EQ(tracker.getProcessBandwidthStats().size(), 0u);
    EXPECT_EQ(tracker.getClosedProcessStats().size(), 1u);

    const ProcessBandwidthStats& closedProcess =
        tracker.getClosedProcessStats().begin()->second;
    EXPECT_EQ(closedProcess.totalTransferredBytes(), 6000u);
    EXPECT_NEAR(closedProcess.uploadRateBps, 0.0, 1e-9);

    // Re-open a connection for the same process — historical totals fold back in.
    tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 100, 50, t2)});
    EXPECT_EQ(tracker.getProcessBandwidthStats().size(), 1u);
    EXPECT_EQ(tracker.getProcessBandwidthStats()[0].totalTransferredBytes(), 6000u);
}

static void testCounterResetDoesNotProduceInvalidRates()
{
    const auto t0 = std::chrono::system_clock::now();
    const auto t1 = t0 + std::chrono::seconds(1);
    const auto t2 = t1 + std::chrono::seconds(2);

    TransferRateTracker tracker;
    tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 1000, 500, t0)});
    tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 5000, 2500, t1)});

    // OS counters reset to smaller values — treat as fresh post-reset progress.
    const auto stats = tracker.update({
        makeSnapshot(4120, "104.18.32.45", 443, 800, 300, t2),
    });

    EXPECT_EQ(stats.size(), 1u);
    // Prior cumulative 4000/2000 + post-reset 800/300
    EXPECT_EQ(stats[0].uploadedBytes, 4800u);
    EXPECT_EQ(stats[0].downloadedBytes, 2300u);
    EXPECT_NEAR(stats[0].uploadRateBps, 400.0, 1e-6);   // 800 / 2s
    EXPECT_NEAR(stats[0].downloadRateBps, 150.0, 1e-6); // 300 / 2s
    EXPECT_TRUE(stats[0].uploadRateBps >= 0.0);
    EXPECT_TRUE(stats[0].downloadRateBps >= 0.0);
}

static void testPreservedTimestamps()
{
    const auto firstSeen = std::chrono::system_clock::now() - std::chrono::seconds(30);
    const auto updated = std::chrono::system_clock::now();

    TransferRateTracker tracker;
    tracker.update({
        makeSnapshot(4120, "104.18.32.45", 443, 1024, 512, firstSeen),
    });

    const auto stats = tracker.update({
        makeSnapshot(4120, "104.18.32.45", 443, 2048, 1024, updated),
    });

    EXPECT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats[0].firstObserved, firstSeen);
    EXPECT_EQ(stats[0].lastUpdated, updated);
}

static void testZeroElapsedProducesZeroRate()
{
    const auto t0 = std::chrono::system_clock::now();

    TransferRateTracker tracker;
    tracker.update({makeSnapshot(4120, "104.18.32.45", 443, 1000, 500, t0)});

    // Same timestamp — avoid divide-by-zero / infinite rates.
    const auto stats = tracker.update({
        makeSnapshot(4120, "104.18.32.45", 443, 5000, 2500, t0),
    });

    EXPECT_EQ(stats.size(), 1u);
    EXPECT_EQ(stats[0].uploadedBytes, 4000u);
    EXPECT_EQ(stats[0].downloadedBytes, 2000u);
    EXPECT_NEAR(stats[0].uploadRateBps, 0.0, 1e-9);
    EXPECT_NEAR(stats[0].downloadRateBps, 0.0, 1e-9);
}

int main()
{
    testFormatRate();
    testProcessFormattedOutput();
    testFirstSnapshotSeedsBaseline();
    testShortWindowRates();
    testExpectedExampleRates();
    testPerConnectionIndependence();
    testClosedConnectionPreservesTotals();
    testCounterResetDoesNotProduceInvalidRates();
    testPreservedTimestamps();
    testZeroElapsedProducesZeroRate();

    if (g_failures == 0)
    {
        std::cout << "All TransferRateTracker tests passed.\n";
        return 0;
    }

    std::cerr << g_failures << " test(s) failed.\n";
    return 1;
}
