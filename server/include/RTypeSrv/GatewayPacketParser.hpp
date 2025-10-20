#pragma once

#include <RTypeSrv/Gateway.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace rtype::srv {

/**
 * @brief Nested class responsible for parsing and building network packets.
 *
 * This class handles all packet parsing, validation, and construction logic
 * for the Gateway server. It operates on raw byte buffers and converts them
 * to/from structured data according to the R-Type network protocol.
 */
class Gateway::PacketParser final
{
    public:
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
        static T getNextVal(const uint8_t *data, std::size_t &offset, std::size_t bufsize, const std::string &error_msg = "Invalid value");

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
        static void pushValInBuffer(uint8_t *data, std::size_t begin, const T &val);

        /**
         * @brief Parses and validates a packet header.
         *
         * @param data Pointer to the packet data.
         * @param offset Current offset in the buffer (will be advanced).
         * @param bufsize Total size of the buffer.
         * @return The packet ID byte.
         * @throws std::runtime_error If header is invalid or incomplete.
         */
        static std::uint8_t getHeader(const uint8_t *data, std::size_t &offset, std::size_t bufsize);

        /**
         * @brief Extracts a game ID from a 4-byte big-endian buffer.
         * @param data Pointer to the 4-byte buffer.
         * @return The extracted game ID.
         */
        static uint32_t extractGameId(const uint8_t *data) noexcept;

        /**
         * @brief Parses a game server key (IP + port) from a buffer.
         * @param data Pointer to the data buffer.
         * @param offset Offset to start parsing from.
         * @return Pair of IP address (16 bytes) and port number.
         */
        static std::pair<std::array<uint8_t, 16>, uint16_t> parseGSKey(const uint8_t *data, std::size_t offset);

        /**
         * @brief Parses occupancy information.
         *
         * New protocol format: [CMD:23][OCCUPANCY:1]
         * Returns only the occupancy count. Server identity is determined by handle.
         *
         * @param data Pointer to the data buffer.
         * @param offset Offset to start parsing from.
         * @return Occupancy count (number of active games).
         */
        static uint8_t parseOccupancy(const uint8_t *data, std::size_t offset);

        /**
         * @brief Parses a list of game IDs from a buffer.
         * @param data Pointer to the data buffer.
         * @param start Starting offset in the buffer.
         * @param bufsize Total size of the buffer.
         * @return Vector of extracted game IDs.
         */
        static std::vector<uint32_t> parseGIDs(const uint8_t *data, std::size_t start, std::size_t bufsize);

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
         * @brief Builds a CREATE packet message for game server.
         *
         * Format: [HEADER:5][GAMETYPE:1]
         * Total size: 6 bytes
         *
         * @param gametype The type of game to create.
         * @return Vector containing the complete CREATE packet.
         */
        static std::vector<uint8_t> buildCreateMsg(uint8_t gametype);

        /**
         * @brief Builds a JOIN message for a client.
         *
         * Format: [HEADER:5][GAME_ID:4][IP:16][PORT:2]
         * Total size: 27 bytes
         *
         * @param data Source data buffer containing game ID, IP, and port.
         * @param offset Offset in the source buffer (points to start of game ID).
         * @return Vector containing the complete JOIN packet for client.
         */
        static std::vector<uint8_t> buildJoinMsgForClient(const uint8_t *data, std::size_t offset);

        /**
         * @brief Builds a JOIN message for a game server (GW->GS).
         *
         * This is different from client JOIN - it's sent by gateway to game server
         * to inform it about a new player.
         *
         * Format: [HEADER:5][IP:16][PORT:2][GAME_ID:4]
         * Total size: 27 bytes
         *
         * @param ip Client's IPv6 address.
         * @param port Client's port number.
         * @param id Game ID the client is joining.
         * @return Vector containing the complete packet.
         */
        static std::vector<uint8_t> buildJoinMsgForGS(const std::array<uint8_t, 16> &ip, uint16_t port, uint32_t id);

        /**
         * @brief Builds a simple response packet with just command byte.
         *
         * Format: [HEADER:5]
         * Total size: 5 bytes
         *
         * Used for: GS_OK, GS_KO, CREATE_KO, JOIN_KO
         *
         * @param cmd The command/response identifier.
         * @return Vector containing the complete response packet.
         */
        static std::vector<uint8_t> buildSimpleResponse(uint8_t cmd);
};

}// namespace rtype::srv

#include <RTypeSrv/inline/PacketParser.inl>
