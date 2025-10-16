#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace rtype::srv {

template<typename T>
    requires std::is_integral_v<T>
T Gateway::PacketParser::getNextVal(const uint8_t *data, std::size_t &offset, const std::size_t bufsize, const std::string &error_msg)
{
    std::size_t s = sizeof(T);
    if (offset + s > bufsize) {
        throw std::runtime_error(error_msg);
    }
    T val = 0;
    for (std::size_t i = 0; i < s; ++i) {
        val = static_cast<T>((val << 8) | data[offset + i]);
    }
    offset += s;
    return val;
}

template<typename T>
    requires std::is_integral_v<T>
void Gateway::PacketParser::pushValInBuffer(uint8_t *data, const std::size_t begin, const T &val)
{
    std::size_t s = sizeof(T);
    for (std::size_t i = 0; i < s; ++i) {
        data[begin + i] = static_cast<uint8_t>(val >> (8 * (s - 1 - i)));
    }
}

}// namespace rtype::srv
