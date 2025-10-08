#include <RTypeSrv/GameServer.hpp>
#include <cstring>
#include <ranges>
#include <stdexcept>

void rtype::srv::GameServer::setPolloutForHandle(const network::Handle h) noexcept
{
    for (auto &fd : _fds) {
        if (fd.handle == h) {
            fd.events |= POLLOUT;
            break;
        }
    }
}

void rtype::srv::GameServer::sendErrorResponse(const network::Handle handle)
{
    _send_spans[handle].push_back({0});
    setPolloutForHandle(handle);
}

std::vector<uint8_t> rtype::srv::GameServer::buildJoinMsgForClient([[maybe_unused]] const uint8_t *data,
    [[maybe_unused]] std::size_t offset)
{
    return std::vector(data, data + offset);
}

void rtype::srv::GameServer::handleCreate([[maybe_unused]] network::Handle handle, [[maybe_unused]] const uint8_t *data,
    [[maybe_unused]] std::size_t &offset, [[maybe_unused]] std::size_t bufsize)
{
}

void rtype::srv::GameServer::handleOccupancy([[maybe_unused]] network::Handle handle, [[maybe_unused]] const uint8_t *data,
    [[maybe_unused]] std::size_t &offset, [[maybe_unused]] std::size_t bufsize)
{
}

void rtype::srv::GameServer::handleOKKO([[maybe_unused]] network::Handle handle, [[maybe_unused]] const uint8_t *data,
    [[maybe_unused]] std::size_t &offset, [[maybe_unused]] std::size_t bufsize)
{
}

void rtype::srv::GameServer::_parsePackets()
{
    for (auto &[handle, packets] : _recv_packets) {
        for (auto &packet : packets) {
            if (packet.empty())
                continue;

            try {
                std::size_t offset = 0;
                switch (packet[0]) {
                    case 0:
                    case 1:
                        handleOKKO(handle, packet.data(), offset, packet.size());
                        break;
                    case 3:
                        handleCreate(handle, packet.data(), offset, packet.size());
                        break;
                    case 21:
                        handleOccupancy(handle, packet.data(), offset, packet.size());
                        break;
                    default:
                        throw std::runtime_error("Invalid packet sent by client.");
                }
            } catch (const std::exception &) {
                parseErrors[handle]++;
                if (parseErrors[handle] >= MAX_PARSE_ERRORS) {
                    throw std::runtime_error("Client sent too many malformed packets.");
                }
            }
        }
        packets.clear();
    }
}
