#include <RTypeSrv/GameServerPacketParser.hpp>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace rtype::srv {

/**
 * @brief Parses and validates the header of a Gateway Protocol packet.
 */
std::uint8_t GameServerPacketParser::parseHeader(const uint8_t *data, std::size_t &offset, std::size_t bufsize)
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
    if (getNextVal<std::uint16_t>(data, offset, bufsize) != HEADER_MAGIC) {
        std::ostringstream msg;
        msg << "Invalid magic number - starting bytes: " << make_hex(start, 32);
        throw std::runtime_error(msg.str());
    }
    std::uint8_t ver = getNextVal<std::uint8_t>(data, offset, bufsize);
    if (ver != VERSION) {
        std::ostringstream msg;
        msg << "Invalid version (got " << static_cast<int>(ver) << ") - bytes: " << make_hex(start, 32);
        throw std::runtime_error(msg.str());
    }
    [[maybe_unused]] std::uint8_t flags = getNextVal<std::uint8_t>(data, offset, bufsize);

    if (offset >= bufsize) {
        std::ostringstream msg;
        msg << "Incomplete Header (no CMD byte) - bytes: " << make_hex(start, 32);
        throw std::runtime_error(msg.str());
    }
    return data[offset];
}

/**
 * @brief Builds a complete gateway protocol packet header.
 */
std::vector<uint8_t> GameServerPacketParser::buildHeader(uint8_t cmd, uint8_t flags)
{
    std::vector<uint8_t> header;
    header.reserve(5);
    header.push_back(0x42);
    header.push_back(0x57);
    header.push_back(VERSION);
    header.push_back(flags);
    header.push_back(cmd);
    return header;
}

/**
 * @brief Builds a GS registration packet.
 */
std::vector<uint8_t> GameServerPacketParser::buildGSRegistration(const std::array<uint8_t, 16> &ip, uint16_t port)
{
    std::vector<uint8_t> packet = buildHeader(20);

    packet.reserve(23);
    packet.insert(packet.end(), ip.begin(), ip.end());
    packet.push_back(static_cast<uint8_t>(port >> 8));
    packet.push_back(static_cast<uint8_t>(port & 0xFF));
    return packet;
}

/**
 * @brief Builds an OCCUPANCY packet.
 */
std::vector<uint8_t> GameServerPacketParser::buildOccupancy(uint8_t occupancy)
{
    std::vector<uint8_t> packet = buildHeader(23);
    packet.push_back(occupancy);
    return packet;
}

/**
 * @brief Builds a JOIN response packet.
 */
std::vector<uint8_t> GameServerPacketParser::buildJoinResponse(uint32_t game_id, const std::array<uint8_t, 16> &ip, uint16_t port)
{
    std::vector<uint8_t> packet = buildHeader(1);

    packet.reserve(27);
    packet.push_back(static_cast<uint8_t>((game_id >> 24) & 0xFF));
    packet.push_back(static_cast<uint8_t>((game_id >> 16) & 0xFF));
    packet.push_back(static_cast<uint8_t>((game_id >> 8) & 0xFF));
    packet.push_back(static_cast<uint8_t>(game_id & 0xFF));
    packet.insert(packet.end(), ip.begin(), ip.end());
    packet.push_back(static_cast<uint8_t>(port >> 8));
    packet.push_back(static_cast<uint8_t>(port & 0xFF));
    return packet;
}

/**
 * @brief Builds a CREATE_KO error response.
 */
std::vector<uint8_t> GameServerPacketParser::buildCreateKO()
{
    return buildHeader(4);
}

/**
 * @brief Builds a GAME_END notification packet.
 */
std::vector<uint8_t> GameServerPacketParser::buildGameEnd(uint32_t game_id)
{
    std::vector<uint8_t> packet = buildHeader(5);

    packet.push_back(static_cast<uint8_t>((game_id >> 24) & 0xFF));
    packet.push_back(static_cast<uint8_t>((game_id >> 16) & 0xFF));
    packet.push_back(static_cast<uint8_t>((game_id >> 8) & 0xFF));
    packet.push_back(static_cast<uint8_t>(game_id & 0xFF));
    return packet;
}

/**
 * @brief Builds a GID registration packet.
 */
std::vector<uint8_t> GameServerPacketParser::buildGIDRegistration(const std::vector<uint32_t> &game_ids)
{
    std::vector<uint8_t> packet = buildHeader(24);

    packet.push_back(static_cast<uint8_t>(game_ids.size()));
    for (uint32_t game_id : game_ids) {
        packet.push_back(static_cast<uint8_t>((game_id >> 24) & 0xFF));
        packet.push_back(static_cast<uint8_t>((game_id >> 16) & 0xFF));
        packet.push_back(static_cast<uint8_t>((game_id >> 8) & 0xFF));
        packet.push_back(static_cast<uint8_t>(game_id & 0xFF));
    }
    return packet;
}

}// namespace rtype::srv
