/**
 * @file Protocol.hpp
 * @brief R-Type Network Protocol Definitions
 *
 * This file defines the protocol constants and enumerations used for communication
 * between clients, the gateway server, and game servers in the R-Type multiplayer system.
 *
 * The protocol is divided into two main sections:
 * - GWPcol (Gateway Protocol): Communication between clients/game servers and the gateway over TCP
 * - GSPcol (Game Server Protocol): Communication between clients and game servers during gameplay over UDP
 *
 * @note All multi-byte values in the protocol are transmitted in big-endian (network) byte order.
 *
 * Connection Types:
 * - GW <=> CL : TCP
 * - GW <=> GS : TCP
 * - GS <=> CL : UDP
 */

#pragma once

#include <cstdint>

// clang-format off

namespace rtype::srv {

/**
 * @brief Gateway Protocol (TCP) Magic Number
 *
 * Magic number for gateway protocol packets: 0x42 0x57 ('BW')
 * Used to identify valid gateway protocol packets.
 */
constexpr uint16_t GWPCOL_MAGIC = 0x4257;

/**
 * @brief Game Server Protocol (UDP) Magic Number
 *
 * Magic number for game server protocol packets: 0x42 0x54 ('BT')
 * Used to identify valid game server protocol packets.
 * @note This is DIFFERENT from the gateway protocol magic number.
 */
constexpr uint16_t GSPCOL_MAGIC = 0x4254;

/**
 * @enum GAMETYPE
 * @brief Supported game types for R-Type server
 *
 * Defines the available game modes that can be created and joined.
 */
enum class GAMETYPE : std::uint8_t {
    RTYPE           = 1,    ///< Classic R-Type game mode
};

/**
 * @namespace GWPcol
 * @brief Gateway Protocol definitions
 *
 * Defines the packet types and constants for communication between:
 * - Clients <=> Gateway (game management: create, join)
 * - Game Servers <=> Gateway (registration, status updates)
 *
 * Connection Type: TCP
 *
 * @note Header structure: [MAGIC:2][VERSION:1][FLAGS:1][CMD:1][PAYLOAD:N]
 * - MAGIC: 0x4257 (big-endian uint16)
 * - VERSION: 0b1 (uint8)
 * - FLAGS: Currently unused, reserved for future use (uint8)
 * - CMD: Command identifier (uint8, see CMD enum)
 * - PAYLOAD: Variable length, depends on command
 */
namespace GWPcol {

/**
 * @enum CMD
 * @brief Gateway protocol command identifiers
 *
 * Packet types used in the gateway protocol. Commands are divided into:
 * - Client operations (1-5): Game session management
 * - Game Server operations (20-24): Server registration and monitoring
 *
 * @note Command IDs 6-19 are reserved for future client commands
 */
enum class CMD : std::uint8_t {
    // ========== Client → Gateway Operations (1-5) ==========

    /**
     * @brief Join an existing game
     *
     * Client Request: [MAGIC:2][VERSION:1][FLAGS:1][CMD:1][GAME_ID:4]
     * Total size: 9 bytes
     * - GAME_ID: 32-bit game identifier (big-endian)
     *
     * Success Response: [MAGIC:2][VERSION:1][FLAGS:1][CMD:1][GAME_ID:4][IP:16][PORT:2]
     * Total size: 27 bytes
     * - GAME_ID: The joined game's ID
     * - IP: IPv6 address of the game server (16 bytes, see IP address format below)
     * - PORT: Port number (big-endian uint16)
     *
     * Failure Response: [MAGIC:2][VERSION:1][FLAGS:1][CMD:2]
     * Total size: 5 bytes (JOIN_KO)
     *
     * IP Address Format:
     * - For IPv6: Use full 16-byte address
     * - For IPv4: Use IPv4-mapped IPv6 address format
     *   Example: 127.0.0.1 = [0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0x7F,0x00,0x00,0x01]
     */
    JOIN            = 1,

