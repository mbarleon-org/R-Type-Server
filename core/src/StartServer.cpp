#include "StartServer.hpp"
#include <RTypeSrv/Exception.hpp>
#include <RTypeSrv/GameServer.hpp>
#include <RTypeSrv/Gateway.hpp>
#include <iostream>

/**
 * @brief Starts the TCP server in a new thread.
 * @param endpoint The endpoint to listen on.
 * @param quitServer An atomic boolean to signal the server to quit.
 * @return A thread object representing the running server.
 */
std::thread rtype::srv::startTcpServer(const network::Endpoint &endpoint, std::atomic<bool> &quitServer) noexcept
{
    return std::thread([endpoint, &quitServer]() {
        Gateway &s = Gateway::getInstance();

        try {
            s.initServer(endpoint, quitServer);
            s.startServer();
        } catch (const Exception &e) {
            std::cerr << "Exception caught while running server: " << e.where() << ": " << e.what() << std::endl;
        }
    });
}

/**
 * @brief Starts the UDP servers in new threads.
 * @param baseEndpoint The base endpoint to listen on.
 * @param ncores The number of cores to use.
 * @param tcpEndpoint The TCP endpoint to connect to.
 * @param quitServer An atomic boolean to signal the server to quit.
 * @return A vector of thread objects representing the running servers.
 */
std::vector<std::thread> rtype::srv::startUdpServers(network::Endpoint baseEndpoint, std::size_t ncores,
    const network::Endpoint &tcpEndpoint, network::Endpoint externalUdpEndpoint, std::atomic<bool> &quitServer) noexcept
{
    std::vector<std::thread> threads{};

    threads.reserve(ncores);
    for (std::size_t i = 0; i < ncores; ++i) {
        threads.emplace_back([baseEndpoint, ncores, tcpEndpoint, externalUdpEndpoint, &quitServer]() {
            try {
                GameServer(baseEndpoint, ncores, tcpEndpoint, externalUdpEndpoint, quitServer).StartServer();
            } catch (const Exception &e) {
                std::cerr << "Exception caught while running server: " << e.where() << ": " << e.what() << std::endl;
            }
        });
        ++baseEndpoint.port;
        ++externalUdpEndpoint.port;
    }
    return threads;
}
