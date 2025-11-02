#pragma once

#include <type_traits>

namespace rtype::srv {

/**
 * @brief Extracts the next integral value of type T from a byte buffer.
 *
 * This function reads sizeof(T) bytes from the provided data buffer starting at the given offset,
 * interprets them as a big-endian integer of type T, and advances the offset accordingly.
 * If there are not enough bytes left in the buffer, it throws a std::runtime_error with the provided error message.
 *
 * @tparam T An integral type to extract from the buffer. Must satisfy std::is_integral_v<T>.
 * @param data Pointer to the byte buffer to read from.
 * @param offset Reference to the current offset in the buffer; will be incremented by sizeof(T).
 * @param bufsize Total size of the buffer.
 * @param error_msg Error message to use if the buffer does not contain enough bytes.
 * @return The extracted value of type T.
 * @throws std::runtime_error If there are not enough bytes left in the buffer to extract a value of type T.
 */
template<typename T>
    requires std::is_integral_v<T>
T Gateway::getNextVal(const uint8_t *data, std::size_t &offset, const std::size_t bufsize, const std::string &error_msg)
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

/**
 * @brief Inserts the bytes of an integral value into a buffer in big-endian order.
 *
 * This function takes an integral value of type T and writes its bytes into the
 * provided data buffer starting at the specified index. The bytes are written in
 * big-endian order (most significant byte first).
 *
 * @tparam T An integral type (e.g., uint16_t, uint32_t, etc.).
 * @param data Pointer to the buffer where the value will be inserted.
 * @param begin The starting index in the buffer to insert the value.
 * @param val The integral value to insert into the buffer.
 */
template<typename T>
    requires std::is_integral_v<T>
void Gateway::pushValInBuffer(uint8_t *data, const std::size_t begin, const T &val)
{
    std::size_t s = sizeof(T);
    for (std::size_t i = 0; i < s; ++i) {
        data[begin + i] = static_cast<uint8_t>(val >> (8 * (s - 1 - i)));
    }
}

}// namespace rtype::srv
