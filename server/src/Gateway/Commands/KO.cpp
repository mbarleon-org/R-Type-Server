#include <RTypeSrv/Gateway.hpp>
#include <stdexcept>

namespace rtype::srv {

/**
 * @brief Handles a KO packet.
 * @param handle The handle of the sender.
 * @param data A pointer to the data received.
 * @param offset A reference to the current offset in the data.
 * @param bufsize The total size of the data.
 */
void Gateway::handleKO([[maybe_unused]] network::Handle handle, [[maybe_unused]] const uint8_t *data, std::size_t &offset,
    std::size_t bufsize)
{
    if (offset + 1 > bufsize) {
        throw std::runtime_error("Incomplete OK/KO packet");
    }
    offset += 1;
}

}// namespace rtype::srv
