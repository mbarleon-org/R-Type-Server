#include <RTypeSrv/Gateway.hpp>
#include <RTypeSrv/GatewayPacketParser.hpp>
#include <stdexcept>

namespace rtype::srv {

/**
 * @brief Handles an OCCUPANCY packet.
 *
 * New protocol format: [CMD:23][OCCUPANCY:1]
 * The game server is identified by the handle, not by data in the packet.
 *
 * @param handle The handle of the sender.
 * @param data A pointer to the data received.
 * @param offset A reference to the current offset in the data.
 * @param bufsize The total size of the data.
 */
void Gateway::handleOccupancy(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize)
{
    if (offset + 1 + 1 > bufsize) {
        throw std::runtime_error("Incomplete OCCUPANCY packet");
    }

    // Get the occupancy count from the packet
    uint8_t occ = PacketParser::parseOccupancy(data, offset + 1);

    // Find the GS key for this handle
    const std::optional<IP> gs_key = findGSKeyByHandle(handle);
    if (!gs_key) {
        throw std::runtime_error("Occupancy from unregistered game server");
    }

    // Update the occupancy cache
    _occupancy_cache[*gs_key] = occ;
    offset += 1 + 1;
}

}// namespace rtype::srv
