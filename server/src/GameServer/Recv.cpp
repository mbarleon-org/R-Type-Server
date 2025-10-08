#include <RTypeNet/Recv.hpp>
#include <RTypeSrv/GameServer.hpp>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <ranges>

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
