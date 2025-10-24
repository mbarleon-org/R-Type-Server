#pragma once

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif

#include <RTypeNet/Interfaces.hpp>
#include <RTypeSrv/Api.hpp>
#include <RTypeSrv/Utils/NonCopyable.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rtype::srv {

/**
 * @brief The main class for the R-Type game server.
 */
class RTYPE_SRV_API GameServer : public utils::NonCopyable
{
    public:
        /**
         * @brief Constructs a new GameServer object.
         * @param baseEndpoint The base endpoint for the server.
         * @param ncores The number of cores to use.
         * @param tcpEndpoint The TCP endpoint for the server.
         * @param quitServer A reference to an atomic boolean that will be set to true when the server should quit.
         */
        GameServer(const network::Endpoint &baseEndpoint, std::size_t ncores, const network::Endpoint &tcpEndpoint,
            const network::Endpoint &externalUdpEndpoint, std::atomic<bool> &quitServer);
        ~GameServer() noexcept = default;

        /**
         * @brief Starts the server.
         */
        void StartServer() noexcept;

    private:
        static constexpr uint8_t MAX_PARSE_ERRORS = 3;
        static constexpr size_t MAX_AUTH_ATTEMPTS = 3;
        static constexpr size_t MAX_BUFFER_SIZE = 64 * 1024;
        static constexpr auto AUTH_TIMEOUT = std::chrono::seconds(5);
        static constexpr auto FRAGMENT_TIMEOUT = std::chrono::seconds(1);

        enum class AuthState { NONE, CHALLENGED, AUTHENTICATED };

        struct ClientState {
                AuthState authState = AuthState::NONE;
                std::array<uint8_t, 32> challenge;
                std::array<uint8_t, 32> sessionKey;
        };

        struct AuthChallenge {
                std::array<uint8_t, 32> challenge;
                std::chrono::steady_clock::time_point timestamp;
                uint8_t attempts{0};
        };

        struct PlayerState {
                float x{0.0f}, y{0.0f};
                float vx{0.0f}, vy{0.0f};
                bool active{false};
                uint32_t last_input_seq{0};
                std::chrono::steady_clock::time_point last_update;
        };

        struct LatencyMetrics {
                std::chrono::microseconds min_rtt{(std::chrono::microseconds::max)()};
                std::chrono::microseconds max_rtt{(std::chrono::microseconds::min)()};
                std::chrono::microseconds avg_rtt{0};
                uint32_t samples{0};
                std::chrono::steady_clock::time_point last_ping;
        };

        struct FragmentBuffer {
                std::vector<std::vector<uint8_t>> fragments;
                std::chrono::steady_clock::time_point first_fragment;
                size_t total_size{0};
                uint32_t base_seq{0};
        };

        struct PairKeyHash {
                std::size_t operator()(const std::pair<network::Handle, uint32_t> &p) const noexcept
                {
                    uint64_t key = (static_cast<uint64_t>(static_cast<uint64_t>(p.first)) << 32) ^ p.second;
                    return std::hash<uint64_t>{}(key);
                }
        };

        using FdsType = std::vector<network::PollFD>;
        using IP = std::pair<std::array<uint8_t, 16>, uint16_t>;
        using SeqMapType = std::unordered_map<network::Handle, uint32_t>;
        using SackBitsType = std::unordered_map<network::Handle, uint8_t>;
        using PlayerStatesType = std::unordered_map<uint32_t, PlayerState>;
        using ClientIDsType = std::unordered_map<uint32_t, network::Handle>;
        using ParseErrorsType = std::unordered_map<network::Handle, uint8_t>;
        using SocketsMapType = std::unordered_map<std::size_t, network::Socket>;
        using AuthStatesType = std::unordered_map<network::Handle, AuthChallenge>;
        using ClientStatesType = std::unordered_map<network::Handle, ClientState>;
        using RecvSpanType = std::unordered_map<network::Handle, std::vector<uint8_t>>;
        using LatencyMetricsType = std::unordered_map<network::Handle, LatencyMetrics>;
        using ClientEndpointsType = std::unordered_map<network::Handle, network::Endpoint>;
        using SendSpanType = std::unordered_map<network::Handle, std::vector<std::vector<uint8_t>>>;
        using RecvPacketsType = std::unordered_map<network::Handle, std::vector<std::vector<uint8_t>>>;
        using FragBufType = std::unordered_map<std::pair<network::Handle, uint32_t>, FragmentBuffer, PairKeyHash>;

        void _initServer();
        void _serverLoop();
        void _cleanupServer();
        void _parsePackets();
        void _recvTcpPackets();
        void _sendTcpPackets();
        void _parseTcpPackets();
        void _sendGSRegistration();
        void _acceptClients() noexcept;
        void _recvPackets(network::NFDS i);
        void _sendPackets(network::NFDS i);
        void _handleLoop(network::NFDS &i);
        void _cleanupExpiredAuthChallenges() noexcept;
        void _handleClients(network::NFDS &i) noexcept;
        void sendErrorResponse(network::Handle handle);
        void _handleClientsSend(network::NFDS &i) noexcept;
        void setPolloutForHandle(network::Handle h) noexcept;
        void _recordAuthAttempt(const network::Handle &handle) noexcept;
        void _disconnectByHandle(const network::Handle &handle) noexcept;
        network::Endpoint GetEndpointFromHandle(const network::Handle &handle);
        std::vector<uint8_t> buildJoinMsgForClient(const uint8_t *data, std::size_t offset);
        void _handleOccupancyRequest(const uint8_t *data, std::size_t &offset, std::size_t bufsize);
        void handleOKKO(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize);
        void handleCreate(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize);
        void handleOccupancy(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize);
        static void _handleGatewayOKKO(const uint8_t cmd, const uint8_t *data, std::size_t &offset, std::size_t bufsize);
        void handleUDPJoin(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize, uint32_t clientId);
        void handleUDPPing(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize, uint32_t clientId);
        void handleUDPPong(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize, uint32_t clientId);
        void handleUDPInput(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize, uint32_t clientId);
        void handleUDPResync(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize, uint32_t clientId);
        void handleUDPAuthResponse(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize,
            uint32_t clientId);

        FdsType _fds{};
        network::NFDS _nfds = 1;
        SocketsMapType _sockets;
        network::Socket _sock{};
        std::size_t _ncores = 4;
        SendSpanType _send_spans;
        std::size_t _next_id = 0;
        bool _is_running = false;
        SackBitsType _sack_bits{};
        ClientIDsType _client_ids{};
        network::Socket _tcp_sock{};
        ParseErrorsType parseErrors;
        RecvSpanType _tcp_recv_spans;
        SendSpanType _tcp_send_spans;
        network::Handle _tcp_handle{};
        RecvPacketsType _recv_packets;
        AuthStatesType _auth_states{};
        network::Socket _server_sock{};
        SeqMapType _last_received_seq{};
        FragBufType _fragment_buffers{};
        network::Endpoint _tcp_endpoint{};
        PlayerStatesType _player_states{};
        ClientStatesType _client_states{};
        SeqMapType _client_sequence_nums{};
        network::Endpoint _base_endpoint{};
        network::Endpoint _my_tcp_endpoint{};
        LatencyMetricsType _latency_metrics{};
        ClientEndpointsType _client_endpoints;
        network::Endpoint _external_endpoint{};
        std::atomic<bool> *_quit_server = nullptr;
};

}// namespace rtype::srv

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
