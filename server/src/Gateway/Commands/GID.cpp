#include <RTypeSrv/Gateway.hpp>
#include <RTypeSrv/GatewayPacketParser.hpp>
#include <stdexcept>

namespace rtype::srv {

/**
 * @brief Handles a GID packet.
 * @param handle The handle of the sender.
 * @param data A pointer to the data received.
 * @param offset A reference to the current offset in the data.
 * @param bufsize The total size of the data.
 */
void Gateway::handleGID(const network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize)
{
    if (offset + 1 + 4 > bufsize) {
        throw std::runtime_error("Incomplete GID packet");
    }
    const std::size_t pos = offset + 1;
    const std::optional<IP> gs_key = findGSKeyByHandle(handle);
    if (!gs_key) {
        throw std::runtime_error("GS handle not registered");
    }
    const auto gids = PacketParser::parseGIDs(data, pos, bufsize);
    for (uint32_t gid : gids) {
        _game_to_gs[gid] = *gs_key;
    }
    offset = pos + gids.size() * 4;
}

}// namespace rtype::srv
