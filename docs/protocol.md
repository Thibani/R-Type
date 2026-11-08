# R-Type Network Protocol — Version 8

| Status   | Implemented                 |
|----------|-----------------------------|
| Version  | 8                           |
| Transport| UDP only                    |
| Issue    | #21 (design), #10 (documentation), #97 (encryption), #73 (accounts), #72 (main page), #126 (friends), #92 (chat), #71 (groups), #91 (leaderboard) |

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
   | -- CONNECT_REQUEST ------------------> |  in clear: public key of the client
   | <------------------- CONNECT_ACCEPT -- |  in clear: public key of the server
   |        everything below is encrypted   |
   | -- AUTH_REQUEST ---------------------> |  log in, or create the account
   | <----------- AUTH_RESULT (reliable) -- |
   |    if OK, the player is on the main    |
   |    page: connected, not in the game    |
   | <------- GROUP_STATE (reliable) ------ |  alone in a new group, leading it
   | -- JOIN_GAME (reliable) -------------> |  the leader starts the game
   | <----------- JOIN_RESULT (reliable) -- |  a place in the game, after the countdown
   | <-------- PLAYER_JOINED (reliable) --- |  one per other player of the group
   | <-------- ENTITY_SPAWN  (reliable) --- |  one per existing entity
   |                                        |
   | -- INPUT (every client frame) -------> |
   | <------------ SNAPSHOT (30 Hz) ------ |
   | <------- events (reliable) ----------- |
   |                                        |
   | -- LEAVE_GAME (reliable) ------------> |  back to the main page
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

A password follows the same rules in a 64-byte field. The field has a fixed size
so that the length of the password does not show in the size of the packet.

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
| 2      | 1    | `version`      | Protocol version. This document describes version `8`.       |
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
| 2–7 | —          | Reserved, **MUST** be `0` in version 8.                              |

### 3.2 Versioning

- The `version` byte must match exactly. Peers do not negotiate.
- Any change to an existing layout bumps the version.
- A `CONNECT_REQUEST` with a different version is answered with
  `CONNECT_REJECT (VERSION_MISMATCH)`, sent with the server's own version.
  Every other packet with a different version is dropped silently.
- Reserved bits and fields leave room for later extensions (compression,
  delta snapshots) without changing the header size.

### 3.3 Encryption

Every packet of a session is encrypted and authenticated. Only `CONNECT_REQUEST`,
`CONNECT_ACCEPT` and `CONNECT_REJECT` travel in clear, because they happen before
a key exists. A peer **MUST** drop any other packet of a session that is not
sealed with its key.

**Key exchange.** Each side creates an X25519 key pair for this session and
sends its public key (`CONNECT_REQUEST`, `CONNECT_ACCEPT`). No key is hard-coded.

```text
shared      = X25519(our secret key, the peer's public key)
session key = BLAKE2b-256(shared | client public key | server public key)
```

A public key that makes `shared` all zeros is invalid and refused.

**Sealing.** The header stays readable, since the receiver needs the token to find
the session. The payload is encrypted with XChaCha20-Poly1305, with the header as
additional data: changing a single byte of the datagram makes it fail.

```text
datagram        = header (20) | counter (8, big-endian) | encrypted payload | tag (16)
payloadSize     = size of the payload + 24
additional data = the 20 bytes of the header above
nonce (24)      = sender (1: 0 client, 1 server) | counter (8, big-endian) | zeros (15)
```

**Counter.** Each side counts its own sealed packets from 0 and never reuses a
value. `sequence` cannot be used instead: at 16 bits it wraps after about 18 minutes
at 60 packets per second, and a nonce must never be used twice with one key. A
resent reliable message is sealed again, with a new counter.

**Replay.** A receiver refuses a counter already received, or more than 63 behind
the newest one (UDP can reorder packets). It records a counter only once the tag
is verified, so a forged packet cannot block real ones.

**Effect on the 1200-byte limit.** Sealing adds 24 bytes per packet. Only `SNAPSHOT`
can get near the limit: it goes from 89 to 88 entities per part.

