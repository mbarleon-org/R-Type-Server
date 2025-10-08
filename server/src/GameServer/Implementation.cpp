#include <RTypeNet/Cleanup.hpp>
#include <RTypeNet/Connect.hpp>
#include <RTypeNet/Disconnect.hpp>
#include <RTypeNet/Listen.hpp>
#include <RTypeNet/Poll.hpp>
#include <RTypeNet/Startup.hpp>
#include <RTypeSrv/Exception.hpp>
#include <RTypeSrv/GameServer.hpp>
#include <RTypeSrv/Utils/IPToStr.hpp>
#include <RTypeSrv/Utils/Logger.hpp>

void rtype::srv::GameServer::_initServer()
{
    network::startup();
    try {
        _sock = listen(_base_endpoint, network::Protocol::UDP);
    } catch (const std::exception &e) {
        throw Exception("startServer", "Could not start listening on ", utils::ipToStr(_base_endpoint.ip), ":", _base_endpoint.port, ": ",
            e.what());
    }
    _fds.push_back({_sock.handle, POLLIN, 0});
    _is_running = true;
    utils::cout("Game server listening on ", utils::ipToStr(_base_endpoint.ip), ":", _base_endpoint.port, "...");
    _my_tcp_endpoint.ip = _base_endpoint.ip;
    _my_tcp_endpoint.port = 0;
    _tcp_handle = connect(_my_tcp_endpoint, _tcp_endpoint, network::Protocol::TCP);
    if (_tcp_handle == -1) {
        throw Exception("_initServer", "Failed to connect to TCP gateway at ", utils::ipToStr(_tcp_endpoint.ip), ":", _tcp_endpoint.port);
    }
    _fds.push_back({_tcp_handle, POLLIN, 0});
    _nfds = static_cast<network::NFDS>(_fds.size());
    utils::cout("Connected to TCP gateway at ", utils::ipToStr(_tcp_endpoint.ip), ":", _tcp_endpoint.port);
    _sendGSRegistration();
}

void rtype::srv::GameServer::_handleClients(network::NFDS &i) noexcept
{
    try {
        _recvPackets(i);
        _parsePackets();
    } catch (const std::exception &e) {
        utils::cerr("Error handling client socket: ", e.what());
        _disconnectByHandle(_fds[i].handle);
        --i;
    }
}

void rtype::srv::GameServer::_handleClientsSend(network::NFDS &i) noexcept
{
    try {
        _sendPackets(i);
    } catch (const std::exception &e) {
        utils::cerr("Error handling client socket: ", e.what());
        _disconnectByHandle(_fds[i].handle);
        --i;
    }
}

void rtype::srv::GameServer::_handleLoop(network::NFDS &i)
{
    if (_fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
        if (_fds[i].handle == _tcp_handle) {
            throw Exception("TCP gateway connection lost!");
        }
        _disconnectByHandle(_fds[i].handle);
        --i;
        return;
    }

    if (_fds[i].handle == _sock.handle) {
        if (_fds[i].revents & POLLIN) {
            _handleClients(i);
        }
        if (_fds[i].revents & POLLOUT) {
            _handleClientsSend(i);
        }
    } else if (_fds[i].handle == _tcp_handle) {
        try {
            if (_fds[i].revents & POLLIN) {
                _recvTcpPackets();
                _parseTcpPackets();
            }
            if (_fds[i].revents & POLLOUT) {
                _sendTcpPackets();
            }
        } catch (const std::exception &e) {
            throw Exception(e.what());
        }
    }
}

void rtype::srv::GameServer::_serverLoop()
{
    while (!(*_quit_server)) {
        if (network::poll(_fds.data(), _nfds, 0) == -1) {
            utils::cerr("Poll error, stopping server...");
            break;
        }
        for (network::NFDS i = 0; i < _nfds; ++i) {
            _handleLoop(i);
        }
    }
}
void rtype::srv::GameServer::_cleanupServer()
{
    _send_spans.clear();
    _recv_packets.clear();
    _client_endpoints.clear();
    _tcp_recv_spans.clear();
    _tcp_send_spans.clear();
    _sockets.clear();
    _fds.clear();
    _nfds = 0;
    _next_id = 0;
    disconnect(_sock);
    if (_tcp_handle != -1) {
        network::Socket s{_tcp_endpoint, _tcp_handle, network::Protocol::TCP};
        disconnect(s);
    }
    _is_running = false;
    utils::cout("Game server stopped.");
    network::cleanup();
}
