# R-Type Protocol Implementation

## Overview

This document describes the implementation of the R-Type network protocol as specified in the protocol documentation. The protocol uses **big-endian (network byte order)** for all multi-byte values.

## Connection Types

- **Gateway ↔ Client**: TCP
- **Gateway ↔ Game Server**: TCP
- **Game Server ↔ Client**: UDP

## Gateway Protocol (GWPcol) - TCP

### Magic Numbers

- **Gateway Protocol**: `0x4257` ('BW')
- **Game Server Protocol**: `0x4254` ('BT')

### Header Format

All gateway protocol packets include a 5-byte header:

```
[MAGIC:2][VERSION:1][FLAGS:1][CMD:1]
```

- **MAGIC**: 0x4257 (big-endian uint16)
- **VERSION**: 0b1 (uint8)
- **FLAGS**: Currently unused, reserved for future (uint8)
- **CMD**: Command identifier (uint8)

### Packet Formats

#### Client → Gateway

##### JOIN (CMD: 1)
**Request**: `[HEADER:5][CMD:1][GAME_ID:4]` (9 bytes)
- GAME_ID: Big-endian uint32

**Success Response**: `[HEADER:5][CMD:1][GAME_ID:4][IP:16][PORT:2]` (27 bytes)
- Returns game server address for client to connect via UDP

**Failure Response**: `[HEADER:5][CMD:2]` (5 bytes, JOIN_KO)

##### CREATE (CMD: 3)
**Request**: `[HEADER:5][CMD:3][GAMETYPE:1]` (6 bytes)
- GAMETYPE: 1 = RTYPE

**Success Response**: `[HEADER:5][CMD:1][GAME_ID:4][IP:16][PORT:2]` (27 bytes)
- Same as JOIN success (client automatically joins created game)

**Failure Response**: `[HEADER:5][CMD:4]` (5 bytes, CREATE_KO)

#### Game Server → Gateway

##### GS Registration (CMD: 20)
**Request**: `[HEADER:5][CMD:20][IP:16][PORT:2]` (23 bytes)
- IP: Server's IPv6 address (or IPv4-mapped IPv6)
- PORT: Server's UDP listening port (big-endian uint16)

**Success Response**: `[HEADER:5][CMD:21]` (5 bytes, GS_OK)
**Failure Response**: `[HEADER:5][CMD:22]` (5 bytes, GS_KO)

**Flow after successful registration**:
1. GS receives GS_OK
2. GS immediately sends OCCUPANCY update
3. GS sends GID list if hosting games

##### OCCUPANCY (CMD: 23)
**Format**: `[HEADER:5][CMD:23][OCCUPANCY:1]` (6 bytes)
- OCCUPANCY: Number of active games (uint8)
- Server identified by TCP connection handle
- No response expected

**Sent**:
- Immediately after receiving GS_OK
- Periodically as keepalive
- When game count changes

##### GID Registration (CMD: 24)
**Format**: `[HEADER:5][CMD:24][LEN:1][GAME_ID:4]...` (6 + LEN*4 bytes)
- LEN: Number of game IDs (uint8)
- GAME_ID: Each game ID (big-endian uint32)
- No response expected

##### GAME_END (CMD: 5)
**Format**: `[HEADER:5][CMD:5][GAME_ID:4]` (9 bytes)
- GAME_ID: ID of game that ended (big-endian uint32)
- Fire-and-forget notification
- No response expected

## IP Address Format

All IP addresses use 16 bytes:
- **IPv6**: Use full 16-byte address
- **IPv4**: Use IPv4-mapped IPv6 format

Example of `127.0.0.1` in IPv4-mapped IPv6:
```
[0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0xFF, 0xFF, 0x7F, 0x00, 0x00, 0x01]
```

## Game Server Protocol (GSPcol) - UDP

### Header Format

21-byte header for all UDP packets:

```
[MAGIC:2][VERSION:1][FLAGS:1][SEQ:4][ACKBASE:4][ACKBITS:1][CHANNEL:1][SIZE:2][ID:4][CMD:1]
```

- **MAGIC**: 0x4254 (big-endian uint16) - **DIFFERENT from gateway!**
- **VERSION**: 0b1 (uint8)
- **FLAGS**: Packet control flags (uint8)
- **SEQ**: Sequence number unique to sender (big-endian uint32)
- **ACKBASE**: Last received sequence from peer (big-endian uint32)
- **ACKBITS**: Selective ACK for 8 packets before ACKBASE (uint8)
- **CHANNEL**: Delivery guarantee (uint8)
- **SIZE**: Total packet size including header (big-endian uint16)
- **ID**: Client/player ID (big-endian uint32) - Only useful for CL->GS connections, sent to client by GS on connect
- **CMD**: Command identifier (uint8)

### FLAGS

- `F_CONN` (1 << 0): Handshake/control packet
- `F_RELIABLE` (1 << 1): Requires ACK + retransmit
- `F_FRAGMENT` (1 << 2): Fragment of larger message
- `F_PING` (1 << 3): Heartbeat/RTT probe
- `F_CLOSE` (1 << 4): Connection teardown
- `F_ENCRYPTED` (1 << 5): Payload encrypted
- `F_COMPRESSED` (1 << 6): Payload compressed

### CHANNEL

- `UU` (0b00): Unreliable, Unordered (fastest)
- `UO` (0b01): Unreliable, Ordered
- `RU` (0b10): Reliable, Unordered
- `RO` (0b11): Reliable, Ordered (strictest)

Formula: `CHANNEL = (Reliable << 1) | Ordered`

### Commands

