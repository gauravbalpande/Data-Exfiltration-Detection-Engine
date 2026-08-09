#include <cassert>
#include <iostream>
#include <string>

#include "../../src/network/utils/NetworkUtils.h"
#include "../../src/network/utils/NetworkUtils.cpp"
#include "../../src/network/models/RemoteEndpoint.h"
#include "../../src/network/models/RemoteEndpoint.cpp"
#include "../../src/network/models/Connection.h"
#include "../../src/network/models/Connection.cpp"
#include "../../src/network/intelligence/RemoteEndpointIdentifier.h"
#include "../../src/network/intelligence/RemoteEndpointIdentifier.cpp"

// Attribution headers are needed by RemoteEndpointIdentifier; provide a
// minimal stub path by including the attribution sources as well.
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

static void testIpv4Validation()
{
    EXPECT_TRUE(NetworkUtils::isValidIpv4("104.18.32.45"));
    EXPECT_TRUE(NetworkUtils::isValidIpv4("127.0.0.1"));
    EXPECT_TRUE(NetworkUtils::isValidIpv4("0.0.0.0"));
    EXPECT_TRUE(NetworkUtils::isValidIpv4("255.255.255.255"));

    EXPECT_TRUE(!NetworkUtils::isValidIpv4(""));
    EXPECT_TRUE(!NetworkUtils::isValidIpv4("104.18.32"));
    EXPECT_TRUE(!NetworkUtils::isValidIpv4("104.18.32.45.1"));
    EXPECT_TRUE(!NetworkUtils::isValidIpv4("256.1.1.1"));
    EXPECT_TRUE(!NetworkUtils::isValidIpv4("01.2.3.4"));
    EXPECT_TRUE(!NetworkUtils::isValidIpv4("abc.def.ghi.jkl"));
}

static void testIpv6Validation()
{
    EXPECT_TRUE(NetworkUtils::isValidIpv6("::1"));
    EXPECT_TRUE(NetworkUtils::isValidIpv6("::"));
    EXPECT_TRUE(NetworkUtils::isValidIpv6("2001:db8::1"));
    EXPECT_TRUE(NetworkUtils::isValidIpv6("2001:0db8:85a3:0000:0000:8a2e:0370:7334"));
    EXPECT_TRUE(NetworkUtils::isValidIpv6("::ffff:192.0.2.1"));

    EXPECT_TRUE(!NetworkUtils::isValidIpv6(""));
    EXPECT_TRUE(!NetworkUtils::isValidIpv6("2001:db8:::1"));
    EXPECT_TRUE(!NetworkUtils::isValidIpv6("gggg::1"));
    EXPECT_TRUE(!NetworkUtils::isValidIpv6("1:2:3:4:5:6:7"));
    EXPECT_TRUE(!NetworkUtils::isValidIpv6("1:2:3:4:5:6:7:8:9"));
}

static void testAddressFamilyAndHelpers()
{
    EXPECT_EQ(NetworkUtils::detectAddressFamily("104.18.32.45"), AddressFamily::IPv4);
    EXPECT_EQ(NetworkUtils::detectAddressFamily("2001:db8::1"), AddressFamily::IPv6);
    EXPECT_EQ(NetworkUtils::detectAddressFamily("not-an-ip"), AddressFamily::UNKNOWN);

    EXPECT_EQ(NetworkUtils::addressFamilyToString(AddressFamily::IPv4), std::string("IPv4"));
    EXPECT_EQ(NetworkUtils::addressFamilyToString(AddressFamily::IPv6), std::string("IPv6"));

    EXPECT_TRUE(NetworkUtils::isLoopback("127.0.0.1"));
    EXPECT_TRUE(NetworkUtils::isLoopback("::1"));
    EXPECT_TRUE(!NetworkUtils::isLoopback("104.18.32.45"));

    EXPECT_TRUE(NetworkUtils::isUnspecified(""));
    EXPECT_TRUE(NetworkUtils::isUnspecified("0.0.0.0"));
    EXPECT_TRUE(NetworkUtils::isUnspecified("::"));
    EXPECT_TRUE(!NetworkUtils::isUnspecified("104.18.32.45"));

    EXPECT_TRUE(NetworkUtils::hasRemoteEndpoint("104.18.32.45"));
    EXPECT_TRUE(NetworkUtils::hasRemoteEndpoint("2001:db8::1"));
    EXPECT_TRUE(NetworkUtils::hasRemoteEndpoint("127.0.0.1"));
    EXPECT_TRUE(!NetworkUtils::hasRemoteEndpoint(""));
    EXPECT_TRUE(!NetworkUtils::hasRemoteEndpoint("0.0.0.0"));
    EXPECT_TRUE(!NetworkUtils::hasRemoteEndpoint("::"));
    EXPECT_TRUE(!NetworkUtils::hasRemoteEndpoint("bad"));
}

