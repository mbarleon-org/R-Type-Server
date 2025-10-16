#include <RTypeSrv/Gateway.hpp>
#include <RTypeSrv/GatewayPacketParser.hpp>
#include <array>
#include <cstring>
#include <ranges>
#include <stdexcept>

/**
 * @brief Sets the POLLOUT flag for a given handle.
 * @param h The handle to set the POLLOUT flag for.
 */
void rtype::srv::Gateway::setPolloutForHandle(const network::Handle h) noexcept
{
    for (auto &fd : _fds) {
        if (fd.handle == h) {
            fd.events |= POLLOUT;
            break;
        }
    }
}

/**
 * @brief Finds the least occupied game server.
 * @return An iterator to the least occupied game server, or std::nullopt if no game servers are available.
 */
std::optional<rtype::srv::Gateway::GsRegistryType::iterator> rtype::srv::Gateway::findLeastOccupiedGS()
{
    auto min_gs = _gs_registry.end();
    std::size_t min_occupancy = SIZE_MAX;
    for (auto it = _gs_registry.begin(); it != _gs_registry.end(); ++it) {
        std::size_t occ = 0;
        auto occ_it = _occupancy_cache.find(it->first);
        occ = (occ_it != _occupancy_cache.end()) ? occ_it->second : 0;
        if (occ < min_occupancy) {
            min_occupancy = occ;
            min_gs = it;
        }
    }
    if (min_gs == _gs_registry.end()) {
        return std::nullopt;
    }
    return min_gs;
}

/**
 * @brief Gets the handle for a game server.
 * @param gs_key The key of the game server.
 * @return The handle of the game server, or 0 if the game server is not found.
 */
rtype::network::Handle rtype::srv::Gateway::getGSHandle(const IP &gs_key) const
{
    if (const auto it_handle = _gs_addr_to_handle.find(gs_key); it_handle != _gs_addr_to_handle.end()) {
        return it_handle->second;
    }
    return 0;
}

/**
 * @brief Sends an error response to a client.
 * @param handle The handle of the client to send the error to.
 */
void rtype::srv::Gateway::sendErrorResponse(const network::Handle handle)
{
    _send_spans[handle].push_back({0});
    setPolloutForHandle(handle);
}

/**
 * @brief Finds the GS key for a given handle.
 * @param handle The handle to find the GS key for.
 * @return The GS key, or std::nullopt if the handle is not found.
 */
std::optional<rtype::srv::Gateway::IP> rtype::srv::Gateway::findGSKeyByHandle(const network::Handle handle) const noexcept
{
    for (const auto &[key, h] : _gs_addr_to_handle) {
        if (h == handle) {
            return key;
        }
    }
    return std::nullopt;
}

/**
 * @brief Parses packets received from clients.
 */
void rtype::srv::Gateway::_parsePackets()
{
    for (auto &[handle, buf] : _recv_spans) {
        std::size_t offset = 0;
        while (offset < buf.size()) {
            try {
                const uint8_t pkt = PacketParser::getHeader(buf.data(), offset, buf.size());
                switch (pkt) {
                    case 0:
                        handleKO(handle, buf.data(), offset, buf.size());
                        break;
                    case 1:
                        handleOK(handle, buf.data(), offset, buf.size());
                        break;
                    case 2:
                        handleJoin(handle, buf.data(), offset, buf.size());
                        break;
                    case 3:
                        handleCreate(handle, buf.data(), offset, buf.size());
                        break;
                    case 20:
                        handleGSRegistration(handle, buf.data(), offset, buf.size());
                        break;
                    case 21:
                        handleOccupancy(handle, buf.data(), offset, buf.size());
                        break;
                    case 22:
                        handleGID(handle, buf.data(), offset, buf.size());
                        break;
                    default:
                        throw std::runtime_error("Invalid packet sent by client.");
                }
            } catch (const std::exception &) {
                _parseErrors[handle]++;
                if (_parseErrors[handle] >= MAX_PARSE_ERRORS) {
                    throw std::runtime_error("Client sent too many malformed packets.");
                }
                break;
            }
        }
        if (offset > 0 && offset <= buf.size()) {
            buf.erase(buf.begin(), buf.begin() + static_cast<long long>(offset));
        }
    }
}
