#include <RTypeSrv/GameServer.hpp>
#include <RTypeSrv/GameServerPacketParser.hpp>
#include <RTypeSrv/GameServerUDPPacketParser.hpp>
#include <RTypeSrv/Utils/Logger.hpp>
#include <arpa/inet.h>
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
    if (offset + 1 + 1 > bufsize) {
        utils::cerr("Incomplete CREATE packet from gateway");
        sendErrorResponse(handle);
        return;
    }
    uint8_t gametype = data[offset + 1];
    offset += 2;
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

void rtype::srv::GameServer::_recordAuthAttempt(const network::Handle &handle) noexcept
{
    auto it = _auth_states.find(handle);
    if (it == _auth_states.end()) {
        return;
    }
    it->second.attempts++;
    it->second.timestamp = std::chrono::steady_clock::now();
}

void rtype::srv::GameServer::_cleanupExpiredAuthChallenges() noexcept
{
    const auto now = std::chrono::steady_clock::now();
    std::vector<network::Handle> to_remove;
    for (auto &kv : _auth_states) {
        const auto &h = kv.first;
        const auto &entry = kv.second;
        if (entry.attempts >= MAX_AUTH_ATTEMPTS) {
            to_remove.push_back(h);
            continue;
        }
        if (now - entry.timestamp > AUTH_TIMEOUT) {
            to_remove.push_back(h);
            continue;
        }
    }
    for (auto &h : to_remove) {
        utils::cout("Cleaning up expired auth challenge for handle ", h);
        _auth_states.erase(h);
        _client_states.erase(h);
    }
}

void rtype::srv::GameServer::_parsePackets()
{
    const auto now = std::chrono::steady_clock::now();
    const auto ping_interval = std::chrono::seconds(1);
    for (const auto &kv : _client_ids) {
        uint32_t clientId = kv.first;
        network::Handle h = kv.second;
        auto &metrics = _latency_metrics[h];
        if (auto it = _client_states.find(h); it != _client_states.end() && it->second.authState == AuthState::AUTHENTICATED) {
            if (metrics.last_ping.time_since_epoch().count() == 0 || (now - metrics.last_ping) > ping_interval) {
                auto pkt = GameServerUDPPacketParser::buildHeader(GSPcol::CMD::PING, GSPcol::FLAGS::CONN, _client_sequence_nums[h]++,
                    _last_received_seq[h], _sack_bits[h], GSPcol::CHANNEL::UU, GameServerUDPPacketParser::HEADER_SIZE, clientId);
                _send_spans[h].push_back(std::move(pkt));
                setPolloutForHandle(h);
                metrics.last_ping = now;
            }
        }
    }

    for (auto &[handle, packets] : _recv_packets) {
        for (auto &packet : packets) {
            if (packet.empty())
                continue;
            try {
                std::size_t offset = 0;
                if (packet.size() < 21) {
                    utils::cerr("UDP packet too small (need 21 bytes header, got ", packet.size(), " bytes)");
                    continue;
                }
                uint16_t magic = static_cast<uint16_t>((static_cast<uint16_t>(packet[offset]) << 8) | packet[offset + 1]);
                if (magic != GSPCOL_MAGIC) {
                    utils::cerr("Invalid UDP packet magic (got ", std::hex, magic, ", expected ", GSPCOL_MAGIC, ")");
                    continue;
                }
                offset += 2;
                uint8_t version = packet[offset++];
                if (version != 1) {
                    utils::cerr("Invalid UDP protocol version (got ", static_cast<int>(version), ", expected 1)");
                    continue;
                }
                [[maybe_unused]] uint8_t flags = packet[offset++];
                uint32_t seq = 0;
                memcpy(&seq, packet.data() + offset, 4);
                seq = ntohl(seq);
                offset += 4;
                uint32_t ackBase = 0;
                memcpy(&ackBase, packet.data() + offset, 4);
                ackBase = ntohl(ackBase);
                offset += 4;
                [[maybe_unused]] uint8_t ackBits = packet[offset++];
                [[maybe_unused]] uint8_t channel = packet[offset++];
                uint16_t size = 0;
                memcpy(&size, packet.data() + offset, 2);
                size = ntohs(size);
                offset += 2;
                uint32_t clientId = 0;
                memcpy(&clientId, packet.data() + offset, 4);
                clientId = ntohl(clientId);
                offset += 4;
                uint8_t cmd = packet[offset++];

                switch (static_cast<GSPcol::CMD>(cmd)) {
                    case GSPcol::CMD::JOIN:
                        handleUDPJoin(handle, packet.data(), offset, packet.size(), clientId);
                        break;
                    case GSPcol::CMD::AUTH:
                        handleUDPAuthResponse(handle, packet.data(), offset, packet.size(), clientId);
                        break;
                    case GSPcol::CMD::INPUT:
                        if (auto it = _client_states.find(handle);
                            it != _client_states.end() && it->second.authState == AuthState::AUTHENTICATED) {
                            handleUDPInput(handle, packet.data(), offset, packet.size(), clientId);
                        } else {
                            utils::cerr("Received INPUT from unauthenticated client ", clientId);
                        }
                        break;
                    case GSPcol::CMD::PING:
                        handleUDPPing(handle, packet.data(), offset, packet.size(), clientId);
                        break;
                    case GSPcol::CMD::PONG:
                        handleUDPPong(handle, packet.data(), offset, packet.size(), clientId);
                        break;
                    case GSPcol::CMD::RESYNC:
                        if (auto it = _client_states.find(handle);
                            it != _client_states.end() && it->second.authState == AuthState::AUTHENTICATED) {
                            handleUDPResync(handle, packet.data(), offset, packet.size(), clientId);
                        } else {
                            utils::cerr("Received RESYNC from unauthenticated client ", clientId);
                        }
                        break;
                    default:
                        utils::cerr("Unknown UDP command: ", static_cast<int>(cmd));
                        break;
                }
            } catch (const std::exception &e) {
                utils::cerr("Error parsing UDP packet: ", e.what());
                parseErrors[handle]++;
                if (parseErrors[handle] >= MAX_PARSE_ERRORS) {
                    throw std::runtime_error("Client sent too many malformed packets.");
                }
            }
        }
        packets.clear();
    }
    _cleanupExpiredAuthChallenges();
}