**What it does not hide or prevent:**

- the name in `CONNECT_REQUEST`, and in each header the type, token, sequence and acks;
- an active attacker present during the connection: the exchange does not prove who
  the peer is, so one could run one exchange with each side and read what the client
  sends, **including its password** in `AUTH_REQUEST`. This is a known limit of the
  project: it needs a server key known in advance by the clients, which we do not
  distribute. Do not reuse a valuable password.

---

## 4. Packet types

Sizes are payload sizes, header excluded. Reliable packets include their
2-byte `reliableId` in the size.

| Type   | Name              | Direction | Reliable | Payload size       |
|--------|-------------------|-----------|----------|--------------------|
| `0x01` | `CONNECT_REQUEST` | C → S     | no       | 48                 |
| `0x02` | `CONNECT_ACCEPT`  | S → C     | no       | 36                 |
| `0x03` | `CONNECT_REJECT`  | S → C     | no       | 1                  |
| `0x04` | `DISCONNECT`      | both      | no       | 1                  |
| `0x05` | `HEARTBEAT`       | both      | no       | 0                  |
| `0x06` | `AUTH_REQUEST`    | C → S     | no       | 65                 |
| `0x07` | `AUTH_RESULT`     | S → C     | **yes**  | 3                  |
| `0x10` | `INPUT`           | C → S     | no       | 6 … 13 (`5 + count`) |
| `0x20` | `SNAPSHOT`        | S → C     | no       | `11 + 13 × count`  |
| `0x21` | `ENTITY_SPAWN`    | S → C     | **yes**  | 14                 |
| `0x22` | `ENTITY_DESTROY`  | S → C     | **yes**  | 7                  |
| `0x23` | `PLAYER_JOINED`   | S → C     | **yes**  | 19                 |
| `0x24` | `PLAYER_LEFT`     | S → C     | **yes**  | 4                  |
| `0x25` | `PLAYER_DIED`     | S → C     | **yes**  | 5                  |
| `0x26` | `GAME_STATE`      | S → C     | **yes**  | 7                  |
| `0x30` | `JOIN_GAME`       | C → S     | **yes**  | 2                  |
| `0x31` | `LEAVE_GAME`      | C → S     | **yes**  | 2                  |
| `0x32` | `JOIN_RESULT`     | S → C     | **yes**  | 4                  |
| `0x33` | `FRIEND_REQUEST`  | C → S     | **yes**  | 18                 |
| `0x34` | `FRIEND_ANSWER`   | C → S     | **yes**  | 19                 |
| `0x35` | `FRIEND_REMOVE`   | C → S     | **yes**  | 18                 |
| `0x36` | `FRIEND_RESULT`   | S → C     | **yes**  | 3                  |
| `0x37` | `FRIEND_ENTRY`    | S → C     | **yes**  | 19                 |
| `0x38` | `CHAT_SEND`       | C → S     | **yes**  | 4 … 203 (`3 + size`) |
| `0x39` | `PRIVATE_SEND`    | C → S     | **yes**  | 20 … 219 (`19 + size`) |
| `0x3A` | `CHAT_MESSAGE`    | S → C     | **yes**  | 21 … 220 (`20 + size`) |
| `0x3B` | `CHAT_REFUSED`    | S → C     | **yes**  | 3                  |
| `0x3C` | `GROUP_CODE`      | C → S     | **yes**  | 3                  |
| `0x3D` | `GROUP_JOIN`      | C → S     | **yes**  | 8                  |
| `0x3E` | `GROUP_LEAVE`     | C → S     | **yes**  | 2                  |
| `0x3F` | `GROUP_KICK`      | C → S     | **yes**  | 18                 |
| `0x40` | `GROUP_PROMOTE`   | C → S     | **yes**  | 18                 |
| `0x41` | `GROUP_INVITE`    | C → S     | **yes**  | 18                 |
| `0x42` | `GROUP_ANSWER`    | C → S     | **yes**  | 19                 |
| `0x43` | `GROUP_READY`     | C → S     | **yes**  | 3                  |
| `0x44` | `GROUP_STATE`     | S → C     | **yes**  | 11 + 17 × count    |
| `0x45` | `GROUP_RESULT`    | S → C     | **yes**  | 3                  |
| `0x46` | `GROUP_INVITED`   | S → C     | **yes**  | 18                 |
| `0x47` | `LEADERBOARD_REQUEST` | C → S | **yes**  | 3                  |
| `0x48` | `LEADERBOARD`     | S → C     | **yes**  | 12 + 68 × count    |

