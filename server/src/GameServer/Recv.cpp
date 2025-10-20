#include <RTypeNet/Recv.hpp>
#include <RTypeSrv/GameServer.hpp>
#include <RTypeSrv/Utils/Logger.hpp>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <ranges>
#include <sstream>

void rtype::srv::GameServer::_recvPackets(const network::NFDS i)
{
    const auto handle = _fds[i].handle;
    std::vector<uint8_t> buffer(1024);
    network::Endpoint endpoint;

    if (const ssize_t ret = recvfrom(handle, buffer.data(), static_cast<network::BufLen>(buffer.size()), 0, endpoint); ret > 0) {
        if (!_client_endpoints.contains(handle)) {
            _client_endpoints[handle] = endpoint;
        }
        _recv_packets[handle].push_back(std::vector<uint8_t>(buffer.begin(), buffer.begin() + ret));
        {
            std::ostringstream ss;
            ss << std::hex << std::setfill('0');
            const size_t len = static_cast<size_t>(ret);
            const size_t show = std::min<size_t>(len, 64);
            for (size_t j = 0; j < show; ++j) {
                ss << std::setw(2) << static_cast<int>(buffer[j]);
                if (j + 1 < show)
                    ss << ' ';
            }
            rtype::srv::utils::clog("IN  UDP handle=", handle, " len=", len, " hex=", ss.str());
        }

    } else if (ret < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
#if defined(_WIN32)
            char error_buf[256];
            strerror_s(error_buf, sizeof(error_buf), errno);
            throw std::runtime_error("recvfrom error: " + std::string(error_buf));
#else
            throw std::runtime_error("recvfrom error: " + std::string(strerror(errno)));
#endif
        }
    }
}
