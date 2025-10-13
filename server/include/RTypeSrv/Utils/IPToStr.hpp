#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace rtype::srv::utils {

/**
 * @brief Converts an IP address to a string.
 *
 * @param ip The IP address to convert.
 * @return The string representation of the IP address.
 */
std::string ipToStr(const std::array<uint8_t, 16> &ip) noexcept;

}
