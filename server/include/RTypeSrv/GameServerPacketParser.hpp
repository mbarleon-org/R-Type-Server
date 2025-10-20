#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace rtype::srv {

/**
 * @brief Helper class for building and parsing Gateway Protocol (TCP) packets for Game Server.
 *
 * This class provides static methods to build and parse packets according to the
 * R-Type Gateway Protocol specification.
 */
class GameServerPacketParser final
{
    public:
        /**
         * @brief Extracts the next integral value of type T from a byte buffer (big-endian).
         *
         * @tparam T An integral type to extract from the buffer.
         * @param data Pointer to the byte buffer to read from.
         * @param offset Reference to the current offset in the buffer; will be incremented by sizeof(T).
         * @param bufsize Total size of the buffer.
         * @param error_msg Error message to use if the buffer does not contain enough bytes.
         * @return The extracted value of type T.
         * @throws std::runtime_error If there are not enough bytes left in the buffer.
         */
        template<typename T>
            requires std::is_integral_v<T>
        static T getNextVal(const uint8_t *data, std::size_t &offset, std::size_t bufsize, const std::string &error_msg = "Invalid value")
        {
            constexpr std::size_t s = sizeof(T);
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
         * @tparam T An integral type (e.g., uint16_t, uint32_t, etc.).
         * @param data Pointer to the buffer where the value will be inserted.
         * @param begin The starting index in the buffer to insert the value.
         * @param val The integral value to insert into the buffer.
         */
        template<typename T>
            requires std::is_integral_v<T>
        static void pushValInBuffer(uint8_t *data, std::size_t begin, const T &val)
        {
            constexpr std::size_t s = sizeof(T);
            for (std::size_t i = 0; i < s; ++i) {
                data[begin + i] = static_cast<uint8_t>(val >> (8 * (s - 1 - i)));
            }
        }

        /**
         * @brief Parses and validates a Gateway Protocol packet header.
         *
         * Header format: [MAGIC:2][VERSION:1][FLAGS:1][CMD:1]
         * Total size: 5 bytes
         *
         * @param data Pointer to the packet data.
         * @param offset Current offset in the buffer (will be advanced by 5 bytes).
         * @param bufsize Total size of the buffer.
         * @return The CMD byte.
         * @throws std::runtime_error If header is invalid or incomplete.
         */
        static std::uint8_t parseHeader(const uint8_t *data, std::size_t &offset, std::size_t bufsize);

        /**
         * @brief Builds a complete gateway protocol packet header.
         *
         * Creates the standard header: [MAGIC:2][VERSION:1][FLAGS:1][CMD:1]
         * Total size: 5 bytes
         *
         * @param cmd The command identifier.
         * @param flags Optional flags byte (default: 0).
         * @return Vector containing the 5-byte header.
         */
        static std::vector<uint8_t> buildHeader(uint8_t cmd, uint8_t flags = 0);

        /**
         * @brief Builds a GS registration packet.
         *
         * Format: [HEADER:5][CMD:20][IP:16][PORT:2]
         * Total size: 23 bytes
         *
         * @param ip Server's IPv6 address (or IPv4-mapped IPv6).
         * @param port Server's UDP port (host byte order, will be converted to big-endian).
         * @return Vector containing the complete registration packet.
         */
        static std::vector<uint8_t> buildGSRegistration(const std::array<uint8_t, 16> &ip, uint16_t port);

        /**
         * @brief Builds an OCCUPANCY packet.
         *
         * Format: [HEADER:5][CMD:23][OCCUPANCY:1]
         * Total size: 6 bytes
         *
         * @param occupancy Number of active games on this server.
         * @return Vector containing the complete OCCUPANCY packet.
         */
        static std::vector<uint8_t> buildOccupancy(uint8_t occupancy);

        /**
         * @brief Builds a JOIN response packet for gateway.
         *
         * Format: [HEADER:5][CMD:1][GAME_ID:4][IP:16][PORT:2]
         * Total size: 27 bytes
         *
         * @param game_id The ID of the created game.
         * @param ip Server's IPv6 address for clients to connect to.
         * @param port Server's UDP port for clients to connect to.
         * @return Vector containing the complete JOIN packet.
         */
        static std::vector<uint8_t> buildJoinResponse(uint32_t game_id, const std::array<uint8_t, 16> &ip, uint16_t port);

        /**
         * @brief Builds a CREATE_KO error response.
         *
         * Format: [HEADER:5][CMD:4]
         * Total size: 5 bytes
         *
         * @return Vector containing the CREATE_KO packet.
         */
        static std::vector<uint8_t> buildCreateKO();

        /**
         * @brief Builds a GAME_END notification packet.
         *
         * Format: [HEADER:5][CMD:5][GAME_ID:4]
         * Total size: 9 bytes
         *
         * @param game_id The ID of the game that ended.
         * @return Vector containing the complete GAME_END packet.
         */
        static std::vector<uint8_t> buildGameEnd(uint32_t game_id);

        /**
         * @brief Builds a GID registration packet.
         *
         * Format: [HEADER:5][CMD:24][LEN:1][GAME_ID:4]...
         * Total size: 6 + (LEN * 4) bytes
         *
         * @param game_ids Vector of game IDs this server is hosting.
         * @return Vector containing the complete GID packet.
         */
        static std::vector<uint8_t> buildGIDRegistration(const std::vector<uint32_t> &game_ids);

        // Constants
        static constexpr uint16_t HEADER_MAGIC = 0x4257;///< Gateway protocol magic number
        static constexpr uint8_t VERSION = 0x01;        ///< Protocol version
};

}// namespace rtype::srv
