#include "RTypeSrv/GameServerUDPPacketParser.hpp"
#include <RTypeSrv/Components.hpp>
#include <RTypeSrv/Exception.hpp>
#include <RTypeSrv/GameServer.hpp>
#include <RTypeSrv/Utils/Logger.hpp>
#include <ranges>

/**
 * @brief Constructs a new GameServer object.
 *
 * @param baseEndpoint The base endpoint of the server.
 * @param ncores The number of cores to use.
 * @param tcpEndpoint The TCP endpoint of the server.
 * @param quitServer A reference to an atomic boolean that will be set to true when the server should quit.
 */
rtype::srv::GameServer::GameServer(const network::Endpoint &baseEndpoint, std::size_t ncores, const network::Endpoint &tcpEndpoint,
    const network::Endpoint &externalUdpEndpoint, std::atomic<bool> &quitServer)
{
    _ncores = ncores;
    _quit_server = &quitServer;
    _tcp_endpoint = tcpEndpoint;
    _base_endpoint = baseEndpoint;
    _external_endpoint = externalUdpEndpoint;
}

/**
 * @brief Initializes the server.
 */
void rtype::srv::GameServer::StartServer() noexcept
{
    if (_is_running) {
        return;
    }
    try {
        _initServer();
        _serverLoop();
        _cleanupServer();
    } catch (const Exception &e) {
        utils::cerr("Exception caught while running server: ", e.where(), ": ", e.what());
    }
}

void rtype::srv::GameServer::_send_game_snapshots()
{
    for (auto &[game_id, app_ptr] : _game_instances) {
        if (!app_ptr)
            continue;

        auto *snapshot_res = app_ptr->get_resource_ptr<GameStateSnapshot>();
        auto *snapshot_seq_res = app_ptr->get_resource_ptr<SnapshotSequence>();

        if (!snapshot_res || !snapshot_seq_res || snapshot_res->data.empty()) {
            continue;
        }

        std::vector<uint32_t> clients_in_game = get_clients_in_game(game_id);

        for (uint32_t client_id : clients_in_game) {
            if (_client_ids.count(client_id)) {
                network::Handle handle = _client_ids.at(client_id);

                auto packet = rtype::srv::GameServerUDPPacketParser::buildSnapshot(_client_sequence_nums[handle]++,
                    _last_received_seq[handle], _sack_bits[handle], client_id, snapshot_seq_res->sequence_number, snapshot_res->data);

                for (const auto &epkv : _endpoint_to_handle) {
                    if (epkv.second == handle) {
                        _send_spans[epkv.first].push_back(std::move(packet));
                        setPolloutForHandle(_sock.handle);
                        break;
                    }
                }
            }
        }
    }
}

std::vector<uint32_t> rtype::srv::GameServer::get_clients_in_game(uint32_t game_id)
{
    std::vector<uint32_t> clients;
    ;
    for (const auto &[client, gid] : _client_to_game) {
        if (gid == game_id) {
            clients.push_back(client);
        }
    }
    return clients;
}
