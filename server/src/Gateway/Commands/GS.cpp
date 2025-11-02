#include <RTypeSrv/Gateway.hpp>
#include <RTypeSrv/GatewayPacketParser.hpp>
#include <stdexcept>

namespace rtype::srv {

/**
 * @brief Handles a GS (Game Server) registration packet.
 *
 * Request format: [HEADER:5][CMD:20][IP:16][PORT:2]
 * Response: [HEADER:5][CMD:21] (GS_OK) or [HEADER:5][CMD:22] (GS_KO)
 *
 * @param handle The handle of the sender.
 * @param data A pointer to the data received.
 * @param offset A reference to the current offset in the data (points to CMD byte).
 * @param bufsize The total size of the data.
 */
void Gateway::handleGSRegistration(const network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize)
{
    if (offset + 1 + 16 + 2 > bufsize) {
        throw std::runtime_error("Incomplete GS Registration packet");
    }
    auto [ip, port] = PacketParser::parseGSKey(data, offset + 1);
    const std::pair key = {ip, port};
    const bool already_registered = _gs_registry.contains(key);
    _gs_registry[key] = 1;
    if (!already_registered) {
        _gs_addr_to_handle[key] = handle;
    }
    uint8_t response_cmd = already_registered ? 22 : 21;
    std::vector<uint8_t> response = PacketParser::buildSimpleResponse(response_cmd);
    _send_spans[handle].push_back(std::move(response));
    setPolloutForHandle(handle);
    offset += 1 + 16 + 2;
}

}// namespace rtype::srv
