#pragma once

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif

#include <RTypeNet/Interfaces.hpp>
#include <RTypeSrv/Api.hpp>
#include <RTypeSrv/Utils/NonCopyable.hpp>
#include <atomic>
#include <cstddef>
#include <unordered_map>
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
        static constexpr size_t MAX_BUFFER_SIZE = 64 * 1024;

        using IP = std::pair<std::array<uint8_t, 16>, uint16_t>;

        using FdsType = std::vector<network::PollFD>;
        using ParseErrorsType = std::unordered_map<network::Handle, uint8_t>;
        using SocketsMapType = std::unordered_map<std::size_t, network::Socket>;
        using RecvSpanType = std::unordered_map<network::Handle, std::vector<uint8_t>>;
        using ClientEndpointsType = std::unordered_map<network::Handle, network::Endpoint>;
        using SendSpanType = std::unordered_map<network::Handle, std::vector<std::vector<uint8_t>>>;
        using RecvPacketsType = std::unordered_map<network::Handle, std::vector<std::vector<uint8_t>>>;

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
        void _handleClients(network::NFDS &i) noexcept;
        void _handleClientsSend(network::NFDS &i) noexcept;
        void _disconnectByHandle(const network::Handle &handle) noexcept;
        network::Endpoint GetEndpointFromHandle(const network::Handle &handle);

        void sendErrorResponse(network::Handle handle);
        void setPolloutForHandle(network::Handle h) noexcept;
        std::vector<uint8_t> buildJoinMsgForClient(const uint8_t *data, std::size_t offset);
        void handleOKKO(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize);
        void handleCreate(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize);
        void handleOccupancy(network::Handle handle, const uint8_t *data, std::size_t &offset, std::size_t bufsize);

        void _handleOccupancyRequest(const uint8_t *data, std::size_t &offset, std::size_t bufsize);
        static void _handleGatewayOKKO(const uint8_t cmd, const uint8_t *data, std::size_t &offset, std::size_t bufsize);

        FdsType _fds{};
        network::NFDS _nfds = 1;
        SocketsMapType _sockets;
        network::Socket _sock{};
        std::size_t _ncores = 4;
        SendSpanType _send_spans;
        std::size_t _next_id = 0;
        bool _is_running = false;
        network::Socket _tcp_sock{};
        ParseErrorsType parseErrors;
        RecvSpanType _tcp_recv_spans;
        SendSpanType _tcp_send_spans;
        network::Handle _tcp_handle{};
        RecvPacketsType _recv_packets;
        network::Socket _server_sock{};
        network::Endpoint _tcp_endpoint{};
        network::Endpoint _base_endpoint{};
        network::Endpoint _my_tcp_endpoint{};
        ClientEndpointsType _client_endpoints;
        network::Endpoint _external_endpoint{};
        std::atomic<bool> *_quit_server = nullptr;
};

}// namespace rtype::srv

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
