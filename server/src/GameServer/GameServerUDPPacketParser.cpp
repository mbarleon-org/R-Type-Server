#include <RTypeSrv/GameServerUDPPacketParser.hpp>
#include <RTypeSrv/Utils/Logger.hpp>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>

std::vector<uint8_t> rtype::srv::GameServerUDPPacketParser::buildAuthOkPacket(uint32_t seq, uint32_t ackBase, uint8_t ackBits, uint32_t clientId,
    const std::array<uint8_t, 8> &sessionKey)
{
    const uint16_t total_size = static_cast<uint16_t>(HEADER_SIZE + 4 + 8);
    std::vector<uint8_t> packet =
        buildHeader(GSPcol::CMD::AUTH_OK, GSPcol::FLAGS::RELIABLE, seq, ackBase, ackBits, GSPcol::CHANNEL::RO, total_size, clientId);
    packet.push_back(static_cast<uint8_t>((clientId >> 24) & 0xFF));
    packet.push_back(static_cast<uint8_t>((clientId >> 16) & 0xFF));
    packet.push_back(static_cast<uint8_t>((clientId >> 8) & 0xFF));
    packet.push_back(static_cast<uint8_t>(clientId & 0xFF));
    packet.insert(packet.end(), sessionKey.begin(), sessionKey.end());
    return packet;
}

namespace rtype::srv {

std::uint8_t GameServerUDPPacketParser::parseHeader(const uint8_t *data, std::size_t &offset, std::size_t bufsize)
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
    if (offset + HEADER_SIZE > bufsize) {
        std::ostringstream msg;
        msg << "Incomplete UDP header (need " << HEADER_SIZE << " bytes, have " << (bufsize - offset)
            << ") - bytes: " << make_hex(offset, 32);
        throw std::runtime_error(msg.str());
    }
    uint16_t magic = static_cast<uint16_t>((static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1]);
    if (magic != HEADER_MAGIC) {
        std::ostringstream msg;
        msg << "Invalid UDP magic number (got 0x" << std::hex << magic << ", expected 0x" << HEADER_MAGIC
            << ") - bytes: " << make_hex(start, 32);
        throw std::runtime_error(msg.str());
    }
    offset += 2;
    uint8_t ver = data[offset++];
    if (ver != VERSION) {
        std::ostringstream msg;
        msg << "Invalid UDP protocol version (got " << static_cast<int>(ver) << ") - bytes: " << make_hex(start, 32);
        throw std::runtime_error(msg.str());
    }
    offset += 17;
    uint8_t cmd = data[offset++];
    return cmd;
}

std::vector<uint8_t> GameServerUDPPacketParser::buildHeader(GSPcol::CMD cmd, GSPcol::FLAGS flags, uint32_t seq, uint32_t ackBase,
    uint8_t ackBits, GSPcol::CHANNEL channel, uint16_t size, uint32_t clientId)
{
    std::vector<uint8_t> header;
    header.reserve(HEADER_SIZE);
    header.push_back(static_cast<uint8_t>(HEADER_MAGIC >> 8));
    header.push_back(static_cast<uint8_t>(HEADER_MAGIC & 0xFF));
    header.push_back(VERSION);
    header.push_back(static_cast<uint8_t>(flags));
    header.push_back(static_cast<uint8_t>((seq >> 24) & 0xFF));
    header.push_back(static_cast<uint8_t>((seq >> 16) & 0xFF));
    header.push_back(static_cast<uint8_t>((seq >> 8) & 0xFF));
    header.push_back(static_cast<uint8_t>(seq & 0xFF));
    header.push_back(static_cast<uint8_t>((ackBase >> 24) & 0xFF));
    header.push_back(static_cast<uint8_t>((ackBase >> 16) & 0xFF));
    header.push_back(static_cast<uint8_t>((ackBase >> 8) & 0xFF));
    header.push_back(static_cast<uint8_t>(ackBase & 0xFF));
    header.push_back(ackBits);
    header.push_back(static_cast<uint8_t>(channel));
    header.push_back(static_cast<uint8_t>(size >> 8));
    header.push_back(static_cast<uint8_t>(size & 0xFF));
    header.push_back(static_cast<uint8_t>((clientId >> 24) & 0xFF));
    header.push_back(static_cast<uint8_t>((clientId >> 16) & 0xFF));
    header.push_back(static_cast<uint8_t>((clientId >> 8) & 0xFF));
    header.push_back(static_cast<uint8_t>(clientId & 0xFF));
    header.push_back(static_cast<uint8_t>(cmd));
    return header;
}

