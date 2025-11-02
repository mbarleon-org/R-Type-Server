#include <RTypeSrv/Utils/IPToStr.hpp>
#include <algorithm>
#include <sstream>

/**
 * @brief Converts an IP address to a string.
 *
 * @param ip The IP address to convert.
 * @return The string representation of the IP address.
 */
std::string rtype::srv::utils::ipToStr(const std::array<uint8_t, 16> &ip) noexcept
{
    bool v4mapped = std::all_of(ip.begin(), ip.begin() + 10, [](uint8_t x) { return x == 0; }) && ip[10] == 0xff && ip[11] == 0xff;
    bool all_zero = std::ranges::all_of(ip, [](const uint8_t x) { return x == 0; });

    if (bool v4compat = !all_zero && std::all_of(ip.begin(), ip.begin() + 12, [](const uint8_t x) { return x == 0; });
        v4mapped || v4compat) {
        std::ostringstream os;
        os << static_cast<unsigned>(ip[12]) << '.' << static_cast<unsigned>(ip[13]) << '.' << static_cast<unsigned>(ip[14]) << '.'
           << static_cast<unsigned>(ip[15]);
        return os.str();
    }

    uint16_t w[8];
    for (uint8_t i = 0; i < 8; ++i) {
        w[i] = (static_cast<uint16_t>(ip[2 * i] << 8) | ip[static_cast<uint16_t>(2 * i + 1)]);
    }

    int bestStart = -1, bestLen = 0;
    for (int i = 0; i < 8;) {
        if (w[i] != 0) {
            ++i;
            continue;
        }
        int j = i;
        while (j < 8 && w[j] == 0)
            ++j;
        if (int len = j - i; len > bestLen && len >= 2) {
            bestStart = i;
            bestLen = len;
        }
        i = j;
    }

    std::ostringstream os;
    os << std::hex;
    for (int i = 0; i < 8;) {
        if (i == bestStart) {
            if (i == 0)
                os << "::";
            else
                os << ':';
            i += bestLen;
            if (i >= 8)
                break;
        } else {
            if (i != 0 && i != bestStart + bestLen)
                os << ':';
            os << std::nouppercase << std::hex << static_cast<unsigned>(w[i]);
            ++i;
        }
    }

    std::string out = os.str();
    if (out.empty())
        out = "::";
    return out;
}
