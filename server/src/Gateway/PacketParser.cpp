#include <RTypeSrv/GatewayPacketParser.hpp>
#include <array>
#include <cstring>
#include <stdexcept>

namespace rtype::srv {

/**
 * @brief Parses and validates the header of a packet from the given data buffer.
 */
std::uint8_t Gateway::PacketParser::getHeader(const uint8_t *data, std::size_t &offset, std::size_t bufsize)
{
    if (offset + 4 > bufsize) {
        throw std::runtime_error("Incomplete Header");
    }
    if (getNextVal<std::uint16_t>(data, offset, bufsize) != Gateway::HEADER_MAGIC) {
        throw std::runtime_error("Invalid magic number");
    }
    std::uint8_t ver = getNextVal<std::uint8_t>(data, offset, bufsize);
    if (ver < Gateway::MINIMUM_VERSION || ver > Gateway::MAXIMUM_VERSION) {
        throw std::runtime_error("Invalid version");
    }
    if (offset >= bufsize) {
        throw std::runtime_error("Incomplete Header (no packet id)");
    }
    return data[offset++];
}

/**
 * @brief Extracts a game ID from a byte array.
 */
uint32_t Gateway::PacketParser::extractGameId(const uint8_t *data) noexcept
{
    return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) | (static_cast<uint32_t>(data[2]) << 8)
        | (static_cast<uint32_t>(data[3]));
}

/**
 * @brief Parses a GS key from a byte array.
 */
std::pair<std::array<uint8_t, 16>, uint16_t> Gateway::PacketParser::parseGSKey(const uint8_t *data, const std::size_t offset)
{
    std::array<uint8_t, 16> ip{};
    std::memcpy(ip.data(), data + offset, 16);
    uint16_t port = static_cast<uint16_t>((data[offset + 16]) << 8 | data[offset + 17]);
    return {ip, port};
}

/**
 * @brief Parses an occupancy packet.
 *
 * New protocol format: [CMD:23][OCCUPANCY:1]
 * The game server identity is determined by the connection handle, not from packet data.
 */
uint8_t Gateway::PacketParser::parseOccupancy(const uint8_t *data, const std::size_t offset)
{
    return data[offset];
}

/**
 * @brief Parses a GID packet.
 */
std::vector<uint32_t> Gateway::PacketParser::parseGIDs(const uint8_t *data, const std::size_t start, std::size_t bufsize)
{
    std::vector<uint32_t> gids;
    for (size_t pos = start; pos + 4 <= bufsize; pos += 4) {
        uint32_t gid = extractGameId(data + pos);
        gids.push_back(gid);
    }
    return gids;
}

/**
 * @brief Builds a CREATE message.
 */
std::vector<uint8_t> Gateway::PacketParser::buildCreateMsg(uint8_t gametype)
{
    return {3, gametype};
}

/**
 * @brief Builds a JOIN message for a client.
 */
std::vector<uint8_t> Gateway::PacketParser::buildJoinMsgForClient(const uint8_t *data, std::size_t offset)
{
    return std::vector(data + offset, data + offset + 1 + 16 + 2 + 4);
}

/**
 * @brief Builds a JOIN message for a game server.
 */
std::vector<uint8_t> Gateway::PacketParser::buildJoinMsgForGS(const std::array<uint8_t, 16> &ip, const uint16_t port, const uint32_t id)
{
    std::vector<uint8_t> join_msg;
    join_msg.reserve(23);
    join_msg.push_back(static_cast<uint8_t>(2));
    join_msg.insert(join_msg.end(), ip.begin(), ip.end());
    join_msg.push_back(static_cast<uint8_t>(port >> 8));
    join_msg.push_back(static_cast<uint8_t>(port & 0xFF));
    join_msg.push_back(static_cast<uint8_t>((id >> 24) & 0xFF));
    join_msg.push_back(static_cast<uint8_t>((id >> 16) & 0xFF));
    join_msg.push_back(static_cast<uint8_t>((id >> 8) & 0xFF));
    join_msg.push_back(static_cast<uint8_t>(id & 0xFF));
    return join_msg;
}

}// namespace rtype::srv
