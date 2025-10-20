#include <RTypeSrv/Gateway.hpp>
#include <RTypeSrv/GatewayPacketParser.hpp>

namespace rtype::srv {

/**
 * @brief Handles a GAME_END packet.
 *
 * Packet format: [HEADER:5][CMD:5][GAME_ID:4]
 * Total size: 9 bytes
 *
 * Fire-and-forget notification from game server that a game has ended.
 * Gateway removes the game from its routing table.
 * No response is sent.
 *
 * @param handle The handle of the sender (game server).
 * @param data A pointer to the data received.
 * @param offset A reference to the current offset in the data (points to CMD byte).
 * @param bufsize The total size of the data.
 */
    void Gateway::handleGameEnd(const network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize)
    {
        if (offset + 1 + 4 > bufsize) {
            throw std::runtime_error("Incomplete GAME_END packet");
        }
        const uint32_t game_id = PacketParser::extractGameId(data + offset + 1);
        const std::optional<IP> gs_key = findGSKeyByHandle(handle);
        if (!gs_key) {
            throw std::runtime_error("GAME_END from unregistered game server");
        }
        if (auto it = _game_to_gs.find(game_id); it != _game_to_gs.end()) {
            if (it->second == *gs_key) {
                _game_to_gs.erase(it);
            } else {
                throw std::runtime_error("GAME_END for game not owned by this server");
            }
        }
        offset += 1 + 4;
    }

}// namespace rtype::srv
