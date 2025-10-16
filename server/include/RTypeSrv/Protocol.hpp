/**
 * @file Protocol.hpp
 * @brief R-Type Network Protocol Definitions
 *
 * This file defines the protocol constants and enumerations used for communication
 * between clients, the gateway server, and game servers in the R-Type multiplayer system.
 *
 * The protocol is divided into two main sections:
 * - GWPcol (Gateway Protocol): Communication between clients/game servers and the gateway
 * - GSPcol (Game Server Protocol): Communication between clients and game servers during gameplay
 *
 * @note All multi-byte values in the protocol are transmitted in big-endian (network) byte order.
 */

#pragma once

#include <cstdint>

// clang-format off

namespace rtype::srv {

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
 * @note Header structure: [MAGIC:2][VERSION:1][CMD:1][PAYLOAD:N]
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
     * Request: [CMD:1][GAME_ID:4]
     * - GAME_ID: 32-bit game identifier
     *
     * Success Response: [CMD:1][GAME_ID:4][IP:16][PORT:2]
     * - GAME_ID: The joined game's ID
     * - IP: IPv6 address of the game server (16 bytes)
     * - PORT: Port number (big-endian uint16)
     *
     * Failure Response: [CMD:2] (JOIN_KO)
     */
    JOIN            = 1,

    /**
     * @brief Join request failed
     *
     * Sent by gateway when a JOIN request cannot be fulfilled.
     * Possible reasons: game full, game doesn't exist, server unavailable.
     *
     * Format: [CMD:2]
     */
    JOIN_KO         = 2,

    /**
     * @brief Create a new game
     *
     * Request: [CMD:3][GAMETYPE:1]
     * - GAMETYPE: Type of game to create (see GAMETYPE enum)
     *
     * Success Response: [CMD:1][GAME_ID:4][IP:16][PORT:2]
     * - Same format as JOIN success (client automatically joins created game)
     *
     * Failure Response: [CMD:4] (CREATE_KO)
     */
    CREATE          = 3,

    /**
     * @brief Create request failed
     *
     * Sent by gateway when game creation fails.
     * Possible reasons: no available game servers, server overload.
     *
     * Format: [CMD:4]
     */
    CREATE_KO       = 4,

    /**
     * @brief Game ended notification
     *
     * Fire-and-forget notification sent by game server to gateway when a game ends.
     * No response is expected or sent.
     *
     * Format: [CMD:5][GAME_ID:4]
     * - GAME_ID: ID of the game that ended
     */
    GAME_END        = 5,

    // Reserved for future client commands (6-19)

    // ========== Game Server => Gateway Operations (20-24) ==========

    /**
     * @brief Game server registration request
     *
     * Sent by game server when it starts up to register with the gateway.
     *
     * Request: [CMD:20][IP:16][PORT:2]
     * - IP: Server's IPv6 address (16 bytes)
     * - PORT: Server's listening port (big-endian uint16)
     *
     * Success Response: [CMD:21] (GS_OK)
     * Failure Response: [CMD:22] (GS_KO)
     */
    GS              = 20,

    /**
     * @brief Game server registration successful
     *
     * Sent by gateway to confirm successful game server registration.
     * Server can now receive game creation requests.
     *
     * Format: [CMD:21]
     */
    GS_OK           = 21,

    /**
     * @brief Game server registration failed
     *
     * Sent by gateway when server registration is rejected.
     * Possible reasons: duplicate registration, invalid address, gateway overload.
     *
     * Format: [CMD:22]
     */
    GS_KO           = 22,

    /**
     * @brief Server occupancy update
     *
     * Periodic heartbeat sent by game server to report current load.
     * Used by gateway for load balancing when creating new games.
     * No response expected.
     *
     * Format: [CMD:23][OCCUPANCY:1]
     * - OCCUPANCY: (Game server only) Server current occupency
     */
    OCCUPANCY       = 23,

    /**
     * @brief Game ID registration
     *
     * Sent by game server to register game IDs it's hosting.
     * Allows gateway to route JOIN requests to the correct server.
     * No response expected.
     *
     * Format: [CMD:24][COUNT:1][GAME_ID:4]...
     * - COUNT: Number of game IDs in this packet
     * - GAME_ID: 32-bit game identifier (repeated COUNT times)
     */
    GID             = 24,
};

}

/**
 * @namespace GSPcol
 * @brief Game Server Protocol definitions
 *
 * Defines the packet types, flags, and constants for communication between
 * clients and game servers during active gameplay sessions.
 *
 * This protocol supports:
 * - Reliable and unreliable message delivery
 * - Ordered and unordered channels
 * - Packet fragmentation for large messages
 * - Optional encryption and compression
 * @note: The protocol uses a header structure: [MAGIC:2][VERSION:1][FLAGS:1][SEQ:4][ACKBASE:4][ACKBITS:1][CHANNEL:1][PACKETSIZE:2][CMD:1][PAYLOAD:N]
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
 * - First letter: R (Reliable) or U (Unreliable)
 * - Second letter: O (Ordered) or U (Unordered)
 *
 * Channels are encoded in 2 bits of the packet header.
 */
enum class CHANNEL : std::uint8_t {
    UU              = 0b00,     ///< Unreliable, Unordered (fastest, may drop/reorder)
    UO              = 0b01,     ///< Unreliable, Ordered (may drop, preserves order)
    RU              = 0b10,     ///< Reliable, Unordered (guaranteed delivery)
    RO              = 0b11,     ///< Reliable, Ordered (guaranteed delivery and order)
};

/**
 * @enum CMD
 * @brief Game server protocol command identifiers
 *
 * Packet types used during active gameplay between client and game server.
 */
enum class CMD : std::uint8_t {
    INPUT           = 1,        ///< Player input (movement, shooting, etc.)
    SNAPSHOT        = 2,        ///< Game state snapshot (entities, positions, health)
    CHAT            = 3,        ///< Chat message
    PING            = 4,        ///< Latency measurement request
    PONG            = 5,        ///< Latency measurement response
    ACK             = 6,        ///< Acknowledgment for reliable packets
    JOIN            = 7,        ///< Join game session (after gateway connection)
    KICK            = 8,        ///< Player kicked from game
    CHALLENGE       = 9,        ///< Authentication challenge
    AUTH            = 10,       ///< Authentication response
    AUTH_OK         = 11,       ///< Authentication successful
    RESYNC          = 12,       ///< Request full state resynchronization
    FRAGMENT        = 13,       ///< Fragment of a larger message
};

/**
 * @enum INPUT
 * @brief Player input types
 *
 * Defines the types of player input actions that can be sent to the game server.
 *
 * @note Currently only forward movement is defined. More input types can be added.
 */
enum class INPUT : std::uint8_t {
    FWD             = 1,        ///< Forward movement
};

}// namespace GSPcol

}// namespace rtype::srv

// clang-format on
