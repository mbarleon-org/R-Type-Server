#include <RTypeSrv/Gateway.hpp>
#include <RTypeSrv/GatewayPacketParser.hpp>
#include <stdexcept>

namespace rtype::srv {

/**
 * @brief Handles a JOIN packet.
 *
 * Client request format: [HEADER:5][CMD:1][GAME_ID:4]
 * Total request size: 9 bytes
 *
 * Success response: [HEADER:5][CMD:1][GAME_ID:4][IP:16][PORT:2] (27 bytes)
 * Failure response: [HEADER:5][CMD:2] (5 bytes, JOIN_KO)
 *
 * This handler is called in two contexts:
 * 1. Client -> Gateway: Client wants to join a game
 * 2. Game Server -> Gateway: Game server responding to CREATE with game info
 *
 * @param handle The handle of the sender.
 * @param data A pointer to the data received.
 * @param offset A reference to the current offset in the data (points to CMD byte).
 * @param bufsize The total size of the data.
 */
void Gateway::handleJoin(const network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize)
{
    if (offset + 1 + 4 > bufsize) {
        throw std::runtime_error("Incomplete JOIN packet");
    }

    const uint32_t id = PacketParser::extractGameId(data + offset + 1);
    if (_gs_registry.empty()) {
        std::vector<uint8_t> error_msg = PacketParser::buildSimpleResponse(2);// JOIN_KO
        _send_spans[handle].push_back(std::move(error_msg));
        setPolloutForHandle(handle);
    } else if (const auto it = _pending_creates.find(handle); it != _pending_creates.end()) {
        const network::Handle client_handle = it->second.first;
        std::vector<uint8_t> join_msg = PacketParser::buildJoinMsgForClient(data, offset + 1);
        const uint32_t game_id = PacketParser::extractGameId(join_msg.data() + 5);
        if (const std::optional<IP> gs_key = findGSKeyByHandle(handle)) {
            _game_to_gs[game_id] = *gs_key;
        }
        _send_spans[client_handle].push_back(std::move(join_msg));
        setPolloutForHandle(client_handle);
        _pending_creates.erase(it);
    } else if (_game_to_gs.contains(id)) {
        auto &[fst, snd] = _game_to_gs[id];
        std::vector<uint8_t> join_msg = PacketParser::buildJoinMsgForGS(fst, snd, id);
        _send_spans[handle].push_back(std::move(join_msg));
        setPolloutForHandle(handle);
    } else {
        std::vector<uint8_t> error_msg = PacketParser::buildSimpleResponse(2);// JOIN_KO
        _send_spans[handle].push_back(std::move(error_msg));
        setPolloutForHandle(handle);
    }
    offset += 1 + 4;
}

}// namespace rtype::srv