std::vector<uint8_t> GameServerUDPPacketParser::buildPongResponse(uint32_t seq, uint32_t ackBase, uint8_t ackBits, uint32_t clientId)
{
    return buildHeader(GSPcol::CMD::PONG, GSPcol::FLAGS::CONN, seq, ackBase, ackBits, GSPcol::CHANNEL::UU, HEADER_SIZE, clientId);
}

std::vector<uint8_t> GameServerUDPPacketParser::buildSnapshot(uint32_t seq, uint32_t ackBase, uint8_t ackBits, uint32_t clientId,
    uint32_t snapshotSeq, const std::vector<uint8_t> &stateData)
{
    if (stateData.size() > MAX_PAYLOAD_SIZE - 4) {
        std::vector<std::vector<uint8_t>> fragments;
        size_t offset = 0;
        const size_t fragment_size = MAX_PAYLOAD_SIZE - 16;

        while (offset < stateData.size()) {
            size_t chunk_size = std::min(fragment_size, stateData.size() - offset);
            using diff_t = std::vector<uint8_t>::difference_type;
            std::vector<uint8_t> fragment_data(stateData.begin() + static_cast<diff_t>(offset),
                stateData.begin() + static_cast<diff_t>(offset + chunk_size));
            fragments.push_back(buildFragment(static_cast<uint32_t>(seq + fragments.size()), ackBase, ackBits, clientId, seq,
                static_cast<uint32_t>(stateData.size() + 4), static_cast<uint32_t>(offset), fragment_data));
            offset += chunk_size;
        }
        return fragments[0];
    }

    const uint16_t total_size = static_cast<uint16_t>(HEADER_SIZE + 4 + stateData.size());

    std::vector<uint8_t> packet =
        buildHeader(GSPcol::CMD::SNAPSHOT, GSPcol::FLAGS::RELIABLE, seq, ackBase, ackBits, GSPcol::CHANNEL::RO, total_size, clientId);
    packet.push_back(static_cast<uint8_t>((snapshotSeq >> 24) & 0xFF));
    packet.push_back(static_cast<uint8_t>((snapshotSeq >> 16) & 0xFF));
    packet.push_back(static_cast<uint8_t>((snapshotSeq >> 8) & 0xFF));
    packet.push_back(static_cast<uint8_t>(snapshotSeq & 0xFF));
    packet.insert(packet.end(), stateData.begin(), stateData.end());
    return packet;
}

std::vector<uint8_t> GameServerUDPPacketParser::buildChallenge(uint32_t seq, uint32_t ackBase, uint8_t ackBits, uint32_t clientId,
    const std::array<uint8_t, 32> &challenge)
{
    auto packet = buildHeader(GSPcol::CMD::CHALLENGE, GSPcol::FLAGS::RELIABLE, seq, ackBase, ackBits, GSPcol::CHANNEL::RO, HEADER_SIZE + 32,
        clientId);
    packet.insert(packet.end(), challenge.begin(), challenge.end());
    return packet;
}

std::vector<uint8_t> GameServerUDPPacketParser::buildFragment(uint32_t seq, uint32_t ackBase, uint8_t ackBits, uint32_t clientId,
    uint32_t baseSeq, uint32_t totalSize, uint32_t offset, const std::vector<uint8_t> &fragmentData)
{
    if (fragmentData.size() > MAX_PAYLOAD_SIZE - 12) {
        throw std::runtime_error("Fragment data too large");
    }
    auto packet = buildHeader(GSPcol::CMD::FRAGMENT,
        static_cast<GSPcol::FLAGS>(static_cast<uint8_t>(GSPcol::FLAGS::RELIABLE) | static_cast<uint8_t>(GSPcol::FLAGS::FRAGMENT)), seq,
        ackBase, ackBits, GSPcol::CHANNEL::RO, static_cast<uint16_t>(HEADER_SIZE + 12 + fragmentData.size()), clientId);
    for (int i = 0; i < 4; i++)
        packet.push_back((baseSeq >> (24 - i * 8)) & 0xFF);
    for (int i = 0; i < 4; i++)
        packet.push_back((totalSize >> (24 - i * 8)) & 0xFF);
    for (int i = 0; i < 4; i++)
        packet.push_back((offset >> (24 - i * 8)) & 0xFF);
    packet.insert(packet.end(), fragmentData.begin(), fragmentData.end());
    return packet;
}

}// namespace rtype::srv
