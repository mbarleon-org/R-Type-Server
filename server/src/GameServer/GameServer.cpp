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
