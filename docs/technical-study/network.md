# Technical study: networking

This page explains **why** the network layer of R-Type is built the way it is.
For each decision it lists the alternatives we considered, compares them, and
justifies the choice. The protocol itself is specified in
[`docs/protocol.md`](../protocol.md); the code lives in `src/network/` and
`src/server/`.

| Decision            | Choice                                        | Main alternatives                         |
|---------------------|-----------------------------------------------|-------------------------------------------|
| Transport           | UDP only                                      | TCP, UDP + TCP                            |
| Socket library      | Standalone Asio (async on the server's network thread, polled on the client) | Raw BSD sockets / Winsock, SFML Network, Boost.Asio, libuv |
| Message format      | Hand-written binary codec, big-endian         | JSON, raw `memcpy` of structs, Protocol Buffers, FlatBuffers |
| Reliability         | Our own: sequences, ack bitfield, resends     | TCP, ENet, GameNetworkingSockets, yojimbo |
| Game state sync     | Authoritative server, inputs up, snapshots + events down | Lockstep, client authority  |
| Timing              | Fixed 60 Hz simulation, 30 Hz snapshots       | Variable time step                        |
| Concurrency         | Server: a network thread + the game thread, two queues between them | One thread per client, async callbacks    |
| Error handling      | Errors as values (`std::expected`)            | Exceptions, error codes + out-parameters  |

---

## 1. Transport: UDP

| Criterion                  | TCP                                    | UDP                                   |
|----------------------------|----------------------------------------|---------------------------------------|
| Unit of data               | Byte stream: messages must be framed   | Datagrams: one message = one packet   |
| Lost packet                | Retransmitted; **everything after it waits** | Simply lost                     |
| Ordering                   | Always in order                        | None                                  |
| Latency under 1–2 % loss   | Spikes of one round-trip or more       | Unaffected                            |
| Connection                 | Handshake, server keeps a socket per client | Connectionless: one socket for all clients |
| Control                    | Kernel decides (Nagle, congestion window) | Application decides everything     |

The deciding problem is **head-of-line blocking**. With TCP, when one packet is
lost, the operating system holds back every packet received after it until
the lost one is resent. In a game, most data is *superseded*: a position from
50 ms ago is worthless once the next one exists. Waiting for it freezes
everything behind it, which players see as a lag spike on every lost packet.

UDP lets the game decide per message:

- **Snapshots** (positions of every entity, 30 times per second) are sent
  unreliably. A lost one is replaced by the next one 33 ms later.
- **Events** (spawn, destroy, a player joining or leaving) happen once and
  must arrive. They are made reliable *on top of UDP* (see §4).

This is also what the subject requires: "ALL in-game communications MUST use
UDP". A second TCP connection (for example for the connection handshake) was
considered and rejected: it would double the connection logic and firewall
configuration for no gain, since our UDP handshake is already reliable
(`CONNECT_REQUEST` is retried every 500 ms, see protocol §8.1).

**Consequence we accept:** we must handle loss, duplication and reordering
ourselves. §4 explains how.

---

## 2. Socket library: standalone Asio

| Criterion           | Raw BSD sockets / Winsock | SFML Network      | Boost.Asio       | Standalone Asio  | libuv            |
|---------------------|---------------------------|-------------------|------------------|------------------|------------------|
| Linux + Windows     | Two APIs, `#ifdef`s       | Yes               | Yes              | Yes              | Yes              |
| Dependency size     | None                      | All of SFML on the **server** | All of Boost | Header-only  | C library        |
| C++ API, RAII       | Must be written           | Yes               | Yes              | Yes              | No (C callbacks) |
| Errors without exceptions | `errno` / `WSAGetLastError` | Status enum | `std::error_code` | `std::error_code` | Return codes |
| Non-blocking UDP    | `fcntl` / `ioctlsocket`   | Yes               | Yes              | Yes              | Event loop only  |
| Allowed by subject  | Yes, if wrapped           | Yes               | Yes              | Yes (named)      | Yes              |

- **Raw sockets** would teach the most, but every call differs on Windows
  (`closesocket` vs `close`, `WSAStartup`, error codes, non-blocking mode), and
  the subject requires a proper abstraction around them anyway. We would be
  writing a smaller, less tested Asio.
- **SFML Network** would make the *server* depend on a multimedia library it
  never uses for graphics, and its error reporting is coarse.
- **Boost.Asio** is the same code as standalone Asio, but pulls the Boost
  distribution in.
- **libuv** is a C event-loop library: no RAII, callback-driven.

**Choice: standalone Asio 1.30.2**, header-only, fetched by CPM and pinned to an
exact version. We use two of its parts:

- `network::UdpSocket`: a non-blocking `asio::ip::udp::socket` that a loop
  polls, for the client and the tests;
- `network::NetworkThread`: the server's socket, served by its own thread
  running `io_context::run()` with asynchronous receives (§7). Only that one
  thread runs the `io_context`, so strands are not needed.

Asio is hidden behind these two classes (`src/network/Transport/`): game code
never includes Asio directly, so replacing it would only touch that folder.

---

## 3. Message format: hand-written binary codec

The subject requires a binary protocol. Within "binary", there are still
several ways to turn a C++ value into bytes.

| Criterion                   | JSON (text)         | `memcpy` of structs | Protocol Buffers | FlatBuffers    | **Our codec**   |
|-----------------------------|---------------------|---------------------|------------------|----------------|-----------------|
| Binary (subject)            | No                  | Yes                 | Yes              | Yes            | Yes             |
| One entity state            | ≈ 75 bytes          | 14 bytes (1 padding byte) | ≈ 15–20 bytes  | ≈ 30 bytes     | **13 bytes**    |
| Same bytes on every machine | Yes                 | **No**: padding, endianness, compiler | Yes | Yes     | Yes             |
| Safe on malformed input     | Parser-dependent    | **No**: trusts sizes | Yes             | With verifier  | Yes, tested     |
| Extra tooling               | A JSON library      | None                | `protoc` code generator | `flatc`  | None            |
| Readable spec for a new client | Easy             | Hard (layout is implicit) | `.proto` file | `.fbs` file | RFC-style tables |

- **JSON**: not binary, and about **6 times larger**. A snapshot of 30
  entities would take ≈ 2.3 KB, more than one safe datagram (1200 bytes): it
  would have to be split or fragmented, and a fragmented datagram is lost if
  any fragment is lost.
- **`memcpy` of a struct**: the classic mistake. The layout depends on the
  compiler's padding and on the CPU's byte order, so a Windows client and a
  Linux server could disagree; and reading a struct straight from untrusted
  bytes is how buffer overflows happen.
- **Protocol Buffers / FlatBuffers**: excellent, but they add a code generator
  to the build, hide the wire layout (the subject wants it documented so that
  anyone can write a client), and solve schema evolution problems we do not
  have at this size.

**Choice: a small hand-written codec** (`src/network/Serialization/`,
`src/network/Protocol/`):

- every field is written one by one, **big-endian** (network byte order), so
  the bytes are identical on every machine;
- **no floats**: positions and velocities are *quantized* to `i16` in ¼ world
  units, which is exact, compact and identical everywhere (floats would need
  a portable encoding);
- **fixed sizes and bounded counts**: names are 16 bytes, a snapshot holds at
  most 88 entities, so every datagram fits in **1200 bytes**, below the usual
  internet MTU (≈ 1500) with room for VPN/tunnel headers;
- the reader is a **bounded, "sticky" `ByteReader`**: any read past the end
  fails, and the decoder checks every rule of protocol §7 before building a
  packet. Malformed input can never crash the program or trigger a large
  allocation.

**Bandwidth** (UDP/IP headers of 28 bytes included):

| Traffic                              | Size per packet | Rate  | Per client     |
|--------------------------------------|-----------------|-------|----------------|
| Snapshot, 30 entities (server → client) | 473 bytes    | 30 Hz | ≈ 14.2 KB/s    |
| Input, 8 masks (client → server)     | 85 bytes        | 60 Hz | ≈ 5.1 KB/s     |

Encryption is included (24 bytes per packet). Four players need ≈ 57 KB/s of upload on the server: any home connection can
host a game.

---

## 4. Reliability: our own, on top of UDP

| Criterion                     | TCP for events     | ENet            | GameNetworkingSockets | yojimbo     | **Our `ReliableChannel`** |
|-------------------------------|--------------------|-----------------|-----------------------|-------------|---------------------------|
| Mixed reliable / unreliable   | Needs 2 transports | Yes (channels)  | Yes                   | Yes         | Yes                       |
| Head-of-line blocking         | Yes                | Per channel     | Per lane              | Per channel | Reliable events only      |
| Fits our own wire format      | —                  | No: its own protocol | No: its own protocol | No       | Yes                       |
| Encryption built in           | With TLS           | No              | Yes                   | Yes         | Not yet (#97)             |
| Size                          | —                  | Small C library | Large (Valve, needs OpenSSL/libsodium) | Medium | ≈ 380 lines |

ENet, GameNetworkingSockets and yojimbo are proven libraries, and the
algorithms we use are the same ones. We implemented them ourselves because:

1. those libraries **replace the protocol**: with them, `docs/protocol.md`
   would describe their format, not ours, and the protocol design is a graded
   part of the project;
2. what we need is small and fully specified (protocol §6), and implementing
   it lets us test every case: loss, duplicates, reordering, wrap-around;
3. a library with its own sockets and threads would fight the ECS game loop,
   which takes the received datagrams once per frame.

**How it works** (`src/network/Reliability/`):

| Problem                | Technique                                                         |
|------------------------|-------------------------------------------------------------------|
| Detect duplicates and order packets | 16-bit `sequence` per packet; "newer" uses RFC 1982 serial arithmetic, so 65535 → 0 works |
| Acknowledge cheaply    | Every packet carries `ack` (newest received) + a 32-bit `ackBits` field: one packet acknowledges 33, and a lost acknowledgement is repeated by the next packets |
| Deliver events surely  | Reliable messages are resent (new sequence, same `reliableId`) until acknowledged |
| Resend at the right time | After `max(100 ms, 2 × RTT)`; the round-trip time is smoothed like TCP (RFC 6298) |
| Deliver exactly once, in order | Receiver delivers by `reliableId`, buffers early ones, drops already-delivered ones |
| Bound memory           | Ring of 1024 sent packets; at most 64 buffered early messages; a **sending window** of 64 |
| Detect a dead peer     | A reliable message unacknowledged for 5 s                          |

The sending window was found by our own stress test (200 messages over a
network losing 40 % of packets): without it, losing one message while many
were in flight overflowed the receiver's buffer and disconnected a healthy
peer. The rule is now part of the protocol (§6.2).

---

## 5. Game state synchronization: authoritative server

| Model                     | Bandwidth          | Cheating            | Late join / crash | Latency feel     |
|---------------------------|--------------------|---------------------|-------------------|------------------|
| Lockstep (send inputs only, everyone simulates) | Lowest | Hard to cheat | Hard: a slow or crashed peer stalls everyone | Waits for the slowest |
| Client authority (each client sends its state) | Low | Trivial to cheat | Easy | Best |
| **Authoritative server** (inputs up, state down) | Medium | Server decides | Easy: the server just keeps going | Own ship reacts one round trip late without prediction |

The subject requires the server to "have authority on what happens in the
end", to keep running when a client crashes, and to notify the others. That
rules out lockstep (one crashed client stalls the game) and client authority.

- Clients send **inputs** (`INPUT`, 60 Hz). Each packet repeats the last 8
  inputs, so a lost datagram costs nothing.
- The server simulates and sends **snapshots** (continuous state, 30 Hz,
  unreliable) and **events** (discrete facts, reliable).
- Snapshots at 30 Hz instead of 60 halve the bandwidth; clients extrapolate
  with each entity's velocity between them.

---

## 6. Timing: fixed time step

A simulation that advances by the real frame duration gives different results
on different machines (a fast bullet can skip a thin enemy at 30 FPS but not
at 144 FPS). So that the game does not depend on the machine, the simulation
advances by a **fixed 1/60 s step** (`ecs::FixedTimestep`, `ecs::GameLoop`).

- An accumulator converts irregular frames into whole steps; leftover time is
  kept, so no time is lost.
- After a freeze, at most 5 steps are replayed (avoids the "spiral of death").
- The server sleeps until the next step: **≈ 0.2 % CPU** when idle, measured.
- Accumulators use `double`: with `float`, an uncapped client lost about 5 %
  of its steps (found by measurement, fixed and tested).

---

## 7. Concurrency

| Model                        | Simplicity | Scales to many clients | Data races       |
|------------------------------|------------|------------------------|------------------|
| One thread per client        | Simple to write | No (threads are heavy) | Many: shared game state |
| Async callbacks (Asio `run`) | Hard to follow | Yes                 | Need strands     |
| **One loop, non-blocking socket** | Simplest | Yes for 4 players, one socket for all | None   |
| Network thread + game thread (#45) | Medium  | Yes                  | One queue between them |

The subject: "The server MUST be multithreaded, so that it does not block or
wait for clients messages, as the game must run frame after frame on the
server regardless of clients' actions." The server has two threads (#45):

```
network thread                                game thread (main)
io_context.run(), owns the socket             GameLoop::run(), owns the ecs::World
  datagram received ──► inbox (mutex) ──────►   NetReceiveSystem: takeReceived()
  send_to()        ◄── asio::post queue ◄─────   sendPacket(): send()
```

- **One owner per piece of data.** The network thread alone touches the
  socket (Asio sockets must not be used from two threads at once); the game
  thread alone touches the World. Gameplay code takes no lock at all.
- **Two queues between them.** Received datagrams wait in an inbox behind a
  `std::mutex`, which the game thread empties with one swap per frame. Sends
  are posted to the `io_context` (`asio::post`, thread-safe) and run on the
  network thread, in order.
- **The network thread waits, the game thread never does.** It sleeps in
  `async_receive_from` until a datagram arrives: no polling, ~0% CPU when idle.
- **Flood.** The inbox holds at most 1024 datagrams; beyond, new ones are
  dropped and counted, so memory stays bounded and the queued datagrams of the
  players already there are kept.
- **Shutdown.** The last DISCONNECTs are queued, then the socket is closed by
  a task posted after them: they go out first, then `run()` returns and the
  thread is joined (`std::jthread`, so an exception never leaves it running).

Checked with ThreadSanitizer: no data race in the network and server tests.
Running several games in parallel (#71) will reuse this split.

---

## 8. Error handling

Network input is untrusted and failures are **expected** (malformed packets,
unreachable clients, ports in use). They are returned as values with C++23
`std::expected<T, Error>`, never thrown:

- the caller cannot forget to check: the value is only reachable through the
  `expected`;
- no hidden control flow and no cost on the hot path;
- the same style everywhere: codec (`DecodeError`), socket (`std::error_code`),
  reliability (`ChannelError`), command line (message string).

Exceptions are kept for programming errors (a zero time step, a system added
twice), which should fail loudly.

---

## 9. Algorithms and data structures

| Where                 | Technique                         | Why                                         |
|-----------------------|-----------------------------------|---------------------------------------------|
| `ByteReader/Writer`   | Bounded cursor, sticky failure    | One check after a whole message; no read past the end, ever |
| Quantization          | Fixed-point `i16`, ¼ unit         | Exact, compact, same on every CPU           |
| `isNewer`             | RFC 1982 serial arithmetic        | 16-bit counters wrap every ≈ 18 min at 60 Hz |
| Acknowledgements      | Sliding 32-bit bitfield           | 33 acks in 6 bytes, self-repairing          |
| Sent history          | Fixed ring buffer (1024)          | Constant memory, O(1) lookup by sequence    |
| RTT                   | Exponentially weighted moving average (1/8) | Stable estimate from noisy samples (RFC 6298) |
| Early reliable messages | Ordered map, capped at 64       | In-order delivery, bounded memory           |
| `Packet` payload      | `std::variant` + `std::visit`     | One type per packet, checked at compile time |
| Received packets on the server | Short-lived ECS entities (`ReceivedPacket`) | Fits the ECS: handlers are ordinary systems |
| Simulation clock      | Fixed-step accumulator            | Deterministic, frame-rate independent       |

**Designed by us** (rather than taken as-is): the wire format itself, the
reliable sending window (a gap in our first spec, see §4), and the
"packets as entities" glue between the network and the ECS. Everything else
is a standard, documented technique, cited above.

---

## 10. Storage

The network layer stores nothing on disk in Part 1: all of its state is in
memory and lives as long as a session. Its storage concern is **memory
bounds**, because an attacker controls what we receive:

| State                          | Bound                                     |
|--------------------------------|-------------------------------------------|
| Receive buffer                 | One 1200-byte array, reused, no allocation |
| Decoded packet                 | Allocation checked against the datagram size first |
| Datagrams read per frame       | 1024; the rest waits in the OS buffer     |
| Packets per address            | 120 per second (protocol §7.1) |
| Reliability state per client   | 1024 sent records + 64 early messages     |
| Sessions                       | 4 players                                 |

### Accounts (#73)

The server keeps the accounts in a text file, with one account per line
(`--accounts`, default `accounts.txt`):

```text
<kibibytes> <passes> <salt in hex> <hash in hex> <name>
```

| Criterion          | Text file (chosen)                  | SQLite                         | One JSON file               |
|--------------------|-------------------------------------|--------------------------------|-----------------------------|
| Dependency         | None                                | A library, on Linux and Windows | A library, or our own parser |
| Reading            | Loaded once, then a `std::map` in memory | SQL queries               | Loaded once                 |
| Writing            | The whole file, for each new account | One row                       | The whole file              |
| Crash during a write | Temporary file, then rename: the old file stays intact | Built in (journal) | Same as the text file   |
| Reading it by hand | Yes                                 | With a tool                    | Yes                         |

For a game server with few accounts, a new account is rare and small: rewriting
the file, the only disk access, on the game thread, takes 0.5 ms with 1,000 accounts
and 1.6 ms with 5,000 (measured on our VM). The drawback appears with many accounts or frequent writes: leaderboards
or friends (#91, #126) may justify moving to SQLite. `AccountStore` is the only
code that knows the format, so that change would stay inside it.

Known limits: the rename protects against a crash of the server, not fully
against a power cut, since the file is not forced to disk (`fsync` is not portable
in standard C++); the last account created could be lost, not the older ones.

**Reliability.** A missing file is created at startup, and a file that cannot be
read or written stops the server there rather than at the first sign-up. A damaged
line is skipped and the file is first copied to `<file>.damaged`, since the next
save would remove that line for good.

**Password hashing.** We compared how long one hash takes an attacker:

| Algorithm                 | Cost for an attacker                       | Decision |
|---------------------------|--------------------------------------------|----------|
| SHA-256, BLAKE2b          | Fast: built for speed, cheap on a GPU      | Rejected |
| bcrypt                    | Slow, but no memory cost; another library  | Rejected |
| **Argon2id** (Monocypher) | Slow and memory-hard, already in the project | Chosen |

Cost: 64 MiB and 3 passes, 130 ms measured on our VM (`-O2`), above the OWASP minimum (19 MiB,
2 passes). It is stored with each account, so raising it later does not break the
accounts that exist. Each account has a random salt: two identical passwords give
two different hashes. Hashes are compared in constant time.

The hash runs on its own thread, one at a time, so the game never waits for it and
a client cannot make the server use more than 64 MiB for hashing. At most 4 are
waiting, since there is one per session.

---

### Friends (#126)

Friends use a second text file (`--friends`, default `friends.txt`), for the reasons above.
It is apart from the accounts so that a mistake on friends can never damage a password hash.
The difference is when it is written: creating an account costs a slow hash, so a client cannot
do it often, but a friend request costs nothing, and the server accepts 120 packets per second
per address. Rewriting the whole file for each one would make the game wait for the disk. So
changes stay in memory and the file is written at most once per second, and when the server
stops. The price: a crash can lose the last second of changes, which a player can simply redo.

### Private messages (#92)

Messages for an offline friend wait in a third text file (`--messages`, default `messages.txt`),
written like the friends file and for the same reason. At most 100 wait per player, so the file
stays small and a message is read at most once, then forgotten. They are in clear: encrypting
them for their reader would need a key that the reader alone holds, which the accounts do not
have. This is written in the protocol as a known limit.

### Games in parallel and groups (#71, #72, #111)

Each game has its own `ecs::World` and `ecs::GameLoop` (`GameRoom`). The sessions stay in the
server's main World for the whole connection, and a player in a game also has an entity in the
World of that game, which points to their session.

| Option | For | Against |
|---|---|---|
| **One World per game (chosen)** | Two games cannot touch each other: no query can see the other's entities. The game systems do not change. The score and the enemies go with the room | One indirection from the player to their session |
| One World with a game number on each entity | No second World | Every system must filter, including the collisions of `game`: one mistake and a shot crosses into another game |
| Moving the session between Worlds | The game systems see a `NetClient` as before | Every network system, and the chat and friends, must look through every World |

Groups are plain data (`Groups`), kept in memory: a group is worth nothing after its last member
leaves. The rules are in that class, tested without a network; `GroupSystem` only reads the orders
and tells the members. Each member is told the group's state when it differs from the last one
they were sent, so no change can be forgotten, whatever its cause.

A code has 6 signs out of 32, about a billion values, without those easy to mistake when read
aloud. Trying them is limited to 5 wrong codes per 30 seconds and per account.

### Leaderboard (#91)

Finished games go to a fourth text file (`--scores`, default `scores.txt`), one line per game,
written at most once per second like the friends file. A game is a line, not a best score per
player, as asked: the same team can be in the 10 several times, and "your last game" can be
ranked even far below them. Each ranking keeps its games sorted, so an answer costs a copy of
10 lines and a binary search for the player's last game, whatever the number of games.

A game ends when nobody is left in it, and its team score then counts for everyone who started
it. Until lives exist (#145), leaving is the only end; with them, the score will be taken at
GAME_OVER instead, which changes only `RoomSystem`.

| Option | For | Against |
|---|---|---|
| **Text file, like the other stores (chosen)** | Same tools, same safe replace, readable by hand | The whole file is rewritten at each save |
| SQLite | Queries, no full rewrite | A new dependency for 10 lines and one rank |

## 11. Security

| Threat                              | Mitigation                                         | Status |
|-------------------------------------|----------------------------------------------------|--------|
| Malformed / truncated packets crash the server | Bounded reader, full validation (protocol §7), fuzz-style tests: every truncation, every single-bit flip, 20 000 random datagrams and 20 000 random payloads behind a valid header | Done |
| Huge counts trigger huge allocations | Counts checked against the bytes received before allocating | Done |
| Flood of garbage stalls the game    | At most 1024 datagrams per frame; drops counted, not logged one by one | Done |
| Flood fills the logs / disk         | Malformed packets are counted, never logged individually | Done |
| Packet injection into another session (spoofed address) | Random 32-bit session token from a secure source, checked with the address | Done |
| Reflection / amplification attacks  | Replies to unknown addresses are never larger than the request | Done |
| One client floods the server        | 120 packets/s per address                          | Done |
| Reliable-buffer exhaustion          | 64 early messages, then disconnect (`PROTOCOL_ERROR`) | Done |
| Eavesdropping, tampering, replay    | Authenticated encryption per session (#97)         | Done (active attacker during the connection: not covered, protocol §3.3) |
| Password read on the network        | `AUTH_REQUEST` always encrypted, fixed size (#73)  | Done, except against an active attacker during the connection (protocol §3.3) |
| Stolen accounts file                | Salted Argon2id hashes only, readable by the server's user only (#73) | Done |
| Guessing passwords online           | One attempt per connection + slow hash + 120 packets/s per address. No limit per account | Partly |
| Finding which names have an account | Same answer and same delay at login. `NAME_TAKEN` still reveals a name at sign-up, which is unavoidable | Partly |
| Keeping a player slot without an account | Session closed after 10 s without login (#73) | Done |

Main weaknesses of the technologies themselves:

- **UDP** has no handshake, so source addresses can be forged. Hence the
  session tokens and the no-amplification rule.
- **Asio** is a mature, widely used library; its main risk is an unpatched
  version. It is pinned to an exact release in `cmake/Dependencies.cmake`,
  so updates are deliberate and reviewed.
- **Our own codec** is the code most exposed to attackers, so it carries the
  heaviest tests (above), run under AddressSanitizer and UndefinedBehavior
  Sanitizer.

**Long-term monitoring:**

- the CI builds with warnings as errors and runs every test on each pull
  request (#7, PR #101), so a regression in validation cannot be merged unnoticed;
- the server counts dropped datagrams (`NetReceiveSystem::droppedCount()`): a
  sudden rise means an attack or a broken client. Nothing displays the count
  yet, because metrics (#69) were closed as not planned;
- dependency versions are pinned; their release notes and security advisories
  are checked when bumping them.