Ranges `0x00`, `0x08–0x0F`, `0x11–0x1F`, `0x27–0x2F` and `0x49–0xFF` are reserved.

These sizes are before encryption: every type except `CONNECT_REQUEST`,
`CONNECT_ACCEPT` and `CONNECT_REJECT` adds 24 bytes on the network
([§3.3](#33-encryption)).

### 4.1 Connection

#### `CONNECT_REQUEST` (`0x01`) — C → S

| Offset | Size | Field  | Description                    |
|-------:|-----:|--------|--------------------------------|
| 0      | 16   | `name` | Name of the player's account ([§2.2](#22-strings)) |
| 16     | 32   | `publicKey` | The client's X25519 public key ([§3.3](#33-encryption)). The same for every retry of a connection. |

`sessionToken` **MUST** be `0`.

#### `CONNECT_ACCEPT` (`0x02`) — S → C

| Offset | Size | Field          | Description                                   |
|-------:|-----:|----------------|-----------------------------------------------|
| 0      | 1    | `playerId`     | Always `0xFF`: a place in the game comes with `JOIN_RESULT`. |
| 1      | 1    | `tickRate`     | Server simulation rate in Hz (e.g. `60`).     |
| 2      | 1    | `snapshotRate` | Snapshots per second (e.g. `30`).             |
| 3      | 1    | `maxPlayers`   | Players per game (`4`).                       |
| 4      | 32   | `publicKey`    | The server's X25519 public key ([§3.3](#33-encryption)). |

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

#### `AUTH_REQUEST` (`0x06`) — C → S

Logs in to the account named in `CONNECT_REQUEST`, or creates it.

| Offset | Size | Field      | Description                                  |
|-------:|-----:|------------|----------------------------------------------|
| 0      | 1    | `mode`     | `0` LOGIN, `1` REGISTER                      |
| 1      | 64   | `password` | [§2.2](#22-strings). To create an account: at least 8 bytes. |

#### `AUTH_RESULT` (`0x07`) — S → C

Reliable, and always the first reliable message of a session.

| Offset | Size | Field        | Description                       |
|-------:|-----:|--------------|-----------------------------------|
| 0      | 2    | `reliableId` | [§6.2](#62-reliable-messages)     |
| 2      | 1    | `code`       | See below                         |

| Code | Name                | Meaning                                                    |
|-----:|---------------------|------------------------------------------------------------|
| `0`  | `OK`                | The player is logged in, on the main page                  |
| `1`  | `WRONG_CREDENTIALS` | Unknown name or wrong password: the same code for both     |
| `2`  | `NAME_TAKEN`        | REGISTER: this account already exists                      |
| `3`  | `ALREADY_CONNECTED` | LOGIN: right password, but this account is already connected |
| `4`  | `INVALID_PASSWORD`  | REGISTER: fewer than 8 bytes                               |
| `5`  | `INVALID_NAME`      | REGISTER: empty name, or a control character in it         |
| `6`  | `SERVER_ERROR`      | The server could not hash or save                          |

### 4.2 Client → server

#### `JOIN_GAME` (`0x30`) and `LEAVE_GAME` (`0x31`)

Reliable, and their only field is `reliableId`. Before the login, the server
ignores them. It also ignores `JOIN_GAME` from a player in a game, and
`LEAVE_GAME` from a player who is not ([§8.4](#84-main-page-and-game)).

#### `FRIEND_REQUEST` (`0x33`), `FRIEND_ANSWER` (`0x34`), `FRIEND_REMOVE` (`0x35`)

The server ignores them before the login ([§8.5](#85-friends)).

| Offset | Size | Field        | Values                                              |
|-------:|-----:|--------------|-----------------------------------------------------|
| 0      | 2    | `reliableId` |                                                     |
| 2      | 16   | `name`       | Exact name of the other account, not empty ([§2.2](#22-strings)) |
| 18     | 1    | `reply`      | `FRIEND_ANSWER` only: `0` REFUSE, `1` ACCEPT        |

`FRIEND_REMOVE` removes a friend, or cancels a request the player sent.

#### `CHAT_SEND` (`0x38`) and `PRIVATE_SEND` (`0x39`)

A chat text is 1 to 200 bytes of valid UTF-8, without any control character. It comes
last in its packet, after a `u8` count that **MUST** match the bytes left.

| Offset | Size | Field        | Values                                              |
|-------:|-----:|--------------|-----------------------------------------------------|
| 0      | 2    | `reliableId` |                                                     |
| 2      | 16   | `name`       | `PRIVATE_SEND` only: the exact name of a friend     |
| …      | 1    | `size`       | Bytes of `text`                                     |
| …      | size | `text`       |                                                     |

#### Group orders (`0x3C`–`0x43`)

Each starts with `reliableId`. The server ignores them before the login and from a player
in a game ([§8.7](#87-groups)). `GROUP_LEAVE` has no other field.

| Packet          | Offset | Size | Field    | Values                                     |
|-----------------|-------:|-----:|----------|--------------------------------------------|
| `GROUP_CODE`    | 2      | 1    | `action` | `0` GENERATE, `1` DELETE                   |
| `GROUP_JOIN`    | 2      | 6    | `code`   | 6 signs of `ABCDEFGHJKLMNPQRSTUVWXYZ23456789` |
| `GROUP_KICK`, `GROUP_PROMOTE`, `GROUP_INVITE` | 2 | 16 | `name` | Exact name, not empty |
| `GROUP_ANSWER`  | 2      | 16   | `name`   | Who invited                                |
|                 | 18     | 1    | `reply`  | `0` REFUSE, `1` ACCEPT                     |
| `GROUP_READY`   | 2      | 1    | `ready`  | `0` or `1`                                 |

#### `LEADERBOARD_REQUEST` (`0x47`)

After `reliableId`, 1 byte: `board`, `0` SOLO or `1` MULTI. The server ignores it before the
login ([§8.8](#88-leaderboard)).

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

At most `(1200 − 20 − 24 − 11) / 13 = 88` entities fit in one part, encryption
included.

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

`JOIN_RESULT` (`0x32`) — the answer to `JOIN_GAME`. It comes before every event of the game.

| Offset | Size | Field        | Values                                        |
|-------:|-----:|--------------|-----------------------------------------------|
| 0      | 2    | `reliableId` |                                               |
| 2      | 1    | `code`       | `0` OK, `1` NOT_LEADER, `2` NOT_READY         |
| 3      | 1    | `playerId`   | `0`–`3` if OK, `0xFF` otherwise               |

---

`FRIEND_RESULT` (`0x36`) — the answer to `FRIEND_REQUEST`.

| Offset | Size | Field        | Values                                              |
|-------:|-----:|--------------|-----------------------------------------------------|
| 0      | 2    | `reliableId` |                                                     |
| 2      | 1    | `code`       | `0` OK, `1` UNKNOWN_NAME, `2` ALREADY_LINKED, `3` YOURSELF, `4` LIMIT_REACHED |

`FRIEND_ENTRY` (`0x37`) — one line of the player's list, new or changed.

| Offset | Size | Field        | Values                                              |
|-------:|-----:|--------------|-----------------------------------------------------|
| 0      | 2    | `reliableId` |                                                     |
| 2      | 16   | `name`       | Not empty                                           |
| 18     | 1    | `state`      | `0` OFFLINE, `1` ONLINE, `2` REQUEST_RECEIVED, `3` REQUEST_SENT, `4` REMOVED |

---

`CHAT_MESSAGE` (`0x3A`) — one line of chat. The server writes `name` itself.

| Offset | Size | Field        | Values                                              |
|-------:|-----:|--------------|-----------------------------------------------------|
| 0      | 2    | `reliableId` |                                                     |
| 2      | 1    | `kind`       | `0` GAME, `1` RECEIVED (private), `2` SENT (private, your own) |
| 3      | 16   | `name`       | Who wrote it; for SENT, who receives it             |
| 19     | 1    | `size`       | Bytes of `text`                                     |
| 20     | size | `text`       |                                                     |

`CHAT_REFUSED` (`0x3B`) — a message was not taken.

| Offset | Size | Field        | Values                                              |
|-------:|-----:|--------------|-----------------------------------------------------|
| 0      | 2    | `reliableId` |                                                     |
| 2      | 1    | `reason`     | `0` TOO_FAST, `1` NOT_FRIEND, `2` MAILBOX_FULL      |

`GROUP_STATE` (`0x44`) — the player's whole group, whenever something in it changes.

| Offset | Size | Field        | Values                                              |
|-------:|-----:|--------------|-----------------------------------------------------|
| 0      | 2    | `reliableId` |                                                     |
| 2      | 6    | `code`       | Zero bytes: no code                                 |
| 8      | 1    | `countdown`  | Seconds before the game; `0`: none                  |
| 9      | 1    | `leader`     | Index in the members                                |
| 10     | 1    | `count`      | `1`–`4`                                             |
| 11     | 17 × count | members | 16 bytes of `name`, then 1 byte: bit 0 READY, bit 1 IN_GAME |

`GROUP_RESULT` (`0x45`) — what came of an order, when it needs saying.

| Offset | Size | Field        | Values                                              |
|-------:|-----:|--------------|-----------------------------------------------------|
| 0      | 2    | `reliableId` |                                                     |
| 2      | 1    | `outcome`    | `0` INVITED, `1` UNKNOWN_CODE, `2` GROUP_FULL, `3` KICKED, `4` GROUP_IN_GAME, `5` NOT_LEADER, `6` NOT_MEMBER, `7` ALONE, `8` TOO_MANY_TRIES, `9` NOT_AVAILABLE, `10` INVITATION_GONE, `11` REMOVED_FROM_GROUP |

`GROUP_INVITED` (`0x46`) — a leader invites this player. After `reliableId`, 16 bytes: their name.

`LEADERBOARD` (`0x48`) — the answer to `LEADERBOARD_REQUEST`.

| Offset | Size | Field        | Values                                              |
|-------:|-----:|--------------|-----------------------------------------------------|
| 0      | 2    | `reliableId` |                                                     |
| 2      | 1    | `board`      | `0` SOLO, `1` MULTI: the one asked for              |
| 3      | 1    | `count`      | `0`–`10`: the best games, best first                |
| 4      | 68 × count | games  | 4 bytes of `score` (`u32`), then 4 names of 16 bytes: the accounts that started the game, the unused ones last and all zero. SOLO: exactly one name |
| …      | 4    | `lastRank`   | Rank of the player's last game in this ranking, even beyond 10; `0`: none |
| …      | 4    | `lastScore`  | Its score; `0` if `lastRank` is `0`                 |

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
four players are distinct. A player has one only while in the game: it is given
by `JOIN_RESULT`, and may change after leaving. `0xFF` means "no player" (`owner`
of an enemy, `CONNECT_ACCEPT`).

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

The client sends reliable messages too (`JOIN_GAME`, `LEAVE_GAME`, friends, chat, groups, leaderboard), with the same rules.

---

## 7. Validation rules

A malformed datagram **MUST NOT** crash the receiver, cause an allocation
larger than its own size, or change any state. A receiver checks, in order,
and drops the datagram at the first failure:

1. Size is between 20 and 1200 bytes.
2. `magic` is `0x5254`.
3. `version` is `8` (see [§3.2](#32-versioning) for `CONNECT_REQUEST`).
4. `type` is known and allowed in this direction (e.g. a server drops
   `SNAPSHOT`).
5. `reserved` is `0` and reserved flag bits are `0`.
6. `payloadSize` equals the datagram size minus 20.
7. `RELIABLE` is set exactly for the reliable types of [§4](#4-packet-types).
8. The payload size matches the type: exact for fixed-size types, and
   `5 + count` / `11 + 13 × count` / `11 + 17 × count` / `12 + 68 × count` for
   `INPUT` / `SNAPSHOT` / `GROUP_STATE` / `LEADERBOARD`, with `count` within its range.
9. Every enumeration value is in its range; names are valid UTF-8.
10. `sessionToken` matches the session of the sender's address (or is `0`
    for `CONNECT_REQUEST`).

Encryption comes first ([§3.3](#33-encryption)): when the address and the
`sessionToken` match a session, the datagram is opened before rules 3 to 10
apply to its content. A datagram that cannot be opened is dropped.

The parser reads through a bounded reader that fails on any read past the
end of the buffer. Counts are never trusted for allocation: they are checked
against the remaining bytes first (rule 8).

### 7.1 Server-side protections

- **Sessions.** A session is identified by the client address (IP + port)
  *and* its token. Tokens are random, non-zero and generated with a
  cryptographically secure source, so a third party cannot inject packets
  into someone else's session by guessing. The token is visible on the
  network: what really protects a session is its key, since a packet that
  is not sealed with it is dropped.
- **Amplification.** A reply to an unauthenticated address
  (`CONNECT_ACCEPT` 56 bytes, `CONNECT_REJECT` 21 bytes) is never larger
  than the request that triggered it (`CONNECT_REQUEST` 68 bytes), so the
  server cannot be abused to flood a spoofed address.
- **Rate limiting.** The server drops packets from an address sending more
  than 120 packets per second.
- **Accounts.** One `AUTH_REQUEST` is handled per session: guessing another
  password takes a new connection. A session not logged in after 10 seconds
  is closed. An unknown name is hashed like a known one, so the delay of the
  answer does not reveal which names exist.
- **Group codes.** A player can try 5 wrong codes per 30 seconds. Beyond them, even the
  right code gets `TOO_MANY_TRIES`, so that an answer tells nothing while it is blocked.
- **Input sanity.** `INPUT` ticks more than 120 ticks ahead of the last
  processed one are ignored.

---

## 8. Session lifecycle

### 8.1 Connection

1. The client sends `CONNECT_REQUEST` with `sessionToken = 0`, every 500 ms,
   up to 10 times, until it gets an answer.
2. The server answers:
   - `CONNECT_REJECT` if the version differs, the server already has 64 sessions
     (`SERVER_FULL`), or the name is invalid;
   - otherwise `CONNECT_ACCEPT` with a new token.
   A duplicate request from an address that already has a session receives
   the same `CONNECT_ACCEPT` again (the first one may have been lost). A
   request with another `publicKey` gets no answer and does not change the
   session: an address can be faked. An invalid `publicKey` gets no answer.
3. The client sends `AUTH_REQUEST`, sealed, every 500 ms, up to 20 times, until
   it receives `AUTH_RESULT`. The server handles only the first one it receives.
   Until the result is `OK`, the server sends the session only `AUTH_RESULT` and heartbeats, and ignores everything from it except
   `AUTH_REQUEST`, `HEARTBEAT` and `DISCONNECT`.
4. If the result is not `OK`, the client sends `DISCONNECT`, and must open a new
   session to try again. The server closes a session still not logged in after
   **10 seconds** (`DISCONNECT` TIMEOUT).
5. If it is `OK`, the player is on the main page ([§8.4](#84-main-page-and-game)).

An account can be connected only once. A session that is not logged in uses one of
the 64 sessions, for at most 10 seconds.

### 8.2 Keep-alive and timeout

- A peer that sent nothing for **250 ms** sends a `HEARTBEAT`.
- A peer that received nothing for **5 seconds** considers the session dead.
- When a client session dies (timeout, crash, `DISCONNECT`), the server
  removes it. If it was in the game, the server destroys its ship and sends
  `PLAYER_LEFT` to the remaining players. The game goes on.

### 8.3 Disconnection

Either side sends `DISCONNECT` (3 times, best effort) and forgets the
session. A client receiving `DISCONNECT` returns to its connection screen.

### 8.4 Main page and game

A logged-in player is on the main page: the session is open, and nothing of the game
is sent to it. The other players do not know about it. Any number of players can be
there, up to the 64 sessions of the server.

- A game holds the players of one group ([§8.7](#87-groups)), and the server runs as many
  games as it has groups playing: each has its own entities, ticks and score. It ends with
  its last player.
- When a game starts, each player gets `JOIN_RESULT (OK)` with a `playerId`, their place in
  the group. Then, reliably and in this order: `PLAYER_JOINED` for every other player,
  `ENTITY_SPAWN` for every existing entity, then `GAME_STATE`.
- `LEAVE_GAME`: the player goes back to the main page. The server destroys their ship, and
  sends `PLAYER_LEFT (QUIT)` to the other players of that game.

Events and snapshots sent before the server read `LEAVE_GAME` may still arrive: a client
**MUST** ignore them until `JOIN_RESULT`. The input ticks of a session and the snapshot ticks it
receives keep growing across games, so nothing late from one game counts in the next: a game
starts its ticks at the server's elapsed time × 60.

### 8.5 Friends

Friends are kept by the server, between accounts. They only need a logged-in session:
they work on the main page and during a game.

- Once logged in, a player receives one `FRIEND_ENTRY` per friend (`OFFLINE` or `ONLINE`)
  and per request, received or sent. A request sent while they were offline arrives this way.
- `FRIEND_REQUEST` is answered by `FRIEND_RESULT`. If it is `OK`, both players get a
  `FRIEND_ENTRY`: `REQUEST_SENT` and `REQUEST_RECEIVED`. If the other had already asked,
  they become friends at once.
- `FRIEND_ANSWER (ACCEPT)` makes them friends: each gets a `FRIEND_ENTRY` with the status
  of the other. `FRIEND_ANSWER (REFUSE)` and `FRIEND_REMOVE` give both a `REMOVED` entry.
  An answer to a request that does not exist does nothing.
- When a player comes online or leaves, each friend online gets a `FRIEND_ENTRY`.
- Limits, per player: **50 friends**, **50 requests sent**, **50 requests received**.
  Beyond them: `LIMIT_REACHED`.

Known limits: `UNKNOWN_NAME` tells whether an account exists, which adding by exact name
cannot avoid. 50 accounts can fill a player's requests. The server saves the file at most
once per second, so a crash can lose the last second of changes. The file is not encrypted.

### 8.6 Chat

- `CHAT_SEND`: every member of the player's group gets a `CHAT_MESSAGE (GAME)`, the author
  too, on the main page and in a game alike.
- `PRIVATE_SEND` to a friend, online or not: if online, they get a `CHAT_MESSAGE (RECEIVED)`;
  if offline, the server keeps the message and delivers it at their next login, oldest first,
  then forgets it. The author gets a `CHAT_MESSAGE (SENT)`. To anybody else: `CHAT_REFUSED
  (NOT_FRIEND)`. With 100 messages already waiting: `CHAT_REFUSED (MAILBOX_FULL)`.
- A player can write **5 messages per 5 seconds**, game and private together. Beyond them:
  `CHAT_REFUSED (TOO_FAST)`. This value is a choice, not a measure.

The server sets the name of a message from the session: a client cannot write as someone else.

Known limits: the messages waiting are in a plain text file, so whoever runs the server can
read them. It is saved at most once per second, so a crash can lose the last second, or
deliver a message a second time. A message is forgotten once sent to its reader, even if
they crash before receiving it. The server keeps no other history: the client's goes when it
logs out.

### 8.7 Groups

Each player online is in exactly one group, alone and leading it at first, with no code. A
group has 1 to 4 members, and lives in the server's memory until its last member goes offline.
Each member gets `GROUP_STATE` whenever something in it changes, the first time at login.

- **The leader** alone can: ask for a code or delete it (`GROUP_CODE`), remove a member
  (`GROUP_KICK`), give the role (`GROUP_PROMOTE`), invite a friend online (`GROUP_INVITE`), and
  start the game (`JOIN_GAME`). From anyone else: `NOT_LEADER`.
- **The code** lets anyone in with `GROUP_JOIN`. A new code replaces the old one. A player
  already in a group leaves it. Refused with `UNKNOWN_CODE`, `GROUP_FULL`, `GROUP_IN_GAME`, or
  `KICKED` for a player the leader removed: only an invitation lets them back.
- **An invitation** reaches the friend as `GROUP_INVITED`, and is answered by `GROUP_ANSWER`.
  It ends with that answer, or if the one who invited left the group (`INVITATION_GONE`).
- **Leaving:** `GROUP_LEAVE` gives the player a new group, alone. From a player already alone:
  `ALONE`. When the leader leaves or goes offline, the first of the other members leads.
- **Starting:** the other members say `GROUP_READY`. `JOIN_GAME` needs them all ready and no
  member in a game (otherwise `JOIN_RESULT (NOT_READY)`). Then `countdown` runs for 3 seconds,
  and any change among the members, their readiness or the leader stops it. At the end the game
  starts ([§8.4](#84-main-page-and-game)), and nobody is ready any more. A member who left the
  game waits on the main page for the others.

Known limits: the 5 wrong codes are per account, so many accounts can try more of the billion
codes. Anyone who reads a code can use it, until it is replaced or deleted.

### 8.8 Leaderboard

The server keeps every finished game in two rankings: **SOLO** for a game started by one
player, **MULTI** for a game started by a group of 2 to 4. A game is a line: the same players
can be there several times.

- A game ends when no player is left in it: its team score then enters its ranking, under the
  names of the accounts that started it, even those who left before the end. While one player
  is still in the game, it goes on and keeps scoring.
- Games are sorted by score; on a tie, the older game comes first.
- `LEADERBOARD_REQUEST` is answered by `LEADERBOARD`: the 10 best games of that ranking, then
  the rank and score of the last game the player played there, even far below the 10.

Known limits: a game still running when the server stops is not kept. The server saves the
file at most once per second, so a crash can lose the last second. The file grows by one line
per game. Until lives exist (endless mode, #145), a game only ends when its last player leaves.

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
a snapshot takes 2 parts, about 1330 bytes in total with headers and encryption. At 30 Hz
that is about **40 KB/s per client** downstream, events excluded. Upstream, `INPUT` at
60 Hz with 8 masks is 57 bytes × 60 ≈ **3.4 KB/s per client**.

---

## 10. Example

`CONNECT_REQUEST` from a player named `Thibani`, first packet of the client:

```text
52 54             magic "RT"
08                version 8
01                type CONNECT_REQUEST
00                flags: none (nothing received yet, so no HAS_ACK)
00                reserved
00 30             payloadSize 48
00 00 00 00       sessionToken 0 (not connected)
00 00             sequence 0
00 00             ack      (ignored, HAS_ACK clear)
00 00 00 00       ackBits  (ignored)
54 68 69 62 61 6E 69 00 00 00 00 00 00 00 00 00   name "Thibani", zero-padded
AB AB AB ... (32 bytes)                           publicKey (random in reality)
```

Total: 68 bytes.

---

## 11. Possible extensions

Kept out of version 8, but the header leaves room for them:

- a `COMPRESSED` flag bit for LZ4 or zlib payloads;
- delta snapshots relative to the last acknowledged snapshot;
- matchmaking and game modes, as new packet types in the `0x49–0x4F` range (#72, #130).
