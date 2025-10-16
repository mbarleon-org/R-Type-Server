#include <RTypeSrv/Gateway.hpp>
#include <RTypeSrv/GatewayPacketParser.hpp>
#include <stdexcept>

namespace rtype::srv {

/**
 * @brief Handles a JOIN packet.
 * @param handle The handle of the sender.
 * @param data A pointer to the data received.
 * @param offset A reference to the current offset in the data.
 * @param bufsize The total size of the data.
 */
void Gateway::handleJoin(const network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize)
{
    if (offset + 1 + 4 > bufsize) {
        throw std::runtime_error("Incomplete JOIN packet");
    }

    const uint32_t id = PacketParser::extractGameId(data + offset + 1);
    if (_gs_registry.empty()) {
        sendErrorResponse(handle);
    } else if (const auto it = _pending_creates.find(handle); it != _pending_creates.end()) {
        const network::Handle client_handle = it->second.first;
        std::vector<uint8_t> join_msg = PacketParser::buildJoinMsgForClient(data, offset);
        const uint32_t game_id = PacketParser::extractGameId(join_msg.data() + join_msg.size() - 4);
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
        sendErrorResponse(handle);
    }
    offset += 1 + 4;
}

}// namespace rtype::srv
