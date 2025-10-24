#include <RTypeSrv/GameServer.hpp>
#include <RTypeSrv/GameServerUDPPacketParser.hpp>
#include <RTypeSrv/Utils/Crypto.hpp>
#include <RTypeSrv/Utils/Logger.hpp>
#include <cstdlib>
#include <openssl/crypto.h>
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
    const char *env_secret = std::getenv("R_TYPE_SHARED_SECRET");
    const std::string secret_str = env_secret ? std::string(env_secret) : std::string("r-type-shared-secret");
    if (!env_secret) {
        utils::cout("R_TYPE_SHARED_SECRET not set, falling back to built-in secret (not recommended for production)");
    }
    std::vector<uint8_t> secret(secret_str.begin(), secret_str.end());
    std::array<uint8_t, 16> ip_bytes{};
    if (auto it_ep = _client_endpoints.find(handle); it_ep != _client_endpoints.end()) {
        ip_bytes = it_ep->second.ip;
    } else {
        try {
            auto ep = GetEndpointFromHandle(handle);
            ip_bytes = ep.ip;
        } catch (...) {
            ip_bytes.fill(0);
        }
    }
    const uint64_t timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    std::vector<uint8_t> mac_data;
    mac_data.insert(mac_data.end(), ip_bytes.begin(), ip_bytes.end());
    mac_data.push_back(nonce);
    for (int i = 0; i < 8; ++i) {
        mac_data.push_back(static_cast<uint8_t>((timestamp >> (56 - i * 8)) & 0xFF));
    }

    auto mac_vec = utils::Crypto::hmacSHA256(secret, mac_data);
    std::array<uint8_t, 32> cookie{};
    if (mac_vec.size() >= cookie.size()) {
        std::copy_n(mac_vec.begin(), cookie.size(), cookie.begin());
    } else {
        std::fill(cookie.begin(), cookie.end(), 0);
        std::copy(mac_vec.begin(), mac_vec.end(), cookie.begin());
    }

    _client_states[handle] = state;
    AuthChallenge aentry;
    aentry.challenge.fill(0);
    aentry.timestamp = std::chrono::steady_clock::now();
    aentry.attempts = 0;
    _auth_states[handle] = aentry;

    auto response = GameServerUDPPacketParser::buildChallengeWithCookie(_client_sequence_nums[handle]++, _last_received_seq[handle],
        _sack_bits[handle], clientId, timestamp, cookie);
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
    if (offset + 1 + 32 > bufsize) {
        utils::cerr("Incomplete AUTH_RESPONSE packet");
        return;
    }
    auto it = _client_states.find(handle);
    if (it == _client_states.end() || it->second.authState != AuthState::CHALLENGED) {
        utils::cerr("Received AUTH_RESPONSE in invalid state from client ", clientId);
        return;
    }
    uint8_t client_nonce = data[offset++];
    std::array<uint8_t, 32> received_cookie{};
    std::copy_n(data + offset, 32, received_cookie.begin());
    offset += 32;
    const char *env_secret = std::getenv("R_TYPE_SHARED_SECRET");
    const std::string secret_str = env_secret ? std::string(env_secret) : std::string("r-type-shared-secret");
    if (!env_secret) {
        utils::cout("R_TYPE_SHARED_SECRET not set, falling back to built-in secret (not recommended for production)");
    }
    std::vector<uint8_t> secret(secret_str.begin(), secret_str.end());
    std::array<uint8_t, 16> ip_bytes{};
    if (auto it_ep = _client_endpoints.find(handle); it_ep != _client_endpoints.end()) {
        ip_bytes = it_ep->second.ip;
    } else {
        try {
            auto ep = GetEndpointFromHandle(handle);
            ip_bytes = ep.ip;
        } catch (...) {
            ip_bytes.fill(0);
        }
    }
    const auto now_s = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    bool valid = false;
    uint64_t found_ts = 0;
    for (int64_t dt = 0; dt <= static_cast<int64_t>(AUTH_TIMEOUT.count()); ++dt) {
        uint64_t ts = now_s - static_cast<uint64_t>(dt);
        std::vector<uint8_t> mac_data;
        mac_data.insert(mac_data.end(), ip_bytes.begin(), ip_bytes.end());
        mac_data.push_back(client_nonce);
        for (int i = 0; i < 8; ++i) {
            mac_data.push_back(static_cast<uint8_t>((ts >> (56 - i * 8)) & 0xFF));
        }
        auto mac = utils::Crypto::hmacSHA256(secret, mac_data);
        if (mac.size() >= received_cookie.size() && CRYPTO_memcmp(mac.data(), received_cookie.data(), received_cookie.size()) == 0) {
            valid = true;
            found_ts = ts;
            break;
        }
    }
    if (!valid) {
        utils::cerr("Invalid authentication cookie from client ", clientId);
        _recordAuthAttempt(handle);
        return;
    }
    std::vector<uint8_t> salt(8);
    for (size_t i = 0; i < 8; ++i)
        salt[i] = static_cast<uint8_t>((found_ts >> (56 - i * 8)) & 0xFF);
    auto derived = utils::Crypto::deriveKey(std::vector<uint8_t>(secret.begin(), secret.end()), salt);
    std::copy(derived.begin(), derived.begin() + 8, it->second.sessionKey.begin());
    it->second.authState = AuthState::AUTHENTICATED;
    auto auth_ok = GameServerUDPPacketParser::buildAuthOkPacket(_client_sequence_nums[handle]++, _last_received_seq[handle],
        _sack_bits[handle], clientId, it->second.sessionKey);
    _send_spans[handle].push_back(std::move(auth_ok));
    setPolloutForHandle(handle);
    utils::cout("Client ", clientId, " successfully authenticated");
}

}// namespace rtype::srv
