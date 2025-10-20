#include <RTypeSrv/GatewayPacketParser.hpp>
#include <array>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace rtype::srv {

/**
 * @brief Parses and validates the header of a packet from the given data buffer.
 *
 * Gateway Protocol Header: [MAGIC:2][VERSION:1][FLAGS:1][CMD:1]
 * Total header size: 5 bytes
 */
std::uint8_t Gateway::PacketParser::getHeader(const uint8_t *data, std::size_t &offset, std::size_t bufsize)
{
    const std::size_t start = offset;
    auto make_hex = [&](std::size_t pos, std::size_t maxlen) {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0');
        const std::size_t end = std::min(bufsize, pos + maxlen);
        for (std::size_t i = pos; i < end; ++i) {
            ss << std::setw(2) << static_cast<int>(data[i]);
            if (i + 1 < end)
                ss << ' ';
        }
        return ss.str();
    };

    if (offset + 5 > bufsize) {
        std::ostringstream msg;
        msg << "Incomplete Header (need 5 bytes, have " << (bufsize - offset) << ") - bytes: " << make_hex(offset, 32);
        throw std::runtime_error(msg.str());
    }
    if (getNextVal<std::uint16_t>(data, offset, bufsize) != Gateway::HEADER_MAGIC) {
        std::ostringstream msg;
        msg << "Invalid magic number - starting bytes: " << make_hex(start, 32);
        throw std::runtime_error(msg.str());
    }
    std::uint8_t ver = getNextVal<std::uint8_t>(data, offset, bufsize);
    if (ver < Gateway::MINIMUM_VERSION || ver > Gateway::MAXIMUM_VERSION) {
        std::ostringstream msg;
        msg << "Invalid version (got " << static_cast<int>(ver) << ") - bytes: " << make_hex(start, 32);
        throw std::runtime_error(msg.str());
    }
    std::uint8_t flags [[maybe_unused]] = getNextVal<std::uint8_t>(data, offset, bufsize);
    if (offset >= bufsize) {
        std::ostringstream msg;
        msg << "Incomplete Header (no packet id) - bytes: " << make_hex(start, 32);
        throw std::runtime_error(msg.str());
    }
    return data[offset];
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
 * @brief Builds a complete gateway protocol packet header.
 *
 * Header format: [MAGIC:2][VERSION:1][FLAGS:1][CMD:1]
 * Total size: 5 bytes
 */
std::vector<uint8_t> Gateway::PacketParser::buildHeader(uint8_t cmd, uint8_t flags)
{
    std::vector<uint8_t> header;
    header.reserve(5);
    header.push_back(0x42);
    header.push_back(0x57);
    header.push_back(0x01);
    header.push_back(flags);
    header.push_back(cmd);
    return header;
}

/**
 * @brief Builds a CREATE message for game server.
 *
 * Format: [HEADER:5][GAMETYPE:1]
 * Total size: 6 bytes
 */
std::vector<uint8_t> Gateway::PacketParser::buildCreateMsg(uint8_t gametype)
{
    std::vector<uint8_t> msg = buildHeader(3);// CMD_CREATE = 3
    msg.push_back(gametype);
    return msg;
}

/**
 * @brief Builds a JOIN message for a client.
 *
 * Format: [HEADER:5][GAME_ID:4][IP:16][PORT:2]
 * Total size: 27 bytes
 */
std::vector<uint8_t> Gateway::PacketParser::buildJoinMsgForClient(const uint8_t *data, std::size_t offset)
{
    std::vector<uint8_t> msg = buildHeader(1);// CMD_JOIN = 1
    msg.insert(msg.end(), data + offset, data + offset + 4 + 16 + 2);
    return msg;
}

/**
 * @brief Builds a JOIN message for a game server.
 *
 * This message is sent from Gateway to Game Server to notify about a new player.
 * Note: The format here may differ from spec - need clarification on what GW sends to GS for CREATE response.
 *
 * Format: [HEADER:5][IP:16][PORT:2][GAME_ID:4]
 * Total size: 27 bytes
 */
std::vector<uint8_t> Gateway::PacketParser::buildJoinMsgForGS(const std::array<uint8_t, 16> &ip, const uint16_t port, const uint32_t id)
{
    std::vector<uint8_t> msg = buildHeader(1);// CMD_JOIN = 1
    msg.reserve(27);
    msg.insert(msg.end(), ip.begin(), ip.end());
    msg.push_back(static_cast<uint8_t>(port >> 8));
    msg.push_back(static_cast<uint8_t>(port & 0xFF));
    msg.push_back(static_cast<uint8_t>((id >> 24) & 0xFF));
    msg.push_back(static_cast<uint8_t>((id >> 16) & 0xFF));
    msg.push_back(static_cast<uint8_t>((id >> 8) & 0xFF));
    msg.push_back(static_cast<uint8_t>(id & 0xFF));
    return msg;
}

/**
 * @brief Builds a simple response packet.
 *
 * Format: [HEADER:5]
 * Total size: 5 bytes
 *
 * Used for: GS_OK (21), GS_KO (22), CREATE_KO (4), JOIN_KO (2)
 */
std::vector<uint8_t> Gateway::PacketParser::buildSimpleResponse(uint8_t cmd)
{
    return buildHeader(cmd);
}

}// namespace rtype::srv
