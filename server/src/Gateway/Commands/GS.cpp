#include <RTypeSrv/Gateway.hpp>
#include <RTypeSrv/GatewayPacketParser.hpp>
#include <stdexcept>

namespace rtype::srv {

/**
 * @brief Handles a GS (Game Server) registration packet.
 * @param handle The handle of the sender.
 * @param data A pointer to the data received.
 * @param offset A reference to the current offset in the data.
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
    uint8_t response = already_registered ? 0 : 1;
    _send_spans[handle].push_back({response});
    setPolloutForHandle(handle);
    offset += 1 + 16 + 2;
}

}// namespace rtype::srv
