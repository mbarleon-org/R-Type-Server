#include <RTypeNet/Recv.hpp>
#include <RTypeNet/Send.hpp>
#include <RTypeSrv/GameServer.hpp>
#include <RTypeSrv/Utils/Logger.hpp>
#include <cerrno>
#include <cstring>
#include <deque>

#if !defined(_WIN32)
    #include <netinet/in.h>
#endif

namespace {

struct SendBuf {
        std::vector<uint8_t> data;
        size_t offset = 0;
};

std::deque<SendBuf> prepareTcpSendQueue(std::vector<std::vector<uint8_t>> &bufs)
{
    std::deque<SendBuf> sendQueue;
    for (auto &buf : bufs) {
        if (!buf.empty())
            sendQueue.push_back(SendBuf{std::move(buf), 0});
    }
    bufs.clear();
    return sendQueue;
}

void processTcpSendQueue(const rtype::network::Handle handle, std::deque<SendBuf> &sendQueue)
{
    while (!sendQueue.empty()) {
        auto &[data, offset] = sendQueue.front();
        const size_t to_send = data.size() - offset;
        const ssize_t sent = rtype::network::send(handle, data.data() + offset, static_cast<rtype::network::BufLen>(to_send), 0);
        if (sent < 0) {
            break;
        }
        offset += static_cast<size_t>(sent);
        if (offset == data.size()) {
            sendQueue.pop_front();
        } else {
            break;
        }
    }
}

void updateTcpSendBuffers(std::deque<SendBuf> &sendQueue, std::vector<std::vector<uint8_t>> &bufs)
{
    for (auto &[data, offset] : sendQueue) {
        if (offset < data.size()) {
            std::vector remaining(data.begin() + static_cast<long long>(offset), data.end());
            bufs.push_back(std::move(remaining));
        }
    }
}

}// namespace

void rtype::srv::GameServer::_recvTcpPackets()
{
    std::vector<uint8_t> buffer(1024);

    const ssize_t ret = network::recv(_tcp_handle, buffer.data(), static_cast<network::BufLen>(buffer.size()), 0);

    if (ret > 0) {
        auto &accum = _tcp_recv_spans[_tcp_handle];
        accum.insert(accum.end(), buffer.begin(), buffer.begin() + ret);
        if (accum.size() > MAX_BUFFER_SIZE) {
            throw std::runtime_error("TCP gateway exceeded max buffer size");
        }
    } else if (ret == 0) {
        throw std::runtime_error("TCP gateway closed connection");
    } else {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
#if defined(_WIN32)
            char error_buf[256];
            strerror_s(error_buf, sizeof(error_buf), errno);
            throw std::runtime_error("TCP recv error: " + std::string(error_buf));
#else
            throw std::runtime_error("TCP recv error: " + std::string(strerror(errno)));
#endif
        }
    }
}

void rtype::srv::GameServer::_sendTcpPackets()
{
    auto it = _tcp_send_spans.find(_tcp_handle);
    if (it == _tcp_send_spans.end()) {
        return;
    }

    auto &bufs = it->second;
    if (bufs.empty()) {
        for (auto &fd : _fds) {
            if (fd.handle == _tcp_handle) {
                fd.events &= ~POLLOUT;
                break;
            }
        }
        return;
    }

    auto sendQueue = prepareTcpSendQueue(bufs);
    processTcpSendQueue(_tcp_handle, sendQueue);
    updateTcpSendBuffers(sendQueue, bufs);
}

void rtype::srv::GameServer::_parseTcpPackets()
{
    const auto it = _tcp_recv_spans.find(_tcp_handle);
    if (it == _tcp_recv_spans.end()) {
        return;
    }

    auto &buf = it->second;
    std::size_t offset = 0;
    while (offset < buf.size()) {
        try {
            switch (buf[offset]) {
                case 0:
                case 1:
                    _handleGatewayOKKO(buf.data(), offset, buf.size());
                    break;
                case 21:
                    _handleOccupancyRequest(buf.data(), offset, buf.size());
                    break;
                default:
                    utils::cerr("Unknown packet type from TCP gateway: ", static_cast<int>(buf[offset]));
                    offset++;
                    break;
            }
        } catch (const std::exception &e) {
            utils::cerr("Error parsing TCP packet: ", e.what());
            break;
        }
    }
    if (offset > 0 && offset <= buf.size()) {
        buf.erase(buf.begin(), buf.begin() + static_cast<long long>(offset));
    }
}

void rtype::srv::GameServer::_sendGSRegistration()
{
    std::vector<uint8_t> packet;

    packet.push_back(20);
    packet.insert(packet.end(), _base_endpoint.ip.begin(), _base_endpoint.ip.end());
    const uint16_t port_be = htons(_base_endpoint.port);
    const auto port_bytes = reinterpret_cast<const uint8_t *>(&port_be);
    packet.insert(packet.end(), port_bytes, port_bytes + 2);
    _tcp_send_spans[_tcp_handle].push_back(std::move(packet));
    for (auto &fd : _fds) {
        if (fd.handle == _tcp_handle) {
            fd.events |= POLLOUT;
            break;
        }
    }
    utils::cout("Sent GS registration to gateway");
}

void rtype::srv::GameServer::_handleGatewayOKKO(const uint8_t *data, std::size_t &offset, std::size_t bufsize)
{
    if (offset + 1 > bufsize) {
        throw std::runtime_error("Incomplete OKKO packet from gateway");
    }
    if (const uint8_t response = data[offset]; response == 1) {
        utils::cout("Successfully registered with TCP gateway");
    } else {
        utils::cerr("Failed to register with TCP gateway");
    }
    offset += 1;
}

void rtype::srv::GameServer::_handleOccupancyRequest([[maybe_unused]] const uint8_t *data, std::size_t &offset, std::size_t bufsize)
{
    if (offset + 1 > bufsize) {
        throw std::runtime_error("Incomplete occupancy request from gateway");
    }
    constexpr uint8_t occupancy = 0;
    std::vector<uint8_t> response;
    response.push_back(21);
    response.insert(response.end(), _external_endpoint.ip.begin(), _external_endpoint.ip.end());
    const uint16_t port_be = htons(_external_endpoint.port);
    const auto port_bytes = reinterpret_cast<const uint8_t *>(&port_be);
    response.insert(response.end(), port_bytes, port_bytes + 2);
    response.push_back(occupancy);
    _tcp_send_spans[_tcp_handle].push_back(std::move(response));
    for (auto &fd : _fds) {
        if (fd.handle == _tcp_handle) {
            fd.events |= POLLOUT;
            break;
        }
    }
    offset += 1;
    utils::cout("Sent occupancy response to gateway: ", static_cast<int>(occupancy));
}
