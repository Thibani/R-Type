# R-Type Network Protocol — Version 1

| Status   | Draft                       |
|----------|-----------------------------|
| Version  | 1                           |
| Transport| UDP only                    |
| Issue    | #21 (design), #10 (documentation) |

This document specifies the binary protocol spoken between `r-type_client`
and `r-type_server`. It is meant to be complete enough to write a new client
for the server without reading its source code.

The key words **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT** and **MAY**
are to be interpreted as described in [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119).

---

## 1. Overview

- **Transport.** Every message, including connection and disconnection, is a
  single UDP datagram. The protocol uses no TCP connection.
- **Authority.** The server owns the game state. Clients only send *inputs*;
  the server simulates, then sends *snapshots* (continuous state) and
  *events* (discrete facts: spawns, deaths, players joining or leaving).
- **Reliability.** UDP is unreliable. Snapshots are sent unreliably: a lost
  snapshot is simply superseded by the next one. Events are sent with the
  `RELIABLE` flag and are retransmitted until acknowledged, then delivered in
  order (see [§6](#6-reliability-and-ordering)).
- **Robustness.** Every field has a fixed size and every count is bounded.
  A receiver validates a datagram completely before acting on it, and drops
  anything malformed (see [§7](#7-validation-rules)).

```text
 Client                                   Server
   | -- CONNECT_REQUEST ------------------> |
   | <------------------- CONNECT_ACCEPT -- |
   | <-------- PLAYER_JOINED (reliable) --- |  one per player already in game
   | <-------- ENTITY_SPAWN  (reliable) --- |  one per existing entity
   |                                        |
   | -- INPUT (every client frame) -------> |
   | <------------ SNAPSHOT (20-30 Hz) ---- |
   | <------- events (reliable) ----------- |
   |                                        |
   | -- DISCONNECT -----------------------> |  or silence => timeout
```

---

## 2. Encoding rules

| Rule            | Value                                                         |
|-----------------|---------------------------------------------------------------|
| Byte order      | Big-endian (network byte order) for every multi-byte field    |
| Integers        | `u8/u16/u32` unsigned, `i16` two's complement signed          |
| Floating point  | Never sent. Real values are quantized to integers (below)     |
| Strings         | Fixed 16-byte UTF-8 field, zero-padded, no length prefix      |
| Struct layout   | Fields are packed back-to-back: no padding, no alignment      |
| Max datagram    | **1200 bytes** (header + payload), safe under common MTUs     |

Implementations **MUST** serialize field by field. Copying a C++ struct with
`memcpy` is not allowed: padding and byte order depend on the compiler and
the CPU, which would break cross-platform play.

### 2.1 Quantized values

| Value      | Wire type | Unit                       | Range                    |
|------------|-----------|----------------------------|--------------------------|
| Position   | `i16`     | 1/4 world unit             | −8192.00 … +8191.75      |
| Velocity   | `i16`     | 1/4 world unit per second  | −8192.00 … +8191.75 u/s  |

The world is a logical **1920 × 1080** area. The origin is the top-left
corner, `x` grows to the right, `y` grows downward. Entities may exist outside
that area (spawning off-screen on the right, for example).

Encoding: `wire = round(value * 4)`, clamped to the `i16` range.
Decoding: `value = wire / 4.0`.

### 2.2 Strings

Player names use a fixed 16-byte field holding UTF-8 text, padded with `0x00`.
A name **MUST NOT** contain `0x00` before its padding, and **MUST** be valid
UTF-8. A name of 16 bytes has no terminator.

---

## 3. Packet header

Every datagram starts with this 20-byte header.

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-------------------------------+---------------+---------------+
|          magic (0x5254)       |    version    |     type      |
+---------------+---------------+-------------------------------+
|     flags     |   reserved    |          payloadSize          |
+---------------+---------------+-------------------------------+
|                         sessionToken                          |
+-------------------------------+-------------------------------+
|            sequence           |              ack              |
+-------------------------------+-------------------------------+
|                            ackBits                            |
+---------------------------------------------------------------+
|                     payload (payloadSize bytes)               |
```

| Offset | Size | Field          | Description                                                  |
|-------:|-----:|----------------|--------------------------------------------------------------|
| 0      | 2    | `magic`        | Always `0x5254` (`"RT"`). Filters out stray traffic.          |
| 2      | 1    | `version`      | Protocol version. This document describes version `1`.       |
| 3      | 1    | `type`         | Packet type, see [§4](#4-packet-types).                       |
| 4      | 1    | `flags`        | Bit field, see below.                                         |
| 5      | 1    | `reserved`     | **MUST** be `0`.                                              |
| 6      | 2    | `payloadSize`  | Number of payload bytes after the header.                     |
| 8      | 4    | `sessionToken` | Session token from `CONNECT_ACCEPT`. `0` before it is known.  |
| 12     | 2    | `sequence`     | Sequence number of this packet, see [§6](#6-reliability-and-ordering). |
| 14     | 2    | `ack`          | Most recent `sequence` received from the peer.                |
| 16     | 4    | `ackBits`      | Bit `i` set ⇔ packet `ack − 1 − i` was received.              |

### 3.1 Flags

| Bit | Name       | Meaning                                                              |
|----:|------------|----------------------------------------------------------------------|
| 0   | `RELIABLE` | The payload starts with a `u16 reliableId` and must be acknowledged. |
| 1   | `HAS_ACK`  | `ack` and `ackBits` are meaningful. Clear until a packet is received.|
| 2–7 | —          | Reserved, **MUST** be `0` in version 1.                              |

### 3.2 Versioning

- The `version` byte must match exactly. Version 1 peers do not negotiate.
- Any change to an existing layout bumps the version.
- A `CONNECT_REQUEST` with a different version is answered with
  `CONNECT_REJECT (VERSION_MISMATCH)`, sent with the server's own version.
  Every other packet with a different version is dropped silently.
- Reserved bits and fields leave room for later extensions (compression,
  delta snapshots) without changing the header size.

---

## 4. Packet types

Sizes are payload sizes, header excluded. Reliable packets include their
2-byte `reliableId` in the size.

| Type   | Name              | Direction | Reliable | Payload size       |
|--------|-------------------|-----------|----------|--------------------|
| `0x01` | `CONNECT_REQUEST` | C → S     | no       | 16                 |
| `0x02` | `CONNECT_ACCEPT`  | S → C     | no       | 4                  |
| `0x03` | `CONNECT_REJECT`  | S → C     | no       | 1                  |
| `0x04` | `DISCONNECT`      | both      | no       | 1                  |
| `0x05` | `HEARTBEAT`       | both      | no       | 0                  |
| `0x10` | `INPUT`           | C → S     | no       | 6 … 13 (`5 + count`) |
| `0x20` | `SNAPSHOT`        | S → C     | no       | `11 + 13 × count`  |
| `0x21` | `ENTITY_SPAWN`    | S → C     | **yes**  | 14                 |
| `0x22` | `ENTITY_DESTROY`  | S → C     | **yes**  | 7                  |
| `0x23` | `PLAYER_JOINED`   | S → C     | **yes**  | 19                 |
| `0x24` | `PLAYER_LEFT`     | S → C     | **yes**  | 4                  |
| `0x25` | `PLAYER_DIED`     | S → C     | **yes**  | 5                  |
| `0x26` | `GAME_STATE`      | S → C     | **yes**  | 7                  |

Ranges `0x00`, `0x06–0x0F`, `0x11–0x1F` and `0x27–0xFF` are reserved.

### 4.1 Connection

#### `CONNECT_REQUEST` (`0x01`) — C → S

| Offset | Size | Field  | Description                    |
|-------:|-----:|--------|--------------------------------|
| 0      | 16   | `name` | Player name ([§2.2](#22-strings)) |

`sessionToken` **MUST** be `0`.

#### `CONNECT_ACCEPT` (`0x02`) — S → C

| Offset | Size | Field          | Description                                   |
|-------:|-----:|----------------|-----------------------------------------------|
| 0      | 1    | `playerId`     | `0`–`3`. Also selects the ship colour.        |
| 1      | 1    | `tickRate`     | Server simulation rate in Hz (e.g. `60`).     |
| 2      | 1    | `snapshotRate` | Snapshots per second (e.g. `30`).             |
| 3      | 1    | `maxPlayers`   | Players per game (`4`).                       |

The header's `sessionToken` carries the new, non-zero session token. The
client **MUST** put it in every following packet.

#### `CONNECT_REJECT` (`0x03`) — S → C

| Offset | Size | Field    | Values                                                            |
|-------:|-----:|----------|-------------------------------------------------------------------|
| 0      | 1    | `reason` | `1` SERVER_FULL, `2` VERSION_MISMATCH, `3` GAME_IN_PROGRESS, `4` INVALID_NAME |

#### `DISCONNECT` (`0x04`) — both directions

| Offset | Size | Field    | Values                                                                 |
|-------:|-----:|----------|------------------------------------------------------------------------|
| 0      | 1    | `reason` | `0` QUIT, `1` TIMEOUT, `2` KICKED, `3` SERVER_SHUTDOWN, `4` PROTOCOL_ERROR |

No acknowledgement is expected: the sender **SHOULD** send it 3 times in a row
then forget the session.

#### `HEARTBEAT` (`0x05`) — both directions

Empty payload. Keeps the session alive and carries acknowledgements when the
sender has nothing else to send.

### 4.2 Client → server

#### `INPUT` (`0x10`)

| Offset | Size    | Field       | Description                                       |
|-------:|--------:|-------------|---------------------------------------------------|
| 0      | 4       | `firstTick` | Client input tick of `masks[0]`.                  |
| 4      | 1       | `count`     | Number of masks, `1`–`8`.                         |
| 5      | `count` | `masks`     | One `u8` per consecutive tick, oldest first.      |

Input mask bits:

| Bit | Meaning | Bit | Meaning |
|----:|---------|----:|---------|
| 0   | UP      | 3   | RIGHT   |
| 1   | DOWN    | 4   | FIRE    |
| 2   | LEFT    | 5–7 | reserved, **MUST** be `0` |

Each packet repeats the last few inputs (up to 8) so a lost datagram costs
nothing: the next one carries the missing ticks again. The server applies only
ticks newer than the last one it processed for that client, and ignores the
others.

The client sends `INPUT` once per input tick, even when no key is pressed.

### 4.3 Server → client

#### `SNAPSHOT` (`0x20`) — unreliable

| Offset | Size | Field              | Description                                         |
|-------:|-----:|--------------------|-----------------------------------------------------|
| 0      | 4    | `serverTick`       | Simulation tick this snapshot describes.            |
| 4      | 4    | `lastInputTick`    | Last input tick of *this* client the server applied.|
| 8      | 1    | `partIndex`        | `0` … `partCount − 1`.                              |
| 9      | 1    | `partCount`        | `≥ 1`. Large states are split over several packets. |
| 10     | 1    | `count`            | Number of entity states that follow.                |
| 11     | 13 × `count` | `entities` | Entity states, layout below.                    |

Entity state (13 bytes):

| Offset | Size | Field    | Description                                 |
|-------:|-----:|----------|---------------------------------------------|
| 0      | 2    | `netId`  | Network entity id.                          |
| 2      | 1    | `kind`   | Entity kind ([§5](#5-identifiers-and-enumerations)). |
| 3      | 1    | `owner`  | `playerId` `0`–`3`, or `0xFF` for none.     |
| 4      | 1    | `health` | Remaining health points.                    |
| 5      | 2    | `x`      | Position, quantized ([§2.1](#21-quantized-values)). |
| 7      | 2    | `y`      |                                             |
| 9      | 2    | `vx`     | Velocity, quantized.                        |
| 11     | 2    | `vy`     |                                             |

At most `(1200 − 20 − 11) / 13 = 89` entities fit in one part.

A client **MUST** ignore a snapshot whose `serverTick` is older than the
newest one it applied (it arrived out of order). Parts of the same tick are
independent: each one can be applied as soon as it arrives.

A snapshot only *updates* entities. Creation and destruction are decided by
the `ENTITY_SPAWN` and `ENTITY_DESTROY` events. If a snapshot mentions an
unknown `netId` (its spawn event is still on its way), the client **MAY**
create the entity from the snapshot data.

#### Reliable events

Each reliable payload starts with `reliableId (u16)`, shown at offset 0.

`ENTITY_SPAWN` (`0x21`) — an entity appears. A missile spawn with an `owner`
is also how "player/monster fires" is notified.

| Offset | Size | Field        |
|-------:|-----:|--------------|
| 0      | 2    | `reliableId` |
| 2      | 2    | `netId`      |
| 4      | 1    | `kind`       |
| 5      | 1    | `owner`      |
| 6      | 2    | `x`          |
| 8      | 2    | `y`          |
| 10     | 2    | `vx`         |
| 12     | 2    | `vy`         |

`ENTITY_DESTROY` (`0x22`) — an entity disappears.

| Offset | Size | Field         | Description                                      |
|-------:|-----:|---------------|--------------------------------------------------|
| 0      | 2    | `reliableId`  |                                                  |
| 2      | 2    | `netId`       |                                                  |
| 4      | 1    | `reason`      | `0` KILLED, `1` OUT_OF_BOUNDS, `2` EXPIRED       |
| 5      | 2    | `killerNetId` | Entity responsible, `0xFFFF` if none.            |

`PLAYER_JOINED` (`0x23`) — sent to everyone when a player joins, and to the
newcomer once for every player already connected.

| Offset | Size | Field        |
|-------:|-----:|--------------|
| 0      | 2    | `reliableId` |
| 2      | 1    | `playerId`   |
| 3      | 16   | `name`       |

`PLAYER_LEFT` (`0x24`) — a player quit, timed out (crashed) or was kicked.

| Offset | Size | Field        | Values                           |
|-------:|-----:|--------------|----------------------------------|
| 0      | 2    | `reliableId` |                                  |
| 2      | 1    | `playerId`   |                                  |
| 3      | 1    | `reason`     | `0` QUIT, `1` TIMEOUT, `2` KICKED|

`PLAYER_DIED` (`0x25`) — a player's ship was destroyed.

| Offset | Size | Field         |
|-------:|-----:|---------------|
| 0      | 2    | `reliableId`  |
| 2      | 1    | `playerId`    |
| 3      | 2    | `killerNetId` |

`GAME_STATE` (`0x26`) — the game changed phase or the score changed.

| Offset | Size | Field        | Values                                        |
|-------:|-----:|--------------|-----------------------------------------------|
| 0      | 2    | `reliableId` |                                               |
| 2      | 1    | `state`      | `0` WAITING_FOR_PLAYERS, `1` RUNNING, `2` GAME_OVER |
| 3      | 4    | `score`      | Shared team score.                            |

---

## 5. Identifiers and enumerations

### 5.1 Network entity ids

The server gives each networked entity a `netId (u16)`, distinct from the ECS
`Entity` handle: the ECS generation is an internal detail and 16 bits are
enough on the wire. The server keeps the `Entity ↔ netId` mapping.

- `0xFFFF` means "no entity".
- A `netId` **MUST NOT** be reused within 5 seconds of its destruction, so
  late packets cannot be mistaken for a new entity.
- After `ENTITY_DESTROY`, a client **MUST** ignore that `netId` in snapshots
  for 5 seconds (a delayed snapshot could otherwise revive it).

### 5.2 Entity kinds

| Value | Kind             |
|------:|------------------|
| `0`   | PLAYER_SHIP      |
| `1`   | BYDO             |
| `2`   | PLAYER_MISSILE   |
| `3`   | ENEMY_MISSILE    |
| `4`–`255` | reserved for new content |

A client receiving an unknown kind **MUST NOT** crash. It **SHOULD** display
a placeholder.

### 5.3 Players

`playerId` is `0`–`3`, unique in a game, and selects the ship colour so the
four players are distinct. `0xFF` means "no player" (`owner` of an enemy).

---

## 6. Reliability and ordering

### 6.1 Sequence numbers and acknowledgements

- Each side keeps its own `sequence` counter per session, starting at `0` and
  incremented for every packet sent (retransmissions get a new sequence).
- Sequences wrap around: `a` is newer than `b` when
  `(a > b and a − b ≤ 32768) or (a < b and b − a > 32768)`
  ([RFC 1982](https://www.rfc-editor.org/rfc/rfc1982) serial arithmetic).
- Every packet acknowledges what the sender received: `ack` is the newest
  sequence received from the peer, and bit `i` of `ackBits` tells whether
  `ack − 1 − i` was also received. One packet thus acknowledges up to 33
  packets, and a lost acknowledgement is repeated by the next packets.
- A receiver **MUST** drop a packet whose `sequence` it already received
  (duplicate) or which is more than 32 sequences older than the newest one.

### 6.2 Reliable messages

- A reliable message has a `reliableId (u16)` that counts reliable messages
  only, per session and per direction, starting at `0`.
- The sender keeps the message until a packet carrying it is acknowledged.
  If it is not acknowledged after `max(100 ms, 2 × RTT)`, it is sent again in
  a new packet with the **same** `reliableId`.
- If a reliable message is still not acknowledged after **5 seconds**, the
  peer is considered disconnected.
- The receiver delivers reliable messages **in `reliableId` order**. A message
  arriving early waits in a buffer; one already delivered is dropped. The
  buffer holds at most 64 messages: a peer exceeding it is disconnected with
  `PROTOCOL_ERROR`.
- To respect that limit, a sender **MUST NOT** send a reliable message whose
  `reliableId` is more than 64 ahead of its oldest unacknowledged one. Later
  messages wait until the window moves. Without this rule, losing a single
  message while many others are in flight would disconnect a healthy peer.

In version 1 only the server sends reliable messages.

---

## 7. Validation rules

A malformed datagram **MUST NOT** crash the receiver, cause an allocation
larger than its own size, or change any state. A receiver checks, in order,
and drops the datagram at the first failure:

1. Size is between 20 and 1200 bytes.
2. `magic` is `0x5254`.
3. `version` is `1` (see [§3.2](#32-versioning) for `CONNECT_REQUEST`).
4. `type` is known and allowed in this direction (e.g. a server drops
   `SNAPSHOT`).
5. `reserved` is `0` and reserved flag bits are `0`.
6. `payloadSize` equals the datagram size minus 20.
7. `RELIABLE` is set exactly for the reliable types of [§4](#4-packet-types).
8. The payload size matches the type: exact for fixed-size types, and
   `5 + count` / `11 + 13 × count` for `INPUT` / `SNAPSHOT`, with `count`
   within its range.
9. Every enumeration value is in its range; names are valid UTF-8.
10. `sessionToken` matches the session of the sender's address (or is `0`
    for `CONNECT_REQUEST`).

The parser reads through a bounded reader that fails on any read past the
end of the buffer. Counts are never trusted for allocation: they are checked
against the remaining bytes first (rule 8).

### 7.1 Server-side protections

- **Sessions.** A session is identified by the client address (IP + port)
  *and* its token. Tokens are random, non-zero and generated with a
  cryptographically secure source, so a third party cannot inject packets
  into someone else's session by guessing.
- **Amplification.** A reply to an unauthenticated address
  (`CONNECT_ACCEPT` 24 bytes, `CONNECT_REJECT` 21 bytes) is never larger
  than the request that triggered it (`CONNECT_REQUEST` 36 bytes), so the
  server cannot be abused to flood a spoofed address.
- **Rate limiting.** The server drops packets from an address sending more
  than 120 packets per second.
- **Input sanity.** `INPUT` ticks more than 120 ticks ahead of the last
  processed one are ignored.

---

## 8. Session lifecycle

### 8.1 Connection

1. The client sends `CONNECT_REQUEST` with `sessionToken = 0`, every 500 ms,
   up to 10 times, until it gets an answer.
2. The server answers:
   - `CONNECT_REJECT` if the version differs, the game is full or running,
     or the name is invalid;
   - otherwise `CONNECT_ACCEPT` with a new token and `playerId`.
   A duplicate request from an address that already has a session receives
   the same `CONNECT_ACCEPT` again (the first one may have been lost).
3. The server sends the newcomer, reliably: `PLAYER_JOINED` for every other
   player, `ENTITY_SPAWN` for every existing entity, then `GAME_STATE`. It
   sends `PLAYER_JOINED` for the newcomer to every other client.

### 8.2 Keep-alive and timeout

- A peer that sent nothing for **250 ms** sends a `HEARTBEAT`.
- A peer that received nothing for **5 seconds** considers the session dead.
- When a client session dies (timeout, crash, `DISCONNECT`), the server
  removes it, destroys its ship and sends `PLAYER_LEFT` to the remaining
  clients. The game goes on.

### 8.3 Disconnection

Either side sends `DISCONNECT` (3 times, best effort) and forgets the
session. A client receiving `DISCONNECT` returns to its connection screen.

---

## 9. Timing

| Value              | Default | Sent in          |
|--------------------|--------:|------------------|
| Server tick rate   | 60 Hz   | `CONNECT_ACCEPT` |
| Snapshot rate      | 30 Hz   | `CONNECT_ACCEPT` |
| Client input rate  | 60 Hz   | —                |
| Heartbeat interval | 250 ms  | —                |
| Session timeout    | 5 s     | —                |

Ticks are counters, not clock times, so the game speed never depends on the
CPU speed.

**Bandwidth estimate.** With 4 ships, 30 Bydos and 60 missiles (94 entities),
a snapshot takes 2 parts, about 1280 bytes in total with headers. At 30 Hz that is about
**38 KB/s per client** downstream, events excluded. Upstream, `INPUT` at
60 Hz with 8 masks is 33 bytes × 60 ≈ **2 KB/s per client**. These figures are
the baseline for the optimisation work of Part 2 (#77, #78, #79, #93).

---

## 10. Example

`CONNECT_REQUEST` from a player named `Thibani`, first packet of the client:

```text
52 54             magic "RT"
01                version 1
01                type CONNECT_REQUEST
00                flags: none (nothing received yet, so no HAS_ACK)
00                reserved
00 10             payloadSize 16
00 00 00 00       sessionToken 0 (not connected)
00 00             sequence 0
00 00             ack      (ignored, HAS_ACK clear)
00 00 00 00       ackBits  (ignored)
54 68 69 62 61 6E 69 00 00 00 00 00 00 00 00 00   name "Thibani", zero-padded
```

Total: 36 bytes.

---

## 11. Possible extensions

Kept out of version 1, but the header leaves room for them:

- a `COMPRESSED` flag bit for LZ4 or zlib payloads (#79);
- delta snapshots relative to the last acknowledged snapshot (#78);
- rooms and lobbies, as new packet types in the `0x30–0x4F` range (#71, #72);
- reliable client → server messages (chat, room commands) using the same
  mechanism as [§6.2](#62-reliable-messages).
