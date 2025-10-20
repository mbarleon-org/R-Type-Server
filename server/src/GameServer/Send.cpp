#include <RTypeNet/Send.hpp>
#include <RTypeSrv/GameServer.hpp>
#include <RTypeSrv/Utils/Logger.hpp>
#include <cerrno>
#include <deque>
#include <ranges>
#include <iomanip>
#include <sstream>
#include <utility>

rtype::network::Endpoint rtype::srv::GameServer::GetEndpointFromHandle(const network::Handle &handle)
{
    for (const auto &sock : _sockets | std::views::values) {
        if (sock.handle == handle) {
            return sock.endpoint;
        }
    }
    throw std::runtime_error("Handle not found in sockets map.");
}

void rtype::srv::GameServer::_sendPackets(const network::NFDS i)
{
    const auto handle = _fds[i].handle;

    if (!(_fds[i].revents & POLLOUT)) {
        return;
    }
    const auto it = _send_spans.find(handle);
    if (it == _send_spans.end()) {
        return;
    }
    auto &bufs = it->second;
    if (bufs.empty()) {
        _fds[i].events &= ~POLLOUT;
        return;
    }
    auto client_it = _client_endpoints.find(handle);
    if (client_it == _client_endpoints.end()) {
        bufs.clear();
        _fds[i].events &= ~POLLOUT;
        return;
    }
    const network::Endpoint &client_endpoint = client_it->second;
    for (auto &buf : bufs) {
        if (!buf.empty()) {
            {
                std::ostringstream ss;
                ss << std::hex << std::setfill('0');
                const size_t len = buf.size();
                const size_t show = std::min<size_t>(len, 64);
                for (size_t j = 0; j < show; ++j) {
                    ss << std::setw(2) << static_cast<int>(buf[j]);
                    if (j + 1 < show)
                        ss << ' ';
                }
                rtype::srv::utils::clog("OUT UDP handle=", handle, " len=", len, " hex=", ss.str());
            }
            const ssize_t sent =
                rtype::network::sendto(handle, buf.data(), static_cast<rtype::network::BufLen>(buf.size()), 0, client_endpoint);
            if (sent < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    utils::cerr("Could not send packet.");
                }
                break;
            }
        }
    }
    bufs.clear();
    _fds[i].events &= ~POLLOUT;
}
