#pragma once

#include <RTypeSrv/Protocol.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rtype::srv {

/**
 * @brief Helper class for building and parsing Game Server Protocol (UDP) packets.
 *
 * This class provides static methods to build and parse UDP packets according to
 * the Game Server Protocol specification (GSPcol).
 *
 * @note All multi-byte values are transmitted in big-endian (network) byte order
 */
class GameServerUDPPacketParser final
{
    public:
        /**
         * @brief Validates and parses a UDP packet header.
         *
         * Header format (21 bytes total):
         * [MAGIC:2][VERSION:1][FLAGS:1][SEQ:4][ACKBASE:4][ACKBITS:1][CHANNEL:1][SIZE:2][ID:4][CMD:1]
         *
         * @param data Pointer to packet data
         * @param offset Current position in buffer (will be advanced past header)
         * @param bufsize Total size of buffer
         * @return Command byte from header
         * @throws std::runtime_error If header is invalid or incomplete
         */
        static std::uint8_t parseHeader(const uint8_t *data, std::size_t &offset, std::size_t bufsize);

        /**
         * @brief Creates a complete UDP packet header.
         *
         * @param cmd Command identifier
         * @param flags Control flags
         * @param seq Sequence number
         * @param ackBase Base sequence for selective ACK
         * @param ackBits SACK bits for previous 8 packets
         * @param channel Delivery channel
         * @param size Total packet size including header
         * @param clientId Client/Player ID
         * @return Vector containing the 21-byte header
         */
        static std::vector<uint8_t> buildHeader(GSPcol::CMD cmd, GSPcol::FLAGS flags, uint32_t seq, uint32_t ackBase, uint8_t ackBits,
            GSPcol::CHANNEL channel, uint16_t size, uint32_t clientId);

        /**
         * @brief Builds a PONG response packet.
         *
         * Format: [HEADER:21]
         * Total size: 21 bytes
         * Used to respond to PING requests for latency measurement.
         *
         * @param seq Current sequence number
         * @param ackBase Last received sequence from peer
         * @param ackBits SACK bitfield
         * @param clientId Client ID to respond to
         * @return Vector containing complete PONG packet
         */
        static std::vector<uint8_t> buildPongResponse(uint32_t seq, uint32_t ackBase, uint8_t ackBits, uint32_t clientId);

        /**
         * @brief Builds a SNAPSHOT packet containing game state.
         *
         * Format: [HEADER:21][SNAPSHOT_SEQ:4][STATE_DATA:N]
         * Uses reliable ordered delivery channel.
         *
         * @param seq Packet sequence number
         * @param ackBase Last received sequence
         * @param ackBits SACK bitfield
         * @param clientId Target client
         * @param snapshotSeq Game state sequence number
         * @param stateData Serialized game state
         * @return Vector containing complete snapshot packet
         */
        static std::vector<uint8_t> buildSnapshot(uint32_t seq, uint32_t ackBase, uint8_t ackBits, uint32_t clientId, uint32_t snapshotSeq,
            const std::vector<uint8_t> &stateData);

        /**
         * @brief Build an authentication challenge packet.
         *
         * Format: [HEADER:21][CHALLENGE:32]
         * Uses reliable ordered delivery with encryption flag.
         *
         * @param seq Current sequence number
         * @param ackBase Last received sequence
         * @param ackBits SACK bitfield
         * @param clientId Target client ID
         * @param challenge 32-byte random challenge data
         * @return Vector containing complete challenge packet
         */
        static std::vector<uint8_t> buildChallenge(uint32_t seq, uint32_t ackBase, uint8_t ackBits, uint32_t clientId,
            const std::array<uint8_t, 32> &challenge);

        /**
         * @brief Build an authentication challenge containing a timestamp and server-generated cookie.
         *
         * Format: [HEADER:21][TIMESTAMP:8][COOKIE:32]
         * Total payload size: 40 bytes
         */
        static std::vector<uint8_t> buildChallengeWithCookie(uint32_t seq, uint32_t ackBase, uint8_t ackBits, uint32_t clientId,
            uint64_t timestamp, const std::array<uint8_t, 32> &cookie);

        /**
         * @brief Build a fragment of a larger message.
         *
         * Format: [HEADER:21][BASE_SEQ:4][TOTAL_SIZE:4][FRAGMENT_OFFSET:4][FRAGMENT_DATA:N]
         *
         * @param seq Current sequence number
         * @param ackBase Last received sequence
         * @param ackBits SACK bitfield
         * @param clientId Target client ID
         * @param baseSeq Base sequence number of the complete message
         * @param totalSize Total size of the complete message
         * @param offset Offset of this fragment in the complete message
         * @param fragmentData This fragment's data
         * @return Vector containing the fragment packet
         */
        static std::vector<uint8_t> buildFragment(uint32_t seq, uint32_t ackBase, uint8_t ackBits, uint32_t clientId, uint32_t baseSeq,
            uint32_t totalSize, uint32_t offset, const std::vector<uint8_t> &fragmentData);

        /**
         * @brief Build an AUTH_OK packet for successful authentication.
         *
         * Format: [HEADER:21][ID:4][SESSION_KEY:8]
         * Total size: 33 bytes
         *
         * @param seq Current sequence number
         * @param ackBase Last received sequence
         * @param ackBits SACK bitfield
         * @param clientId Target client ID
         * @param sessionKey 8-byte session key
         * @return Vector containing complete AUTH_OK packet
         */
        static std::vector<uint8_t> buildAuthOkPacket(uint32_t seq, uint32_t ackBase, uint8_t ackBits, uint32_t clientId,
            const std::array<uint8_t, 8> &sessionKey);

        static constexpr uint16_t HEADER_MAGIC = GSPCOL_MAGIC;
        static constexpr uint8_t VERSION = 0x01;
        static constexpr uint16_t MAX_PACKET_SIZE = 1200;
        static constexpr uint16_t HEADER_SIZE = 21;
        static constexpr uint16_t MAX_PAYLOAD_SIZE = MAX_PACKET_SIZE - HEADER_SIZE;
};

}// namespace rtype::srv
