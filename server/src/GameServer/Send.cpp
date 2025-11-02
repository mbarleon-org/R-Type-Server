#include <RTypeNet/Send.hpp>
#include <RTypeSrv/GameServer.hpp>
#include <RTypeSrv/Utils/IPToStr.hpp>
#include <RTypeSrv/Utils/Logger.hpp>
#include <cerrno>
#include <cstring>
#include <deque>
#include <iomanip>
#include <ranges>
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
    const auto fd_handle = _fds[i].handle;

    if (!(_fds[i].revents & POLLOUT)) {
        return;
    }

    if (fd_handle == _sock.handle) {
        for (auto it = _send_spans.begin(); it != _send_spans.end();) {
            const auto &ep_key = it->first;
            auto &bufs = it->second;
            if (bufs.empty()) {
                it = _send_spans.erase(it);
                continue;
            }
            network::Endpoint client_endpoint{ep_key.first, ep_key.second};
            for (auto &buf : bufs) {
                if (buf.empty())
                    continue;
                std::ostringstream ss;
                ss << std::hex << std::setfill('0');
                const size_t len = buf.size();
                const size_t show = std::min<size_t>(len, 64);
                for (size_t j = 0; j < show; ++j) {
                    ss << std::setw(2) << static_cast<int>(buf[j]);
                    if (j + 1 < show)
                        ss << ' ';
                }
                rtype::srv::utils::clog("OUT UDP to=", utils::ipToStr(client_endpoint.ip), ":", client_endpoint.port, " len=", len,
                    " hex=", ss.str());

                std::ostringstream ephex;
                ephex << std::hex << std::setfill('0');
                for (size_t b = 0; b < client_endpoint.ip.size(); ++b) {
                    ephex << std::setw(2) << static_cast<int>(client_endpoint.ip[b]);
                    if (b + 1 < client_endpoint.ip.size())
                        ephex << ' ';
                }
                const bool endpoint_is_ipv6 = rtype::network::isIPv6(client_endpoint);
                utils::clog("OUT UDP to=", utils::ipToStr(client_endpoint.ip), ":", client_endpoint.port, " (raw=", ephex.str(),
                    ") ipv6=", endpoint_is_ipv6, " len=", buf.size());

                bool ip_all_zero = true;
                for (auto v : client_endpoint.ip) {
                    if (v != 0) {
                        ip_all_zero = false;
                        break;
                    }
                }
                if (client_endpoint.port == 0 || ip_all_zero) {
                    utils::cerr("Skipping send: invalid client endpoint (port=", client_endpoint.port, ") or IP all-zero");
                    continue;
                }
                const ssize_t sent =
                    rtype::network::sendto(_sock.handle, buf.data(), static_cast<rtype::network::BufLen>(buf.size()), 0, client_endpoint);
                if (sent < 0) {
                    const int err = errno;
                    if (err == EAGAIN || err == EWOULDBLOCK) {
                        utils::cerr("Socket buffer full, will retry later");
                        continue;
                    }
#if defined(_WIN32)
                    char error_buf[256];
                    strerror_s(error_buf, sizeof(error_buf), errno);
                    utils::cerr("Could not send packet: ", error_buf, " (errno=", err, ")");
#else
                    utils::cerr("Could not send packet: ", std::strerror(err), " (errno=", err, ")");
#endif
                    continue;
                }
            }
            it = _send_spans.erase(it);
        }
        _fds[i].events &= ~POLLOUT;
        return;
    }
}
