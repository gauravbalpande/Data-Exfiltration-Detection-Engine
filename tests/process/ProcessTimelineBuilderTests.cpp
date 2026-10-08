#include <chrono>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#include "../../src/network/utils/NetworkUtils.h"
#include "../../src/network/utils/NetworkUtils.cpp"
#include "../../src/network/models/Connection.h"
#include "../../src/network/models/Connection.cpp"
#include "../../src/process/events/ProcessActivityEvent.h"
#include "../../src/process/events/ProcessActivityEvent.cpp"
#include "../../src/process/events/ProcessEvent.h"
#include "../../src/process/events/ProcessEvent.cpp"
#include "../../src/process/models/ProcessInfo.h"
#include "../../src/process/models/ProcessInfo.cpp"
#include "../../src/network/events/ConnectionEvent.h"
#include "../../src/network/events/ConnectionEvent.cpp"
#include "../../src/network/models/ConnectionUploadStats.h"
#include "../../src/network/models/ConnectionUploadStats.cpp"
#include "../../src/network/models/ConnectionDownloadStats.h"
#include "../../src/network/models/ConnectionDownloadStats.cpp"
#include "../../src/process/tracker/TimelineEventCollector.h"
#include "../../src/process/tracker/TimelineEventCollector.cpp"
#include "../../src/process/tracker/ProcessTimelineBuilder.h"
#include "../../src/process/tracker/ProcessTimelineBuilder.cpp"

using namespace process;
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

static std::chrono::system_clock::time_point makeUtcTime(
    int year, int month, int day, int hour, int minute, int second = 0)
{
    std::tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;

#if defined(_WIN32)
    const std::time_t value = _mkgmtime(&tm);
#else
    const std::time_t value = timegm(&tm);
#endif

    return std::chrono::system_clock::from_time_t(value);
}

static ProcessActivityEvent makeEvent(
    std::chrono::system_clock::time_point when,
    uint32_t pid,
    const std::string& name,
    ProcessActivityEventType type,
    const std::string& remote = "",
    uint64_t uploaded = 0,
    uint64_t downloaded = 0)
{
    return ProcessActivityEvent(
        when,
        pid,
        name,
        type,
        "",
        remote,
        ProtocolType::TCP,
        uploaded,
        downloaded,
        "");
}

static void testEmptyInputDoesNotCrash()
{
    ProcessTimelineBuilder builder;
    builder.build(std::vector<ProcessActivityEvent>{});

    EXPECT_TRUE(builder.getGlobalTimeline().empty());
    EXPECT_TRUE(builder.getProcessTimeline(4120).empty());
    EXPECT_EQ(builder.formatGlobalTimeline(), std::string(""));
    EXPECT_EQ(builder.formatProcessTimeline(4120), std::string(""));
}

static void testSortsEventsChronologically()
{
    ProcessTimelineBuilder builder;
    builder.build(
        {makeEvent(
             makeUtcTime(2024, 1, 15, 10, 1, 15),
             4120,
             "python.exe",
             ProcessActivityEventType::UPLOAD_ACTIVITY,
             "104.18.32.45:443",
             13107200ull),
         makeEvent(
             makeUtcTime(2024, 1, 15, 10, 1, 2),
             4120,
             "python.exe",
             ProcessActivityEventType::PROCESS_SEEN),
         makeEvent(
             makeUtcTime(2024, 1, 15, 10, 1, 10),
             4120,
             "python.exe",
             ProcessActivityEventType::CONNECTION_OPENED,
             "104.18.32.45:443")});

    const auto& global = builder.getGlobalTimeline();
    EXPECT_EQ(global.size(), 3u);
    EXPECT_TRUE(global[0].eventType == ProcessActivityEventType::PROCESS_SEEN);
    EXPECT_TRUE(global[1].eventType == ProcessActivityEventType::CONNECTION_OPENED);
    EXPECT_TRUE(global[2].eventType == ProcessActivityEventType::UPLOAD_ACTIVITY);
}

static void testPerProcessAndGlobalTimelines()
{
    ProcessTimelineBuilder builder;
    builder.build(
        {makeEvent(
             makeUtcTime(2024, 1, 15, 10, 1, 2),
             4120,
             "python.exe",
             ProcessActivityEventType::PROCESS_SEEN),
         makeEvent(
             makeUtcTime(2024, 1, 15, 10, 1, 5),
             2204,
             "chrome.exe",
             ProcessActivityEventType::PROCESS_SEEN),
         makeEvent(
             makeUtcTime(2024, 1, 15, 10, 1, 10),
             4120,
             "python.exe",
             ProcessActivityEventType::CONNECTION_OPENED,
             "104.18.32.45:443"),
         makeEvent(
             makeUtcTime(2024, 1, 15, 10, 1, 12),
             2204,
             "chrome.exe",
             ProcessActivityEventType::CONNECTION_OPENED,
             "142.250.190.78:443")});

    const auto python = builder.getProcessTimeline(4120);
    const auto chrome = builder.getProcessTimeline(2204);
    const auto& global = builder.getGlobalTimeline();

    EXPECT_EQ(python.size(), 2u);
    EXPECT_EQ(chrome.size(), 2u);
    EXPECT_EQ(global.size(), 4u);

    EXPECT_EQ(python[0].processId, 4120u);
    EXPECT_EQ(python[1].processId, 4120u);
    EXPECT_TRUE(python[0].timestamp < python[1].timestamp);

    EXPECT_EQ(chrome[0].processId, 2204u);
    EXPECT_EQ(chrome[1].processId, 2204u);

    // Global order interleaves processes by time.
    EXPECT_EQ(global[0].processId, 4120u);
    EXPECT_EQ(global[1].processId, 2204u);
    EXPECT_EQ(global[2].processId, 4120u);
    EXPECT_EQ(global[3].processId, 2204u);
}

