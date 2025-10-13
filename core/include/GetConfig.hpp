#pragma once

#include <RTypeNet/Interfaces.hpp>
#include <string>

namespace rtype::srv {

/**
 * @brief Represents the server configuration.
 */
struct Config {
        bool tcp_only = false;
        bool udp_only = false;
        network::Endpoint tcp_endpoint;
        network::Endpoint udp_endpoint;
        network::Endpoint external_udp_endpoint;
        std::size_t n_cores = 4;
};

static constexpr uint16_t default_tcp_port = 3000;
static constexpr uint16_t default_udp_port = 4000;

/**
 * @brief Gets the server configuration from a file.
 * @param filename The name of the configuration file.
 * @return The server configuration.
 */
Config getConfig(const std::string &filename);

}// namespace rtype::srv
