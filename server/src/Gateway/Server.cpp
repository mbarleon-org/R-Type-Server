#include <RTypeSrv/Exception.hpp>
#include <RTypeSrv/Gateway.hpp>
#include <RTypeSrv/Utils/Logger.hpp>

#if defined(_WIN32)
template class RTYPE_SRV_API rtype::srv::utils::Singleton<rtype::srv::Gateway>;
#endif

/**
 * @brief Starts the server.
 *
 * This function initializes the server, starts the main server loop, and
 * cleans up the server's resources when it's done.
 */
void rtype::srv::Gateway::startServer() noexcept
{
    try {
        _startServer();
        _serverLoop();
        _cleanupServer();
    } catch (const std::exception &e) {
        utils::cerr("Caught exception in server: ", e.what());
    }
}

/**
 * @brief Initializes the server.
 *
 * This function sets the TCP endpoint and the quit server flag. It should
 * only be called once.
 *
 * @param tcp_endpoint The TCP endpoint to use.
 * @param quit_server A reference to an atomic boolean that will be set to
 * true when the server should quit.
 *
 * @throws Exception if the server has already been initialized.
 */
void rtype::srv::Gateway::initServer(const network::Endpoint &tcp_endpoint, std::atomic<bool> &quit_server)
{
    if (_is_init) {
        throw Exception("initServer", "Server was already initialized.");
    }
    _quit_server = &quit_server;
    _tcp_endpoint = tcp_endpoint;
    _is_init = true;
}
