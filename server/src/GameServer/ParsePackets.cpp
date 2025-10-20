#include <RTypeSrv/GameServer.hpp>
#include <RTypeSrv/GameServerPacketParser.hpp>
#include <RTypeSrv/Utils/Logger.hpp>
#include <cstring>
#include <iomanip>
#include <ranges>
#include <sstream>
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
    std::vector<uint8_t> error_packet = GameServerPacketParser::buildCreateKO();
    _tcp_send_spans[handle].push_back(std::move(error_packet));
    setPolloutForHandle(handle);
}

std::vector<uint8_t> rtype::srv::GameServer::buildJoinMsgForClient(const uint8_t *data, std::size_t offset)
{
    return std::vector(data, data + offset);
}

void rtype::srv::GameServer::handleCreate([[maybe_unused]] network::Handle handle, const uint8_t *data, std::size_t &offset,
    std::size_t bufsize)
{
    if (offset + 1 + 1 > bufsize) {// CMD + gametype
        utils::cerr("Incomplete CREATE packet from gateway");
        sendErrorResponse(handle);
        return;
    }

    uint8_t gametype = data[offset + 1];// Skip CMD byte, read gametype
    offset += 2;                        // Consume both CMD and gametype

    utils::cout("Received CREATE request from gateway, gametype: ", static_cast<int>(gametype));

    // TODO: Actually create a game and get a real game ID
    // For now, just send back a mock response
    constexpr uint32_t mock_game_id = 12345;

    std::vector<uint8_t> join_response =
        GameServerPacketParser::buildJoinResponse(mock_game_id, _external_endpoint.ip, _external_endpoint.port);
    {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0');
        for (size_t i = 0; i < join_response.size(); ++i) {
            ss << std::setw(2) << static_cast<int>(join_response[i]);
            if (i + 1 < join_response.size())
                ss << ' ';
        }
        utils::cout("Outgoing JOIN response (hex): ", ss.str());
    }

    _tcp_send_spans[handle].push_back(std::move(join_response));
    setPolloutForHandle(handle);

    utils::cout("Sent JOIN response to gateway for game ID: ", mock_game_id);
}

void rtype::srv::GameServer::handleOccupancy([[maybe_unused]] network::Handle handle, [[maybe_unused]] const uint8_t *data,
    [[maybe_unused]] std::size_t &offset, [[maybe_unused]] std::size_t bufsize)
{
    utils::cerr("Unexpected OCCUPANCY packet received from gateway");
}

void rtype::srv::GameServer::handleOKKO([[maybe_unused]] network::Handle handle, [[maybe_unused]] const uint8_t *data,
    [[maybe_unused]] std::size_t &offset, [[maybe_unused]] std::size_t bufsize)
{
    utils::cout("Received OK/KO response from gateway");
}

void rtype::srv::GameServer::_parsePackets()
{
    for (auto &[handle, packets] : _recv_packets) {
        for (auto &packet : packets) {
            if (packet.empty())
                continue;

            try {
                // TODO: Implement proper GSPcol (UDP) packet parsing
                // For now, this is a placeholder
                // std::size_t offset = 0;

                // Would need to parse 21-byte UDP header here
                // Then handle commands like CMD_JOIN, CMD_INPUT, CMD_PING, etc.

                utils::cerr("UDP packet parsing not yet implemented");

            } catch (const std::exception &e) {
                parseErrors[handle]++;
                if (parseErrors[handle] >= MAX_PARSE_ERRORS) {
                    throw std::runtime_error("Client sent too many malformed packets.");
                }
            }
        }
        packets.clear();
    }
}