- `CMD_INPUT` (1): Player inputs
- `CMD_SNAPSHOT` (2): Game state delta
- `CMD_CHAT` (3): Text message
- `CMD_PING` (4): RTT request
- `CMD_PONG` (5): RTT response
- `CMD_ACK` (6): Explicit acknowledgment
- `CMD_JOIN` (7): Join game session
- `CMD_KICK` (8): Kick player
- `CMD_CHALLENGE` (9): Auth challenge
- `CMD_AUTH` (10): Auth response
- `CMD_AUTH_OK` (11): Auth success
- `CMD_RESYNC` (12): Request full state
- `CMD_FRAGMENT` (13): Message fragment

### Payload Formats

- **CMD_INPUT**: `[TYPE:1][VALUE:1]...` (max 1179 bytes)
  - Do NOT use F_FRAGMENT - send multiple INPUT packets
- **CMD_SNAPSHOT**: `[SEQ:4]`
- **CMD_CHAT**: `[LEN:2][MSG:1]...` (can use F_FRAGMENT for large messages)
- **CMD_ACK**: `[SEQ:4]...` (list of sequence numbers)
- **CMD_JOIN**: `[ID:4][NONCE:1][VERSION:1]`
- **CMD_KICK**: `[MSG:1]...` (max 1179 bytes)
- **CMD_CHALLENGE**: `[COOKIE:1]...` (max 1179 bytes)
- **CMD_AUTH**: `[DECODED_CHALLENGE:1]...` (max 1179 bytes)
- **CMD_AUTH_OK**: `[ID:4][SESSION_KEY:8]` (12 bytes)
- **CMD_FRAGMENT**: `[SEQ:4][PAYLOAD:1]...`

### MTU Considerations

- Maximum packet size: 1200 bytes
- Header size: 21 bytes
- Maximum payload: 1179 bytes
- Use F_FRAGMENT flag for messages exceeding MTU

## Implementation Files

### Core Protocol

- `server/include/RTypeSrv/Protocol.hpp` - Protocol definitions and documentation
- `server/include/RTypeSrv/GatewayPacketParser.hpp` - Packet parsing interface
- `server/src/Gateway/PacketParser.cpp` - Parsing implementations

### Command Handlers

- `server/src/Gateway/Commands/GS.cpp` - Game server registration
- `server/src/Gateway/Commands/CREATE.cpp` - Game creation
- `server/src/Gateway/Commands/JOIN.cpp` - Join requests
- `server/src/Gateway/Commands/OCCUPANCY.cpp` - Occupancy updates
- `server/src/Gateway/Commands/GID.cpp` - Game ID registration
- `server/src/Gateway/Commands/GCMD_GAME_END.cpp` - Game end notifications
- `server/src/Gateway/Commands/OK.cpp` - OK responses
- `server/src/Gateway/Commands/KO.cpp` - KO responses

### Endianness Handling

All multi-byte values use big-endian (network byte order). The implementation includes:

- `getNextVal<T>()` - Reads big-endian values from buffer
- `pushValInBuffer<T>()` - Writes big-endian values to buffer

These functions work correctly regardless of host architecture.

## Typical Flows

### Game Server Registration

1. GS → GW: `GCMD_GS` with IP:PORT
2. GW → GS: `GCMD_GS_OK` or `GCMD_GS_KO`
3. If OK: GS → GW: `GCMD_OCCUPANCY`
4. If GS has games: GS → GW: `GCMD_GID` with game list

### Client Creates Game

1. CL → GW: `GCMD_CREATE` with GAMETYPE
2. GW broadcasts occupancy check (if needed)
3. GW selects least occupied server
4. GW → GS: `GCMD_CREATE` with GAMETYPE
5. GS → GW: `GCMD_JOIN` with GAME_ID:IP:PORT or `GCMD_CREATE_KO`
6. If success:
   - GW → CL: `GCMD_JOIN` with GAME_ID:IP:PORT
   - CL → GS: UDP auth procedure
7. If failure:
   - GW → CL: `GCMD_CREATE_KO`
   - GW evicts server from cache

### Client Joins Game

1. CL → GW: `GCMD_JOIN` with GAME_ID
2. GW checks game routing table
3. GW → CL: `GCMD_JOIN` with GAME_ID:IP:PORT or `GCMD_JOIN_KO`
4. If success: CL → GS: UDP auth procedure

### Game End

1. GS → GW: `GCMD_GAME_END` with GAME_ID (broadcast to all connected gateways)
2. GW removes game from routing table
3. No response sent

### Client → Game Server Connection

1. CL → GS: `CMD_JOIN` with ID:NONCE:VERSION
2. GS → CL: `CMD_CHALLENGE` with COOKIE or `CMD_KICK`
3. If challenge: CL → GS: `CMD_AUTH` with decoded challenge
4. GS → CL: `CMD_AUTH_OK` with ID:SESSION_KEY or `CMD_KICK`

## Error Handling

- Gateway tracks parse errors per connection
- After MAX_PARSE_ERRORS (3), connection is terminated
- Invalid packets throw runtime_error caught by parse loop
- Unregistered servers sending data result in errors

## Constants

- `GWPCOL_MAGIC`: 0x4257
- `GSPCOL_MAGIC`: 0x4254
- `MINIMUM_VERSION`: 0b1
- `MAXIMUM_VERSION`: 0b1
- `MAX_PARSE_ERRORS`: 3
- `MAX_BUFFER_SIZE`: 64KB
- `OCCUPANCY_INTERVAL`: 60 seconds

## Notes

- All multi-byte values MUST be big-endian
- Game servers identified by TCP connection handle (not IP:PORT in every packet)
- UDP protocol (GSPcol) is documented but not implemented in this gateway server
- Game servers implement the UDP protocol for client connections
