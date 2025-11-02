#pragma once

#include <RTypeNet/Interfaces.hpp>
#include <thread>
#include <vector>

namespace rtype::srv {

/**
 * @brief Starts the TCP server in a new thread.
 * @param endpoint The endpoint to listen on.
 * @param quitServer An atomic boolean to signal the server to quit.
 * @return A thread object representing the running server.
 */
[[nodiscard]] std::thread startTcpServer(const network::Endpoint &endpoint, std::atomic<bool> &quitServer) noexcept;

/**
 * @brief Starts the UDP servers in new threads.
 * @param baseEndpoint The base endpoint to listen on.
 * @param ncores The number of cores to use.
 * @param tcpEndpoint The TCP endpoint to connect to.
 * @param quitServer An atomic boolean to signal the server to quit.
 * @return A vector of thread objects representing the running servers.
 */
[[nodiscard]] std::vector<std::thread> startUdpServers(network::Endpoint baseEndpoint, std::size_t ncores,
    const network::Endpoint &tcpEndpoint, network::Endpoint externalUdpEndpoint, std::atomic<bool> &quitServer) noexcept;

}// namespace rtype::srv
