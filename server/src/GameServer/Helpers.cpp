#include <RTypeNet/Accept.hpp>
#include <RTypeNet/Disconnect.hpp>
#include <RTypeSrv/GameServer.hpp>
#include <RTypeSrv/Utils/IPToStr.hpp>
#include <RTypeSrv/Utils/Logger.hpp>
#include <ranges>

void rtype::srv::GameServer::_disconnectByHandle(const network::Handle &handle) noexcept
{
    if (const auto it = std::ranges::find_if(_sockets, [handle](const auto &pair) { return pair.second.handle == handle; });
        it != _sockets.end()) {
        utils::cout("Disconnecting client at ", utils::ipToStr(it->second.endpoint.ip), ":", it->second.endpoint.port);
        disconnect(it->second);
        _sockets.erase(it);
    }
    _send_spans.erase(handle);
    if (const auto it = std::ranges::find_if(_fds.begin(), _fds.end(), [handle](const auto &elem) { return elem.handle == handle; });
        it != _fds.end()) {
        _fds.erase(it);
        --_nfds;
    }
}

void rtype::srv::GameServer::_acceptClients() noexcept
{
    try {
        const network::Socket client_sock = network::accept(_sock.handle);
        _fds.push_back({client_sock.handle, POLLIN | POLLOUT, 0});
        _sockets[_next_id] = client_sock;
        ++_nfds;
        ++_next_id;
        utils::cout("New client connected: ", utils::ipToStr(client_sock.endpoint.ip), ":", client_sock.endpoint.port);
    } catch (const std::exception &e) {
        utils::cerr("Error accepting new connection: ", e.what());
    }
}
