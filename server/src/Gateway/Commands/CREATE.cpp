#include <RTypeSrv/Gateway.hpp>
#include <RTypeSrv/GatewayPacketParser.hpp>
#include <stdexcept>

namespace rtype::srv {

/**
 * @brief Handles a CREATE packet.
 *
 * Request format: [HEADER:5][CMD:3][GAMETYPE:1]
 * Total request size: 6 bytes
 *
 * Success response: [HEADER:5][CMD:1][GAME_ID:4][IP:16][PORT:2] (27 bytes, JOIN packet)
 * Failure response: [HEADER:5][CMD:4] (5 bytes, CREATE_KO)
 *
 * @param handle The handle of the sender.
 * @param data A pointer to the data received.
 * @param offset A reference to the current offset in the data (points to CMD byte).
 * @param bufsize The total size of the data.
 */
void Gateway::handleCreate(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize)
{
    if (offset + 2 > bufsize) {
        throw std::runtime_error("Incomplete CREATE packet");
    }
    uint8_t gametype = data[offset + 1];
    if (_gs_registry.empty()) {
        std::vector<uint8_t> error_msg = PacketParser::buildSimpleResponse(4);// CREATE_KO
        _send_spans[handle].push_back(std::move(error_msg));
        setPolloutForHandle(handle);
        offset += 2;
        return;
    }
    auto min_gs = findLeastOccupiedGS();
    if (!min_gs) {
        std::vector<uint8_t> error_msg = PacketParser::buildSimpleResponse(4);// CREATE_KO
        _send_spans[handle].push_back(std::move(error_msg));
        setPolloutForHandle(handle);
        offset += 2;
        return;
    }
    auto &[gs_key, _] = **min_gs;
    const network::Handle gs_handle = getGSHandle(gs_key);
    if (gs_handle == 0) {
        std::vector<uint8_t> error_msg = PacketParser::buildSimpleResponse(4);// CREATE_KO
        _send_spans[handle].push_back(std::move(error_msg));
        setPolloutForHandle(handle);
        offset += 2;
        return;
    }
    std::vector<uint8_t> create_msg = PacketParser::buildCreateMsg(gametype);
    _send_spans[gs_handle].push_back(std::move(create_msg));
    setPolloutForHandle(gs_handle);
    _pending_creates[gs_handle] = {handle, gametype};
    offset += 2;
}

}// namespace rtype::srv