static Connection makeConnection(
    uint32_t pid,
    const std::string& localIp,
    uint16_t localPort,
    const std::string& remoteIp,
    uint16_t remotePort,
    ProtocolType protocol = ProtocolType::TCP)
{
    return Connection(
        pid,
        localIp,
        localPort,
        remoteIp,
        remotePort,
        protocol,
        ConnectionState::ESTABLISHED,
        std::chrono::system_clock::now());
}

static void testRemoteEndpointIdentifier()
{
    RemoteEndpointIdentifier identifier;

    std::vector<Connection> connections;
    connections.push_back(makeConnection(
        4120, "192.168.1.10", 51544, "104.18.32.45", 443));
    connections.push_back(makeConnection(
        4120, "fe80::1", 51545, "2001:db8::53", 443));
    connections.push_back(makeConnection(
        1000, "0.0.0.0", 80, "", 0)); // listening — no remote
    connections.push_back(makeConnection(
        1001, "::", 443, "::", 0)); // unspecified remote
    connections.push_back(makeConnection(
        1002, "192.168.1.10", 1234, "not-a-valid-ip", 443)); // malformed
    connections.push_back(makeConnection(
        1003, "127.0.0.1", 9, "127.0.0.1", 9)); // loopback remote is valid

    const std::vector<RemoteEndpoint> endpoints = identifier.identify(connections);

    EXPECT_EQ(endpoints.size(), static_cast<std::size_t>(3));

    EXPECT_EQ(endpoints[0].remoteIp, std::string("104.18.32.45"));
    EXPECT_EQ(endpoints[0].localIp, std::string("192.168.1.10"));
    EXPECT_EQ(endpoints[0].addressFamily, AddressFamily::IPv4);
    EXPECT_EQ(endpoints[0].processId, static_cast<uint32_t>(4120));
    EXPECT_EQ(endpoints[0].remotePort, static_cast<uint16_t>(443));

    EXPECT_EQ(endpoints[1].remoteIp, std::string("2001:db8::53"));
    EXPECT_EQ(endpoints[1].localIp, std::string("fe80::1"));
    EXPECT_EQ(endpoints[1].addressFamily, AddressFamily::IPv6);

    EXPECT_EQ(endpoints[2].remoteIp, std::string("127.0.0.1"));
    EXPECT_EQ(endpoints[2].addressFamily, AddressFamily::IPv4);

    const std::string formatted = endpoints[0].toString();
    EXPECT_TRUE(formatted.find("Remote IP:") != std::string::npos);
    EXPECT_TRUE(formatted.find("104.18.32.45") != std::string::npos);
    EXPECT_TRUE(formatted.find("Address Family:") != std::string::npos);
    EXPECT_TRUE(formatted.find("IPv4") != std::string::npos);
}

static void testAttributedIdentification()
{
    RemoteEndpointIdentifier identifier;

    Connection connection = makeConnection(
        4120, "10.0.0.5", 4000, "104.18.32.45", 443);

    process::ProcessInfo processInfo(
        4120, "python.exe", 1, process::ProcessStatus::RUNNING);

    AttributedConnection attributed(connection, processInfo, true, false);

    const std::vector<RemoteEndpoint> endpoints =
        identifier.identify(std::vector<AttributedConnection>{attributed});

    EXPECT_EQ(endpoints.size(), static_cast<std::size_t>(1));
    EXPECT_EQ(endpoints[0].processName, std::string("python.exe"));
    EXPECT_EQ(endpoints[0].remoteIp, std::string("104.18.32.45"));
    EXPECT_EQ(endpoints[0].addressFamily, AddressFamily::IPv4);

    const std::string formatted = endpoints[0].toString();
    EXPECT_TRUE(formatted.find("python.exe") != std::string::npos);
}

int main()
{
    testIpv4Validation();
    testIpv6Validation();
    testAddressFamilyAndHelpers();
    testRemoteEndpointIdentifier();
    testAttributedIdentification();

    if (g_failures == 0)
    {
        std::cout << "All remote endpoint tests passed.\n";
        return 0;
    }

    std::cerr << g_failures << " assertion(s) failed.\n";
    return 1;
}