    /**
     * @brief Join request failed
     *
     * Sent by gateway when a JOIN request cannot be fulfilled.
     * Possible reasons: game full, game doesn't exist, server unavailable.
     *
     * Format: [MAGIC:2][VERSION:1][FLAGS:1][CMD:2]
     * Total size: 5 bytes
     */
    JOIN_KO         = 2,

    /**
     * @brief Create a new game
     *
     * Client Request: [MAGIC:2][VERSION:1][FLAGS:1][CMD:3][GAMETYPE:1]
     * Total size: 6 bytes
     * - GAMETYPE: Type of game to create (see GAMETYPE enum)
     *
     * Success Response: [MAGIC:2][VERSION:1][FLAGS:1][CMD:1][GAME_ID:4][IP:16][PORT:2]
     * Total size: 27 bytes (same as JOIN success - client automatically joins created game)
     *
     * Failure Response: [MAGIC:2][VERSION:1][FLAGS:1][CMD:4]
     * Total size: 5 bytes (CREATE_KO)
     */
    CREATE          = 3,

    /**
     * @brief Create request failed
     *
     * Sent by gateway when game creation fails.
     * Possible reasons: no available game servers, server overload.
     *
     * Format: [MAGIC:2][VERSION:1][FLAGS:1][CMD:4]
     * Total size: 5 bytes
     */
    CREATE_KO       = 4,

    /**
     * @brief Game ended notification
     *
     * Fire-and-forget notification sent by game server to gateway when a game ends.
     * No response is expected or sent.
     *
     * Format: [MAGIC:2][VERSION:1][FLAGS:1][CMD:5][GAME_ID:4]
     * Total size: 9 bytes
     * - GAME_ID: ID of the game that ended (big-endian uint32)
     */
    GAME_END        = 5,

    // Reserved for future client commands (6-19)

    // ========== Game Server => Gateway Operations (20-24) ==========

    /**
     * @brief Game server registration request
     *
     * Sent by game server when it starts up to register with the gateway.
     *
     * Request: [MAGIC:2][VERSION:1][FLAGS:1][CMD:20][IP:16][PORT:2]
     * Total size: 23 bytes
     * - IP: Server's IPv6 address (16 bytes, see IP address format in JOIN)
     * - PORT: Server's listening port (big-endian uint16)
     *
     * Success Response: [MAGIC:2][VERSION:1][FLAGS:1][CMD:21]
     * Total size: 5 bytes (GS_OK)
     *
     * Failure Response: [MAGIC:2][VERSION:1][FLAGS:1][CMD:22]
     * Total size: 5 bytes (GS_KO)
     *
     * Flow after successful registration:
     * 1. GS receives GS_OK
     * 2. GS immediately sends OCCUPANCY update
     * 3. GS sends GID list if hosting games
     */
    GS              = 20,

    /**
     * @brief Game server registration successful
     *
     * Sent by gateway to confirm successful game server registration.
     * Server can now receive game creation requests.
     *
     * Format: [MAGIC:2][VERSION:1][FLAGS:1][CMD:21]
     * Total size: 5 bytes
     */
    GS_OK           = 21,

    /**
     * @brief Game server registration failed
     *
     * Sent by gateway when server registration is rejected.
     * Possible reasons: duplicate registration, invalid address, gateway overload.
     *
     * Format: [MAGIC:2][VERSION:1][FLAGS:1][CMD:22]
     * Total size: 5 bytes
     *
     * If GS_KO is received and server has no other gateways:
     * - Server ends all active games
     * - Retries registration
     * - If still fails or has no games, server stops
     */
    GS_KO           = 22,

    /**
     * @brief Server occupancy update
     *
     * Periodic heartbeat sent by game server to report current load.
     * Used by gateway for load balancing when creating new games.
     * Gateway identifies server by TCP connection handle.
     * No response expected.
     *
     * Format: [MAGIC:2][VERSION:1][FLAGS:1][CMD:23][OCCUPANCY:1]
     * Total size: 6 bytes
     * - OCCUPANCY: Number of active games currently running on this server (uint8)
     *
     * This packet is sent:
     * - Immediately after receiving GS_OK
     * - Periodically as a keepalive
     * - Whenever game count changes
     */
    OCCUPANCY       = 23,

