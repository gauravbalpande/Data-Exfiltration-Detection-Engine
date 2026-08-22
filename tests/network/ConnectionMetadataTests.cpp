#include <chrono>
#include <iostream>
#include <string>

#include "../../src/network/utils/NetworkUtils.h"
#include "../../src/network/utils/NetworkUtils.cpp"
#include "../../src/network/models/Connection.h"
#include "../../src/network/models/Connection.cpp"
#include "../../src/network/models/ConnectionMetadata.h"
#include "../../src/network/models/ConnectionMetadata.cpp"

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
                      << __LINE__ << "\n";                                     \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)

static void testStoresEndpointFields()
{
    const auto now = std::chrono::system_clock::now();
    ConnectionMetadata meta(
        4120,
        "python.exe",
        "104.18.32.45",
        "api.example.com",
        ProtocolType::TCP,
        53142,
        443,
        AddressFamily::IPv4,
        true,
        now);

    EXPECT_EQ(meta.processId, 4120u);
    EXPECT_EQ(meta.processName, std::string("python.exe"));
    EXPECT_EQ(meta.remoteIp, std::string("104.18.32.45"));
    EXPECT_EQ(meta.domain, std::string("api.example.com"));
    EXPECT_TRUE(meta.protocol == ProtocolType::TCP);
    EXPECT_EQ(meta.localPort, 53142);
    EXPECT_EQ(meta.remotePort, 443);
    EXPECT_TRUE(meta.addressFamily == AddressFamily::IPv4);
    EXPECT_TRUE(meta.hasRemotePort);
}

static void testFromConnection()
{
    Connection connection(
        4120,
        "192.168.1.10",
        53142,
        "104.18.32.45",
        443,
        ProtocolType::TCP,
        ConnectionState::ESTABLISHED,
        std::chrono::system_clock::now());

    ConnectionMetadata meta = ConnectionMetadata::fromConnection(
        connection, "api.example.com", "python.exe");

    EXPECT_EQ(meta.processId, 4120u);
    EXPECT_EQ(meta.processName, std::string("python.exe"));
    EXPECT_EQ(meta.remoteIp, std::string("104.18.32.45"));
    EXPECT_EQ(meta.domain, std::string("api.example.com"));
    EXPECT_TRUE(meta.protocol == ProtocolType::TCP);
    EXPECT_EQ(meta.localPort, 53142);
    EXPECT_EQ(meta.remotePort, 443);
    EXPECT_TRUE(meta.addressFamily == AddressFamily::IPv4);
    EXPECT_TRUE(meta.hasRemotePort);
}

static void testFromConnectionIpv6AndUdp()
{
    Connection connection(
        1000,
        "fe80::1",
        5353,
        "2001:db8::1",
        53,
        ProtocolType::UDP,
        ConnectionState::ESTABLISHED,
        std::chrono::system_clock::now());

    ConnectionMetadata meta =
        ConnectionMetadata::fromConnection(connection, "dns.example.com");

    EXPECT_EQ(meta.remoteIp, std::string("2001:db8::1"));
    EXPECT_EQ(meta.domain, std::string("dns.example.com"));
    EXPECT_TRUE(meta.protocol == ProtocolType::UDP);
    EXPECT_EQ(meta.localPort, 5353);
    EXPECT_EQ(meta.remotePort, 53);
    EXPECT_TRUE(meta.addressFamily == AddressFamily::IPv6);
    EXPECT_TRUE(meta.hasRemotePort);
}

static void testListeningSocketOmitsRemotePort()
{
    Connection connection(
        4120,
        "0.0.0.0",
        8080,
        "0.0.0.0",
        0,
        ProtocolType::TCP,
        ConnectionState::LISTENING,
        std::chrono::system_clock::now());

    ConnectionMetadata meta = ConnectionMetadata::fromConnection(connection);

    EXPECT_EQ(meta.localPort, 8080);
    EXPECT_EQ(meta.remotePort, 0);
    EXPECT_TRUE(!meta.hasRemotePort);

    const std::string formatted = meta.toString();
    EXPECT_TRUE(formatted.find("Remote Port:") == std::string::npos);
}

static void testFormattedOutput()
{
    ConnectionMetadata meta(
        4120,
        "python.exe",
        "104.18.32.45",
        "api.example.com",
        ProtocolType::TCP,
        53142,
        443,
        AddressFamily::IPv4,
        true,
        std::chrono::system_clock::now());

    const std::string expected =
        "Remote IP:\n"
        "104.18.32.45\n"
        "\n"
        "Domain:\n"
        "api.example.com\n"
        "\n"
        "Protocol:\n"
        "TCP\n"
        "\n"
        "Remote Port:\n"
        "443";

    EXPECT_EQ(meta.toString(), expected);
}

int main()
{
    testStoresEndpointFields();
    testFromConnection();
    testFromConnectionIpv6AndUdp();
    testListeningSocketOmitsRemotePort();
    testFormattedOutput();

    if (g_failures == 0)
    {
        std::cout << "All ConnectionMetadata tests passed.\n";
        return 0;
    }

    std::cerr << g_failures << " test(s) failed.\n";
    return 1;
}
