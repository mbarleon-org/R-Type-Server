#include <RTypeSrv/Gateway.hpp>
#include <RTypeSrv/GatewayPacketParser.hpp>
#include <stdexcept>

namespace rtype::srv {

/**
 * @brief Handles a GID packet.
 *
 * Packet format: [HEADER:5][CMD:24][LEN:1][GAME_ID:4]...
 * Total size: 6 + (LEN * 4) bytes
 *
 * The game server registers which game IDs it's hosting.
 * No response is sent.
 *
 * @param handle The handle of the sender.
 * @param data A pointer to the data received.
 * @param offset A reference to the current offset in the data (points to CMD byte).
 * @param bufsize The total size of the data.
 */
void Gateway::handleGID(const network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize)
{
    if (offset + 1 + 1 + 4 > bufsize) {
        throw std::runtime_error("Incomplete GID packet");
    }
    const uint8_t len = data[offset + 1];
    const std::size_t expected_size = offset + 1 + 1 + (len * 4);
    if (expected_size > bufsize) {
        throw std::runtime_error("Incomplete GID packet - insufficient game IDs");
    }
    const std::optional<IP> gs_key = findGSKeyByHandle(handle);
    if (!gs_key) {
        throw std::runtime_error("GS handle not registered");
    }
    const std::size_t gid_start = offset + 2;
    const auto gids = PacketParser::parseGIDs(data, gid_start, gid_start + (len * 4));
    for (uint32_t gid : gids) {
        _game_to_gs[gid] = *gs_key;
    }
    offset = expected_size;
}

}// namespace rtype::srv