    /**
     * @brief Game ID registration
     *
     * Sent by game server to register game IDs it's hosting.
     * Allows gateway to route JOIN requests to the correct server.
     * No response expected.
     *
     * Format: [MAGIC:2][VERSION:1][FLAGS:1][CMD:24][LEN:1][GAME_ID:4]...
     * Total size: 6 + (LEN * 4) bytes
     * - LEN: Number of game IDs in this packet (uint8)
     * - GAME_ID: 32-bit game identifier (big-endian uint32, repeated LEN times)
     *
     * Note: This is sent after registration when server already has active games
     */
    GID             = 24,
};

}

/**
 * @namespace GSPcol
 * @brief Game Server Protocol definitions
 *
 * Defines the packet types, flags, and constants for communication between
 * clients and game servers during active gameplay sessions over UDP.
 *
 * This protocol supports:
 * - Reliable and unreliable message delivery
 * - Ordered and unordered channels
 * - Packet fragmentation for large messages
 * - Optional encryption and compression
 * - Selective acknowledgment (SACK) for reliable channels
 *
 * Connection Type: UDP
 *
 * @note Header structure: [MAGIC:2][VERSION:1][FLAGS:1][SEQ:4][ACKBASE:4][ACKBITS:1][CHANNEL:1][SIZE:2][ID:4][CMD:1][PAYLOAD:N]
 * Total header size: 21 bytes
 * - MAGIC: 0x4254 (big-endian uint16) - DIFFERENT from gateway protocol!
 * - VERSION: 0b1 (uint8)
 * - FLAGS: Packet control flags (uint8, see FLAGS enum)
 * - SEQ: Sequence number unique to sender (big-endian uint32)
 * - ACKBASE: Sequence number of last received packet from peer (big-endian uint32)
 * - ACKBITS: Selective ACK for 8 packets before ACKBASE (uint8)
 * - CHANNEL: Delivery guarantee channel (uint8, see CHANNEL enum)
 * - SIZE: Total packet size including header (big-endian uint16)
 * - ID: Client/player ID (big-endian uint32) - Only useful for CL->GS, sent by GS on connect
 * - CMD: Command identifier (uint8, see CMD enum)
 * - PAYLOAD: Variable length (SIZE - 21 bytes)
 *
 * Maximum packet size: 1200 bytes (to respect MTU)
 * Maximum payload: 1200 - 21 = 1179 bytes
 *
 * AckBits encoding example:
 * If ACKBASE = 1005 and we received all packets except 1002:
 * AckBits = 0b11110111 (bits represent packets 998-1004, LSB = 998)
 * - Bit 0 (LSB): packet 998 received
 * - Bit 1: packet 999 received
 * - Bit 2: packet 1000 received
 * - Bit 3: packet 1001 received
 * - Bit 4: packet 1002 NOT received
 * - Bit 5: packet 1003 received
 * - Bit 6: packet 1004 received
 * - Bit 7 (MSB): (unused in this 8-bit window)
 */
namespace GSPcol {

/**
 * @enum FLAGS
 * @brief Packet header flags
 *
 * Bit flags that modify packet handling and delivery guarantees.
 * Multiple flags can be combined using bitwise OR.
 *
 * @note Flags are stored in a single uint8_t byte in the packet header
 */
enum class FLAGS : std::uint8_t {
    CONN            = 1 << 0,   ///< Connection establishment packet
    RELIABLE        = 1 << 1,   ///< Requires acknowledgment and retransmission on loss
    FRAGMENT        = 1 << 2,   ///< Packet is part of a fragmented message
    PING            = 1 << 3,   ///< Latency measurement packet
    CLOSE           = 1 << 4,   ///< Connection termination packet
    ENCRYPTED       = 1 << 5,   ///< Payload is encrypted
    COMPRESSED      = 1 << 6,   ///< Payload is compressed
};

/**
 * @enum CHANNEL
 * @brief Message delivery channels
 *
 * Defines the delivery guarantees for packets:
 * - First bit (bit 1): R (Reliable=1) or U (Unreliable=0)
 * - Second bit (bit 0): O (Ordered=1) or U (Unordered=0)
 *
 * Formula: CHANNEL = (Reliable << 1) | Ordered
 *
 * Channels are encoded in 2 bits, but stored in a uint8 for simplicity.
 * Each channel may have its own send queue for proper ordering/reliability.
 */
enum class CHANNEL : std::uint8_t {
    UU              = 0b00,     ///< Unreliable, Unordered (fastest, may drop/reorder)
    UO              = 0b01,     ///< Unreliable, Ordered (may drop, preserves order of delivered packets)
    RU              = 0b10,     ///< Reliable, Unordered (guaranteed delivery, any order)
    RO              = 0b11,     ///< Reliable, Ordered (guaranteed delivery and order - strictest)
};

/**
 * @enum CMD
 * @brief Game server protocol command identifiers
 *
 * Packet types used during active gameplay between client and game server.
 *
 * Payload formats:
 * - CMD_INPUT: [TYPE:1][VALUE:1]... (multiple pairs, max 1179 bytes total)
 *   Do NOT use F_FRAGMENT for inputs - send multiple INPUT packets instead
 * - CMD_SNAPSHOT: [SEQ:4] (sequence number of state snapshot)
 * - CMD_CHAT: [LEN:2][MSG:1]... (LEN = message length, MSG = UTF-8 text)
 *   Can exceed 1200 bytes - use F_FRAGMENT flag for large messages
 * - CMD_PING: No payload
 * - CMD_PONG: No payload
 * - CMD_ACK: [SEQ:4]... (list of sequence numbers being acknowledged)
 * - CMD_JOIN: [ID:4][NONCE:1][VERSION:1] (client auth request to game server)
 * - CMD_KICK: [MSG:1]... (kick reason text, max 1179 bytes)
 * - CMD_CHALLENGE: [TIMESTAMP:8][COOKIE:32] (40 bytes) — server → client stateless cookie challenge
 * - CMD_AUTH: [NONCE:1][COOKIE:32] (33 bytes) — client → server authentication response
 * - CMD_AUTH_OK: [ID:4][SESSION_KEY:32] (successful auth, 36 bytes)
 * - CMD_RESYNC: No payload (request full state)
 * - CMD_FRAGMENT: [SEQ:4][PAYLOAD:1]... (fragment sequence + fragment data)
 */
enum class CMD : std::uint8_t {
    INPUT           = 1,        ///< Player input (movement, shooting, etc.)
    SNAPSHOT        = 2,        ///< Game state snapshot (entities, positions, health)
    CHAT            = 3,        ///< Chat message (can exceed 1200 bytes, use F_FRAGMENT)
    PING            = 4,        ///< Latency measurement request
    PONG            = 5,        ///< Latency measurement response
    ACK             = 6,        ///< Explicit acknowledgment for reliable packets
    JOIN            = 7,        ///< Join game session (after gateway redirects client here)
    KICK            = 8,        ///< Player kicked from game
    CHALLENGE       = 9,        ///< Authentication challenge (server -> client)
    AUTH            = 10,       ///< Authentication response (client -> server)
    AUTH_OK         = 11,       ///< Authentication successful (server -> client)
    RESYNC          = 12,       ///< Request full state resynchronization after desync
    FRAGMENT        = 13,       ///< Fragment of a larger message (use with F_FRAGMENT flag)
};

/**
 * @enum INPUT
 * @brief Player input types
 *
 * Defines the types of player input actions that can be sent to the game server.
 * Used in CMD_INPUT payload as TYPE:VALUE pairs.
 *
 * @note More input types (shoot, special, etc.) can be added as needed.
 */
enum class INPUT : std::uint8_t {
    FWD             = 1,        ///< Forward movement input
};

}// namespace GSPcol

}// namespace rtype::srv

// clang-format on
