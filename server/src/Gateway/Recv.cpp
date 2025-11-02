#include <RTypeNet/Recv.hpp>
#include <RTypeSrv/Gateway.hpp>
#include <RTypeSrv/Utils/Logger.hpp>
#include <iomanip>
#include <sstream>

/**
 * @brief Receives packets from a client.
 *
 * @param i The index of the client in the `_fds` array.
 */
void rtype::srv::Gateway::_recvPackets(const network::NFDS i)
{
    const auto handle = _fds[i].handle;
    std::vector<uint8_t> buffer(1024);

    if (const ssize_t ret = network::recv(handle, buffer.data(), static_cast<network::BufLen>(buffer.size()), 0); ret > 0) {
        auto &accum = _recv_spans[handle];
        accum.insert(accum.end(), buffer.begin(), buffer.begin() + ret);
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
            rtype::srv::utils::clog("IN  TCP handle=", handle, " len=", len, " hex=", ss.str());
        }
        if (accum.size() > MAX_BUFFER_SIZE) {
            throw std::runtime_error("Client exceded max buffer size.");
        }
    } else {
        throw std::runtime_error("Client closed connection.");
    }
}
