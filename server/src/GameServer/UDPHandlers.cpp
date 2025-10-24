#include <RTypeSrv/GameServer.hpp>
#include <RTypeSrv/GameServerUDPPacketParser.hpp>
#include <RTypeSrv/Utils/Crypto.hpp>
#include <RTypeSrv/Utils/Logger.hpp>
#include <random>

namespace rtype::srv {

void GameServer::handleUDPJoin(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize, uint32_t clientId)
{
    if (offset + 6 > bufsize) {
        utils::cerr("Incomplete UDP JOIN packet");
        return;
    }
    uint32_t payload_client_id =
        static_cast<uint32_t>((data[offset] << 24) | (data[offset + 1] << 16) | (data[offset + 2] << 8) | data[offset + 3]);
    offset += 4;

    if (payload_client_id != clientId) {
        utils::cerr("Client ID mismatch in JOIN packet");
        return;
    }
    uint8_t nonce = data[offset++];
    uint8_t version = data[offset++];
    utils::cout("UDP JOIN from client ", clientId, " (nonce=", static_cast<int>(nonce), ", version=", static_cast<int>(version), ")");
    _client_ids[clientId] = handle;
    _client_sequence_nums[handle] = 0;
    _last_received_seq[handle] = 0;
    _sack_bits[handle] = 0;
    ClientState state;
    state.authState = AuthState::CHALLENGED;
    auto challenge = utils::Crypto::generateSecureRandom(32);
    std::copy(challenge.begin(), challenge.end(), state.challenge.begin());
    _client_states[handle] = state;
    AuthChallenge aentry;
    aentry.challenge = state.challenge;
    aentry.timestamp = std::chrono::steady_clock::now();
    aentry.attempts = 0;
    _auth_states[handle] = aentry;
    auto response = GameServerUDPPacketParser::buildChallenge(_client_sequence_nums[handle]++, _last_received_seq[handle],
        _sack_bits[handle], clientId, state.challenge);
    _send_spans[handle].push_back(std::move(response));
    setPolloutForHandle(handle);
}

void GameServer::handleUDPInput(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize, uint32_t clientId)
{
    while (offset + 2 <= bufsize) {
        uint8_t type = data[offset++];
        uint8_t value = data[offset++];
        switch (static_cast<GSPcol::INPUT>(type)) {
            case GSPcol::INPUT::FWD:
                utils::cout("Client ", clientId, " input: FWD = ", static_cast<int>(value));
                // TODO: Update game state with input
                break;
            default:
                utils::cerr("Unknown input type ", static_cast<int>(type), " from client ", clientId);
                break;
        }
    }
    _last_received_seq[handle] = (static_cast<uint32_t>(data[5]) << 24) | (static_cast<uint32_t>(data[6]) << 16)
        | (static_cast<uint32_t>(data[7]) << 8) | static_cast<uint32_t>(data[8]);
    _sack_bits[handle] = static_cast<uint8_t>((_sack_bits[handle] << 1) | 1);
}

void GameServer::handleUDPPing(network::Handle handle, [[maybe_unused]] const uint8_t *data, [[maybe_unused]] std::size_t &offset,
    [[maybe_unused]] std::size_t bufsize, uint32_t clientId)
{
    _latency_metrics[handle].last_ping = std::chrono::steady_clock::now();
    auto response = GameServerUDPPacketParser::buildPongResponse(_client_sequence_nums[handle]++, _last_received_seq[handle],
        _sack_bits[handle], clientId);

    _send_spans[handle].push_back(std::move(response));
    setPolloutForHandle(handle);
}

void GameServer::handleUDPPong([[maybe_unused]] network::Handle handle, [[maybe_unused]] const uint8_t *data,
    [[maybe_unused]] std::size_t &offset, [[maybe_unused]] std::size_t bufsize, uint32_t clientId)
{
    auto now = std::chrono::steady_clock::now();
    auto &metrics = _latency_metrics[handle];
    if (metrics.last_ping.time_since_epoch().count() != 0) {
        auto rtt = std::chrono::duration_cast<std::chrono::microseconds>(now - metrics.last_ping);
        metrics.min_rtt = std::min(metrics.min_rtt, rtt);
        metrics.max_rtt = std::max(metrics.max_rtt, rtt);
        metrics.avg_rtt = (metrics.avg_rtt * metrics.samples + rtt) / (metrics.samples + 1);
        metrics.samples++;
        utils::cout("PONG from client ", clientId, " RTT(us)=", rtt.count(), " avg(us)=", metrics.avg_rtt.count());
    } else {
        utils::cout("PONG from client ", clientId, " (no matching ping timestamp)");
    }
}

void GameServer::handleUDPResync(network::Handle handle, [[maybe_unused]] const uint8_t *data, [[maybe_unused]] std::size_t &offset,
    [[maybe_unused]] std::size_t bufsize, uint32_t clientId)
{
    utils::cout("Resync requested from client ", clientId);

    // TODO: Get current game state
    std::vector<uint8_t> state_data = {1, 2, 3, 4};
    uint32_t snapshot_seq = 1;
    auto response = GameServerUDPPacketParser::buildSnapshot(_client_sequence_nums[handle]++, _last_received_seq[handle],
        _sack_bits[handle], clientId, snapshot_seq, state_data);
    _send_spans[handle].push_back(std::move(response));
    setPolloutForHandle(handle);
}

void GameServer::handleUDPAuthResponse(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize,
    uint32_t clientId)
{
    if (offset + 32 > bufsize) {
        utils::cerr("Incomplete AUTH_RESPONSE packet");
        return;
    }
    auto it = _client_states.find(handle);
    if (it == _client_states.end() || it->second.authState != AuthState::CHALLENGED) {
        utils::cerr("Received AUTH_RESPONSE in invalid state from client ", clientId);
        return;
    }
    std::vector<uint8_t> response(data + offset, data + offset + 32);
    offset += 32;
    std::vector<uint8_t> challenge(it->second.challenge.begin(), it->second.challenge.end());
    auto expectedHMAC = utils::Crypto::hmacSHA256(challenge, challenge);

    if (response != expectedHMAC) {
        utils::cerr("Invalid authentication response from client ", clientId);
        _recordAuthAttempt(handle);
        return;
    }
    auto derivedKey = utils::Crypto::deriveKey(challenge, response);
    std::copy(derivedKey.begin(), derivedKey.begin() + 8, it->second.sessionKey.begin());
    it->second.authState = AuthState::AUTHENTICATED;
    auto auth_ok = GameServerUDPPacketParser::buildAuthOkPacket(_client_sequence_nums[handle]++, _last_received_seq[handle],
        _sack_bits[handle], clientId, it->second.sessionKey);
    _send_spans[handle].push_back(std::move(auth_ok));
    setPolloutForHandle(handle);
    utils::cout("Client ", clientId, " successfully authenticated");
}

}// namespace rtype::srv
