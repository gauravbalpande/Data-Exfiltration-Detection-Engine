#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "../../src/network/utils/NetworkUtils.h"
#include "../../src/network/utils/NetworkUtils.cpp"
#include "../../src/network/models/Connection.h"
#include "../../src/network/models/Connection.cpp"
#include "../../src/network/models/ProtocolPortProfile.h"
#include "../../src/network/models/ProtocolPortProfile.cpp"
#include "../../src/network/intelligence/ProtocolPortAnalyzer.h"
#include "../../src/network/intelligence/ProtocolPortAnalyzer.cpp"
#include "../../src/network/attribution/AttributedConnection.h"
#include "../../src/network/attribution/AttributedConnection.cpp"
#include "../../src/process/models/ProcessInfo.h"
#include "../../src/process/models/ProcessInfo.cpp"

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

static Connection makeConnection(
    ProtocolType protocol,
    uint16_t localPort,
    uint16_t remotePort,
    const std::string& localIp = "192.168.1.10",
    const std::string& remoteIp = "104.18.32.45",
    ConnectionState state = ConnectionState::ESTABLISHED)
{
    return Connection(
        4120,
        localIp,
        localPort,
        remoteIp,
        remotePort,
        protocol,
        state,
        std::chrono::system_clock::now());
}

static void testProtocolAndPortHelpers()
{
    EXPECT_EQ(NetworkUtils::protocolToString(ProtocolType::TCP), std::string("TCP"));
    EXPECT_EQ(NetworkUtils::protocolToString(ProtocolType::UDP), std::string("UDP"));
    EXPECT_EQ(NetworkUtils::protocolToString(ProtocolType::UNKNOWN), std::string("Unknown"));

    EXPECT_TRUE(NetworkUtils::isValidPort(443));
    EXPECT_TRUE(NetworkUtils::isValidPort(53142));
    EXPECT_TRUE(!NetworkUtils::isValidPort(0));

    EXPECT_TRUE(NetworkUtils::hasRemotePort(443));
    EXPECT_TRUE(!NetworkUtils::hasRemotePort(0));
}

static void testTcpAndUdpDetection()
{
    ProtocolPortAnalyzer analyzer;

    const auto tcpProfiles = analyzer.analyze(std::vector<Connection>{
        makeConnection(ProtocolType::TCP, 53142, 443),
    });
    EXPECT_EQ(tcpProfiles.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(tcpProfiles[0].protocol, ProtocolType::TCP);
    EXPECT_EQ(tcpProfiles[0].localPort, static_cast<uint16_t>(53142));
    EXPECT_EQ(tcpProfiles[0].remotePort, static_cast<uint16_t>(443));
    EXPECT_TRUE(tcpProfiles[0].hasRemotePort);

    const auto udpProfiles = analyzer.analyze(std::vector<Connection>{
        makeConnection(ProtocolType::UDP, 5353, 0, "0.0.0.0", std::string{}),
    });
    EXPECT_EQ(udpProfiles.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(udpProfiles[0].protocol, ProtocolType::UDP);
    EXPECT_EQ(udpProfiles[0].localPort, static_cast<uint16_t>(5353));
    EXPECT_TRUE(!udpProfiles[0].hasRemotePort);
    EXPECT_EQ(udpProfiles[0].remotePort, static_cast<uint16_t>(0));
}

static void testIpv4AndIpv6Support()
{
    ProtocolPortAnalyzer analyzer;

    const auto ipv4 = analyzer.analyze(std::vector<Connection>{
        makeConnection(ProtocolType::TCP, 4000, 443, "192.168.1.10", "104.18.32.45"),
    });
    EXPECT_EQ(ipv4[0].addressFamily, AddressFamily::IPv4);

    const auto ipv6 = analyzer.analyze(std::vector<Connection>{
        makeConnection(
            ProtocolType::TCP,
            51544,
            443,
            "fe80::1",
            "2001:db8::53"),
    });
    EXPECT_EQ(ipv6[0].addressFamily, AddressFamily::IPv6);
}

static void testListeningSocketHandling()
{
    ProtocolPortAnalyzer analyzer;

    const auto listening = analyzer.analyze(std::vector<Connection>{
        makeConnection(
            ProtocolType::TCP,
            8080,
            0,
            "0.0.0.0",
            "0.0.0.0",
            ConnectionState::LISTENING),
    });

    EXPECT_EQ(listening.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(listening[0].localPort, static_cast<uint16_t>(8080));
    EXPECT_TRUE(!listening[0].hasRemotePort);
    EXPECT_EQ(listening[0].connectionState, ConnectionState::LISTENING);
}

static void testInvalidRowsSkipped()
{
    ProtocolPortAnalyzer analyzer;

    const auto profiles = analyzer.analyze(std::vector<Connection>{
        makeConnection(ProtocolType::TCP, 0, 443), // invalid local port
        makeConnection(ProtocolType::UNKNOWN, 8080, 443),
    });

    EXPECT_EQ(profiles.size(), static_cast<std::size_t>(0));
}

static void testExpectedOutputFormat()
{
    ProtocolPortAnalyzer analyzer;

    const auto profiles = analyzer.analyze(std::vector<Connection>{
        makeConnection(ProtocolType::TCP, 53142, 443),
    });

    const std::string formatted = profiles[0].toString();
    EXPECT_TRUE(formatted.find("Protocol:") != std::string::npos);
    EXPECT_TRUE(formatted.find("TCP") != std::string::npos);
    EXPECT_TRUE(formatted.find("Local Port:") != std::string::npos);
    EXPECT_TRUE(formatted.find("53142") != std::string::npos);
    EXPECT_TRUE(formatted.find("Remote Port:") != std::string::npos);
    EXPECT_TRUE(formatted.find("443") != std::string::npos);
}

static void testAttributedAnalysis()
{
    ProtocolPortAnalyzer analyzer;

    Connection connection = makeConnection(ProtocolType::TCP, 53142, 443);
    process::ProcessInfo processInfo(
        4120, "python.exe", 1, process::ProcessStatus::RUNNING);
    AttributedConnection attributed(connection, processInfo, true, false);

    const auto profiles =
        analyzer.analyze(std::vector<AttributedConnection>{attributed});

    EXPECT_EQ(profiles.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(profiles[0].processName, std::string("python.exe"));
    EXPECT_EQ(profiles[0].processId, static_cast<uint32_t>(4120));
}

int main()
{
    testProtocolAndPortHelpers();
    testTcpAndUdpDetection();
    testIpv4AndIpv6Support();
    testListeningSocketHandling();
    testInvalidRowsSkipped();
    testExpectedOutputFormat();
    testAttributedAnalysis();

    if (g_failures == 0)
    {
        std::cout << "All protocol/port analysis tests passed.\n";
        return 0;
    }

    std::cerr << g_failures << " assertion(s) failed.\n";
    return 1;
}
