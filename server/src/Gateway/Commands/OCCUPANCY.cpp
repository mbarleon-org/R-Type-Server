#include <RTypeSrv/Gateway.hpp>
#include <RTypeSrv/GatewayPacketParser.hpp>
#include <stdexcept>

namespace rtype::srv {

/**
 * @brief Handles an OCCUPANCY packet.
 *
 * Packet format: [HEADER:5][CMD:23][OCCUPANCY:1]
 * Total size: 6 bytes
 *
 * The game server is identified by the TCP connection handle, not by data in the packet.
 * No response is sent.
 *
 * @param handle The handle of the sender.
 * @param data A pointer to the data received.
 * @param offset A reference to the current offset in the data (points to CMD byte).
 * @param bufsize The total size of the data.
 */
void Gateway::handleOccupancy(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize)
{
    if (offset + 1 + 1 > bufsize) {
        throw std::runtime_error("Incomplete OCCUPANCY packet");
    }
    uint8_t occ = PacketParser::parseOccupancy(data, offset + 1);
    const std::optional<IP> gs_key = findGSKeyByHandle(handle);
    if (!gs_key) {
        throw std::runtime_error("Occupancy from unregistered game server");
    }
    _occupancy_cache[*gs_key] = occ;
    offset += 1 + 1;
}

}// namespace rtype::srv
