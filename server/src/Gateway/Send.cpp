#include <RTypeNet/Send.hpp>
#include <RTypeSrv/Gateway.hpp>
#include <deque>
#include <ranges>
#include <utility>

namespace {

/**
 * @brief A buffer for sending data.
 */
struct SendBuf {
        std::vector<uint8_t> data;
        size_t offset = 0;
};

/**
 * @brief Prepares a queue of buffers to be sent.
 * @param bufs The buffers to prepare.
 * @return A queue of buffers to be sent.
 */
std::deque<SendBuf> prepareSendQueue(std::vector<std::vector<uint8_t>> &bufs)
{
    std::deque<SendBuf> sendQueue;
    for (auto &buf : bufs) {
        if (!buf.empty())
            sendQueue.push_back(SendBuf{std::move(buf), 0});
    }
    bufs.clear();
    return sendQueue;
}

/**
 * @brief Processes a queue of buffers to be sent.
 * @param handle The handle of the recipient.
 * @param sendQueue The queue of buffers to be sent.
 */
void processSendQueue(const rtype::network::Handle handle, std::deque<SendBuf> &sendQueue)
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

/**
 * @brief Updates the send buffers with any remaining data that was not sent.
 * @param sendQueue The queue of buffers that were sent.
 * @param bufs The buffers to update.
 */
void updateSendBuffers(std::deque<SendBuf> &sendQueue, std::vector<std::vector<uint8_t>> &bufs)
{
    for (auto &[data, offset] : sendQueue) {
        if (offset < data.size()) {
            std::vector remaining(data.begin() + static_cast<long long>(offset), data.end());
            bufs.push_back(std::move(remaining));
        }
    }
}

}// namespace

/**
 * @brief Sends packets to a client.
 * @param i The index of the client in the `_fds` array.
 */
void rtype::srv::Gateway::_sendPackets(const network::NFDS i)
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
    auto sendQueue = prepareSendQueue(bufs);
    processSendQueue(handle, sendQueue);
    updateSendBuffers(sendQueue, bufs);
}

/**
 * @brief Sends occupancy requests to all registered game servers.
 */
void rtype::srv::Gateway::sendOccupancyRequests()
{
    for (const auto &gs_key : _gs_registry | std::views::keys) {
        if (auto it = _gs_addr_to_handle.find(gs_key); it != _gs_addr_to_handle.end()) {
            network::Handle gs_handle = it->second;
            std::vector<uint8_t> occ_req = {21};
            _send_spans[gs_handle].push_back(std::move(occ_req));
        }
    }
}
