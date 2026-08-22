#include "NetworkUtils.h"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace network
{

namespace
{
bool isHexDigit(char ch)
{
    return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
}

bool parseDecimalOctet(const std::string& token, int& value)
{
    if (token.empty() || token.size() > 3)
    {
        return false;
    }

    if (token.size() > 1 && token[0] == '0')
    {
        return false;
    }

    int parsed = 0;
    for (char ch : token)
    {
        if (!std::isdigit(static_cast<unsigned char>(ch)))
        {
            return false;
        }
        parsed = (parsed * 10) + (ch - '0');
    }

    if (parsed < 0 || parsed > 255)
    {
        return false;
    }

    value = parsed;
    return true;
}

bool parseHextet(const std::string& token)
{
    if (token.empty() || token.size() > 4)
    {
        return false;
    }

    for (char ch : token)
    {
        if (!isHexDigit(ch))
        {
            return false;
        }
    }
    return true;
}

std::vector<std::string> split(const std::string& value, char delimiter)
{
    std::vector<std::string> parts;
    std::string current;
    for (char ch : value)
    {
        if (ch == delimiter)
        {
            parts.push_back(current);
            current.clear();
        }
        else
        {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}
} // namespace

bool NetworkUtils::isValidIpv4(const std::string& address)
{
    const std::vector<std::string> parts = split(address, '.');
    if (parts.size() != 4)
    {
        return false;
    }

    for (const std::string& part : parts)
    {
        int octet = 0;
        if (!parseDecimalOctet(part, octet))
        {
            return false;
        }
    }

    return true;
}

bool NetworkUtils::isValidIpv6(const std::string& address)
{
    if (address.empty() || address.find(":::") != std::string::npos)
    {
        return false;
    }

    for (char ch : address)
    {
        if (!(isHexDigit(ch) || ch == ':' || ch == '.'))
        {
            return false;
        }
    }

    std::string working = address;

    const std::size_t lastColon = working.find_last_of(':');
    const std::size_t firstDot = working.find('.');
    if (firstDot != std::string::npos)
    {
        if (lastColon == std::string::npos || lastColon > firstDot)
        {
            return false;
        }

        const std::string ipv4Tail = working.substr(lastColon + 1);
        if (!isValidIpv4(ipv4Tail))
        {
            return false;
        }

        // Replace IPv4-mapped tail with two placeholder hextets.
        working = working.substr(0, lastColon + 1) + "0:0";
    }

    const std::size_t compressionPos = working.find("::");
    const bool hasCompression = compressionPos != std::string::npos;
    if (hasCompression && working.find("::", compressionPos + 1) != std::string::npos)
    {
        return false;
    }

    std::vector<std::string> left;
    std::vector<std::string> right;

    if (hasCompression)
    {
        const std::string leftPart = working.substr(0, compressionPos);
        const std::string rightPart = working.substr(compressionPos + 2);

        if (!leftPart.empty())
        {
            left = split(leftPart, ':');
        }
        if (!rightPart.empty())
        {
            right = split(rightPart, ':');
        }
    }
    else
    {
        left = split(working, ':');
    }

    auto validateParts = [](const std::vector<std::string>& parts) -> bool {
        for (const std::string& part : parts)
        {
            if (!parseHextet(part))
            {
                return false;
            }
        }
        return true;
    };

    if (!validateParts(left) || !validateParts(right))
    {
        return false;
    }

    const int hextetCount =
        static_cast<int>(left.size() + right.size());

    if (hasCompression)
    {
        // "::" must stand in for at least one missing hextet.
        return hextetCount < 8;
    }

    return hextetCount == 8;
}

bool NetworkUtils::isValidIpAddress(const std::string& address)
{
    return isValidIpv4(address) || isValidIpv6(address);
}

AddressFamily NetworkUtils::detectAddressFamily(const std::string& address)
{
    if (isValidIpv4(address))
    {
        return AddressFamily::IPv4;
    }
    if (isValidIpv6(address))
    {
        return AddressFamily::IPv6;
    }
    return AddressFamily::UNKNOWN;
}

std::string NetworkUtils::addressFamilyToString(AddressFamily family)
{
    switch (family)
    {
    case AddressFamily::IPv4:
        return "IPv4";
    case AddressFamily::IPv6:
        return "IPv6";
    default:
        return "Unknown";
    }
}

bool NetworkUtils::isLoopback(const std::string& address)
{
    if (isValidIpv4(address))
    {
        return address.rfind("127.", 0) == 0;
    }

    if (isValidIpv6(address))
    {
        return address == "::1" || address == "0:0:0:0:0:0:0:1";
    }

    return false;
}

bool NetworkUtils::isUnspecified(const std::string& address)
{
    if (address.empty())
    {
        return true;
    }

    if (address == "0.0.0.0")
    {
        return true;
    }

    if (address == "::" || address == "0:0:0:0:0:0:0:0")
    {
        return true;
    }

    return false;
}

bool NetworkUtils::hasRemoteEndpoint(const std::string& address)
{
    if (isUnspecified(address))
    {
        return false;
    }

    return isValidIpAddress(address);
}

std::string NetworkUtils::protocolToString(ProtocolType protocol)
{
    switch (protocol)
    {
    case ProtocolType::TCP:
        return "TCP";
    case ProtocolType::UDP:
        return "UDP";
    default:
        return "Unknown";
    }
}

bool NetworkUtils::isValidPort(uint16_t port)
{
    return port > 0;
}

bool NetworkUtils::hasRemotePort(uint16_t port)
{
    return isValidPort(port);
}

std::string NetworkUtils::formatBytes(uint64_t bytes)
{
    if (bytes < 1024)
    {
        return std::to_string(bytes) + " B";
    }

    static const char* units[] = {"KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes) / 1024.0;
    int unit = 0;

    while (value >= 1024.0 && unit < 3)
    {
        value /= 1024.0;
        ++unit;
    }

    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(1);
    out << value << " " << units[unit];
    return out.str();
}

} // namespace network