static void testReadableFormattedOutput()
{
    ProcessTimelineBuilder builder;
    builder.build(
        {makeEvent(
             makeUtcTime(2024, 1, 15, 10, 1, 2),
             4120,
             "python.exe",
             ProcessActivityEventType::PROCESS_SEEN),
         makeEvent(
             makeUtcTime(2024, 1, 15, 10, 1, 10),
             4120,
             "python.exe",
             ProcessActivityEventType::CONNECTION_OPENED,
             "104.18.32.45:443"),
         makeEvent(
             makeUtcTime(2024, 1, 15, 10, 1, 15),
             4120,
             "python.exe",
             ProcessActivityEventType::UPLOAD_ACTIVITY,
             "104.18.32.45:443",
             13107200ull)});

    const std::string expected =
        "10:01:02 | python.exe | Process Seen\n"
        "10:01:10 | python.exe | Connected to 104.18.32.45:443\n"
        "10:01:15 | python.exe | Upload Activity: 12.5 MB";

    EXPECT_EQ(builder.formatGlobalTimeline(), expected);
    EXPECT_EQ(builder.formatProcessTimeline(4120), expected);
}

static void testBuildFromCollector()
{
    TimelineEventCollector collector;
    const auto t1 = makeUtcTime(2024, 1, 15, 10, 1, 2);
    const auto t2 = makeUtcTime(2024, 1, 15, 10, 1, 10);

    ProcessEvent created(
        ProcessEventType::CREATED,
        ProcessInfo(4120, "python.exe", 1),
        t1);

    Connection connection(
        4120,
        "192.168.1.10",
        53142,
        "104.18.32.45",
        443,
        ProtocolType::TCP,
        ConnectionState::ESTABLISHED,
        t2);

    ConnectionEvent opened(ConnectionEventType::CREATED, connection, t2);

    collector.collect(
        {created},
        {opened},
        {},
        {},
        {{4120, ProcessInfo(4120, "python.exe", 1)}});

    ProcessTimelineBuilder builder;
    builder.build(collector);

    EXPECT_EQ(builder.getGlobalTimeline().size(), 2u);
    EXPECT_TRUE(
        builder.getGlobalTimeline()[0].eventType ==
        ProcessActivityEventType::PROCESS_SEEN);
    EXPECT_TRUE(
        builder.getGlobalTimeline()[1].eventType ==
        ProcessActivityEventType::CONNECTION_OPENED);

    const std::string formatted = builder.formatProcessTimeline(4120);
    EXPECT_TRUE(formatted.find("Process Seen") != std::string::npos);
    EXPECT_TRUE(formatted.find("Connected to 104.18.32.45:443") != std::string::npos);
}

static void testDownloadAndClosedSummaries()
{
    const auto event = makeEvent(
        makeUtcTime(2024, 1, 15, 10, 2, 0),
        4120,
        "python.exe",
        ProcessActivityEventType::DOWNLOAD_ACTIVITY,
        "104.18.32.45:443",
        0,
        3355443ull);

    EXPECT_EQ(
        ProcessTimelineBuilder::formatEntry(event),
        std::string("10:02:00 | python.exe | Download Activity: 3.2 MB"));

    const auto closed = makeEvent(
        makeUtcTime(2024, 1, 15, 10, 3, 0),
        4120,
        "python.exe",
        ProcessActivityEventType::CONNECTION_CLOSED,
        "104.18.32.45:443");

    EXPECT_EQ(
        ProcessTimelineBuilder::formatEntry(closed),
        std::string("10:03:00 | python.exe | Disconnected from 104.18.32.45:443"));
}

int main()
{
    testEmptyInputDoesNotCrash();
    testSortsEventsChronologically();
    testPerProcessAndGlobalTimelines();
    testReadableFormattedOutput();
    testBuildFromCollector();
    testDownloadAndClosedSummaries();

    if (g_failures == 0)
    {
        std::cout << "All ProcessTimelineBuilder tests passed.\n";
        return 0;
    }

    std::cerr << g_failures << " test(s) failed.\n";
    return 1;
}
