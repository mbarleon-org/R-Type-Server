#include <RTypeNet/Recv.hpp>
#include <RTypeSrv/Gateway.hpp>

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
        if (accum.size() > MAX_BUFFER_SIZE) {
            throw std::runtime_error("Client exceded max buffer size.");
        }
    } else {
        throw std::runtime_error("Client closed connection.");
    }
}
