#pragma once

#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4251)
#endif

#include <RTypeNet/Interfaces.hpp>
#include <RTypeSrv/Api.hpp>
#include <RTypeSrv/Utils/Singleton.hpp>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>

/**
 * @brief A hash function for std::array<unsigned char, 16>.
 */
template<>
struct std::hash<std::array<unsigned char, 16>> {
        /**
         * @brief Computes the hash of an array.
         * @param arr The array to hash.
         * @return The hash of the array.
         */
        std::size_t operator()(const std::array<unsigned char, 16> &arr) const noexcept
        {
            std::size_t h = 0;
            for (auto b : arr) {
                h = h * 31 + std::hash<unsigned char>{}(b);
            }
            return h;
        }
};

namespace rtype::srv {

/**
 * @brief A hash function for std::pair<std::array<uint8_t, 16>, uint16_t>.
 */
struct RTYPE_SRV_API pair_hash {
        /**
         * @brief Computes the hash of a pair.
         * @param p The pair to hash.
         * @return The hash of the pair.
         */
        std::size_t operator()(const std::pair<std::array<uint8_t, 16>, uint16_t> &p) const
        {
            const std::size_t h1 = std::hash<uint16_t>{}(p.second);
            std::size_t h2 = 0;
            for (const auto b : p.first)
                h2 ^= std::hash<uint8_t>{}(b) + 0x9e3779b9 + (h2 << 6) + (h2 >> 2);
            return h1 ^ (h2 << 1);
        };
};

/**
 * @brief A hash function for std::array.
 */
struct RTYPE_SRV_API array_hash {
        /**
         * @brief Computes the hash of an array.
         * @param arr The array to hash.
         * @return The hash of the array.
         */
        template<typename T, std::size_t N>
        std::size_t operator()(const std::array<T, N> &arr) const
        {
            std::size_t hash = 0;
            for (const auto &elem : arr) {
                hash ^= std::hash<T>{}(elem) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        };
};

/**
 * @brief The main gateway for the R-Type server.
 *
 * This class is responsible for handling all incoming and outgoing network traffic.
 * It is a singleton, so there can only be one instance of it.
 */
class RTYPE_SRV_API Gateway final : public utils::Singleton<Gateway>
{
        friend class Singleton;

    public:
        /**
         * @brief Starts the server.
         */
        void startServer() noexcept;

        /**
         * @brief Initializes the server.
         * @param tcp_endpoint The TCP endpoint to use.
         * @param quit_server A pointer to an atomic boolean that will be set to true when the server should quit.
         */
        void initServer(const network::Endpoint &tcp_endpoint, std::atomic<bool> &quit_server);

    protected:
        explicit Gateway() = default;
        ~Gateway() noexcept = default;

    private:
        static constexpr uint8_t MAX_PARSE_ERRORS = 3; ///< The maximum number of parse errors before a client is disconnected.
        static constexpr size_t MAX_BUFFER_SIZE = 64 * 1024; ///< The maximum buffer size for a client.
        static constexpr auto OCCUPANCY_INTERVAL = std::chrono::seconds(60); ///< The interval at which to send occupancy requests.

        using clock = std::chrono::steady_clock;
        using IP = std::pair<std::array<uint8_t, 16>, uint16_t>;

        using FdsType = std::vector<network::PollFD>;
        using GameToGsType = std::unordered_map<uint32_t, IP>;
        using GsRegistryType = std::unordered_map<IP, int, pair_hash>;
        using ParseErrorsType = std::unordered_map<network::Handle, uint8_t>;
        using OccupancyCacheType = std::unordered_map<IP, uint8_t, pair_hash>;
        using SocketsMapType = std::unordered_map<std::size_t, network::Socket>;
        using GsAddrToHandleType = std::unordered_map<IP, network::Handle, pair_hash>;
        using RecvSpanType = std::unordered_map<network::Handle, std::vector<uint8_t>>;
        using SendSpanType = std::unordered_map<network::Handle, std::vector<std::vector<uint8_t>>>;
        using PendingCreatesType = std::unordered_map<network::Handle, std::pair<network::Handle, uint8_t>>;

        void _serverLoop();
        void _startServer();
        void _cleanupServer();

        void _parsePackets();
        void sendOccupancyRequests();
        void _acceptClients() noexcept;
        void _recvPackets(network::NFDS i);
        void _sendPackets(network::NFDS i);
        void _handleLoop(network::NFDS &i) noexcept;
        void _handleClients(network::NFDS &i) noexcept;
        void _handleClientsSend(network::NFDS &i) noexcept;
        void _disconnectByHandle(const network::Handle &handle) noexcept;

        void setPolloutForHandle(network::Handle h) noexcept;
        void handleGID(network::Handle handle, const uint8_t *data, size_t &offset, size_t bufsize);
        void handleJoin(network::Handle handle, const uint8_t *data, size_t &offset, size_t bufsize);
        void handleCreate(network::Handle handle, const uint8_t *data, size_t &offset, size_t bufsize);
        void handleOccupancy(network::Handle handle, const uint8_t *data, size_t &offset, size_t bufsize);
        static void handleOKKO(network::Handle handle, const uint8_t *data, size_t &offset, size_t bufsize);
        void handleGSRegistration(network::Handle handle, const uint8_t *data, size_t &offset, size_t bufsize);

        void sendErrorResponse(network::Handle handle);
        static uint32_t extractGameId(const uint8_t *data) noexcept;
        static std::vector<uint8_t> buildCreateMsg(uint8_t gametype);
        std::optional<GsRegistryType::iterator> findLeastOccupiedGS();
        [[nodiscard]] network::Handle getGSHandle(const IP &gs_key) const;
        static std::pair<IP, uint8_t> parseOccupancy(const uint8_t *data, std::size_t offset);
        [[nodiscard]] std::optional<IP> findGSKeyByHandle(network::Handle handle) const noexcept;
        static std::vector<uint8_t> buildJoinMsgForClient(const uint8_t *data, std::size_t offset);
        static std::vector<uint32_t> parseGIDs(const uint8_t *data, std::size_t start, std::size_t bufsize);
        static std::pair<std::array<uint8_t, 16>, uint16_t> parseGSKey(const uint8_t *data, std::size_t offset);
        [[nodiscard]] static std::vector<uint8_t> buildJoinMsgForGS(const std::array<uint8_t, 16> &ip, uint16_t port, uint32_t id);

        FdsType _fds;
        bool _is_init = false;
        network::NFDS _nfds = 1;
        network::Socket _sock{};
        SocketsMapType _sockets;
        SendSpanType _send_spans;
        RecvSpanType _recv_spans;
        std::size_t _next_id = 0;
        bool _is_running = false;
        GameToGsType _game_to_gs;
        GsRegistryType _gs_registry;
        ParseErrorsType _parseErrors;
        network::Endpoint _tcp_endpoint{};
        PendingCreatesType _pending_creates;
        OccupancyCacheType _occupancy_cache;
        GsAddrToHandleType _gs_addr_to_handle;
        std::atomic<bool> *_quit_server = nullptr;
};

}// namespace rtype::srv

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif
