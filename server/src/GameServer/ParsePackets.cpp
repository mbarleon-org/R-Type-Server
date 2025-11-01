#include "R-Engine/Plugins/Plugin.hpp"
#include <RTypeSrv/Components.hpp>
#include <RTypeSrv/GameServer.hpp>
#include <RTypeSrv/GameServerPacketParser.hpp>
#include <RTypeSrv/GameServerUDPPacketParser.hpp>
#include <RTypeSrv/Systems.hpp>
#include <RTypeSrv/Utils/Logger.hpp>
#include <cstring>
#include <iomanip>
#include <ranges>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
    #include <winsock2.h>
#else
    #include <arpa/inet.h>
#endif

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

    uint32_t new_game_id = generate_unique_game_id();
    utils::cout("Received CREATE from Gateway. Creating game with ID: ", new_game_id);

    auto game_app = std::make_unique<r::Application>();

    game_app->add_events<PlayerInputEvent, AssignPlayerSlotEvent>()
        .insert_resource(SnapshotSequence{})
        .add_systems<spawn_player_system>(r::Schedule::STARTUP)
        .add_systems<handle_player_input_system, assign_player_slot_system>(r::Schedule::UPDATE)
        .add_systems<movement_system>(r::Schedule::UPDATE)
        .after<handle_player_input_system>()
        .add_systems<debug_print_player_positions_system>(r::Schedule::UPDATE)
        .after<movement_system>()
        .add_systems<create_snapshot_system>(r::Schedule::EVENT_CLEANUP);

    _game_instances.emplace(new_game_id, std::move(game_app));

    _game_instances.at(new_game_id)->init();

    std::vector<uint8_t> join_response =
        GameServerPacketParser::buildJoinResponse(new_game_id, _external_endpoint.ip, _external_endpoint.port);
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
    utils::cout("Sent JOIN response to gateway for game ID: ", new_game_id);
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

    std::vector<decltype(_ep_auth_states.begin())> ep_to_remove;
    for (auto &kv : _ep_auth_states) {
        const auto &entry = kv.second;
        if (entry.attempts >= MAX_AUTH_ATTEMPTS) {
            ep_to_remove.push_back(_ep_auth_states.find(kv.first));
            continue;
        }
        if (now - entry.timestamp > AUTH_TIMEOUT) {
            ep_to_remove.push_back(_ep_auth_states.find(kv.first));
            continue;
        }
    }
    for (auto &it : ep_to_remove) {
        if (it != _ep_auth_states.end()) {
            utils::cout("Cleaning up expired auth challenge for endpoint");
            _ep_auth_states.erase(it->first);
            _ep_client_states.erase(it->first);
            _send_spans.erase(it->first);
            _endpoint_to_client.erase(it->first);
        }
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
                for (const auto &epkv : _endpoint_to_handle) {
                    if (epkv.second == h) {
                        _send_spans[epkv.first].push_back(pkt);
                        setPolloutForHandle(_sock.handle);
                    }
                }
                metrics.last_ping = now;
            }
        }
    }

    for (auto &[ep_key, packets] : _recv_packets) {
        network::Handle handle = 0;
        if (auto hit = _endpoint_to_handle.find(ep_key); hit != _endpoint_to_handle.end()) {
            handle = hit->second;
        }
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
                        handleUDPJoin(ep_key, packet.data(), offset, packet.size(), clientId);
                        break;
                    case GSPcol::CMD::AUTH:
                        handleUDPAuthResponse(ep_key, packet.data(), offset, packet.size(), clientId);
                        break;
                    case GSPcol::CMD::INPUT:
                        if (handle != 0) {
                            if (auto it = _client_states.find(handle);
                                it != _client_states.end() && it->second.authState == AuthState::AUTHENTICATED) {
                                handleUDPInput(ep_key, packet.data(), offset, packet.size(), clientId);
                            } else {
                                utils::cerr("Received INPUT from unauthenticated client ", clientId);
                            }
                        } else {
                            utils::cerr("Received INPUT from unknown handle for client ", clientId);
                        }
                        break;
                    case GSPcol::CMD::PING:
                        handleUDPPing(ep_key, packet.data(), offset, packet.size(), clientId);
                        break;
                    case GSPcol::CMD::PONG:
                        handleUDPPong(ep_key, packet.data(), offset, packet.size(), clientId);
                        break;
                    case GSPcol::CMD::RESYNC:
                        if (handle != 0) {
                            if (auto it = _client_states.find(handle);
                                it != _client_states.end() && it->second.authState == AuthState::AUTHENTICATED) {
                                handleUDPResync(ep_key, packet.data(), offset, packet.size(), clientId);
                            } else {
                                utils::cerr("Received RESYNC from unauthenticated client ", clientId);
                            }
                        } else {
                            utils::cerr("Received RESYNC from unknown handle for client ", clientId);
                        }
                        break;
                    default:
                        utils::cerr("Unknown UDP command: ", static_cast<int>(cmd));
                        break;
                }
            } catch (const std::exception &e) {
                utils::cerr("Error parsing UDP packet: ", e.what());
                if (handle != 0) {
                    parseErrors[handle]++;
                    if (parseErrors[handle] >= MAX_PARSE_ERRORS) {
                        throw std::runtime_error("Client sent too many malformed packets.");
                    }
                }
            }
        }
        packets.clear();
    }
    _cleanupExpiredAuthChallenges();
}
