# Technical study: networking

This page explains **why** the network layer of R-Type is built the way it is.
For each decision it lists the alternatives we considered, compares them, and
justifies the choice. The protocol itself is specified in
[`docs/protocol.md`](../protocol.md); the code lives in `src/network/` and
`src/server/`.

| Decision            | Choice                                        | Main alternatives                         |
|---------------------|-----------------------------------------------|-------------------------------------------|
| Transport           | UDP only                                      | TCP, UDP + TCP                            |
| Socket library      | Standalone Asio, non-blocking, polled         | Raw BSD sockets / Winsock, SFML Network, Boost.Asio, libuv |
| Message format      | Hand-written binary codec, big-endian         | JSON, raw `memcpy` of structs, Protocol Buffers, FlatBuffers |
| Reliability         | Our own: sequences, ack bitfield, resends     | TCP, ENet, GameNetworkingSockets, yojimbo |
| Game state sync     | Authoritative server, inputs up, snapshots + events down | Lockstep, client authority  |
| Timing              | Fixed 60 Hz simulation, 30 Hz snapshots       | Variable time step                        |
| Concurrency         | One thread, non-blocking socket (for now)     | One thread per client, async callbacks    |
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
exact version. We use only its simplest part: a non-blocking
`asio::ip::udp::socket` that the game loop polls once per frame. Asio's
asynchronous model (`io_context::run`, callbacks, strands) is not used: it is
powerful but much harder to read, and a 60 Hz loop that drains the socket
needs none of it.

Asio is hidden behind our own class, `network::UdpSocket`
(`src/network/Transport/`): game code never includes Asio directly, so
replacing it would only touch that folder.

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
  most 89 entities, so every datagram fits in **1200 bytes**, below the usual
  internet MTU (≈ 1500) with room for VPN/tunnel headers;
- the reader is a **bounded, "sticky" `ByteReader`**: any read past the end
  fails, and the decoder checks every rule of protocol §7 before building a
  packet. Malformed input can never crash the program or trigger a large
  allocation.

**Bandwidth** (UDP/IP headers of 28 bytes included):

| Traffic                              | Size per packet | Rate  | Per client     |
|--------------------------------------|-----------------|-------|----------------|
| Snapshot, 30 entities (server → client) | 449 bytes    | 30 Hz | ≈ 13.5 KB/s    |
| Input, 8 masks (client → server)     | 61 bytes        | 60 Hz | ≈ 3.7 KB/s     |

Four players need ≈ 54 KB/s of upload on the server: any home connection can
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
   which polls the network once per frame.

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
| **Authoritative server** (inputs up, state down) | Medium | Server decides | Easy: the server just keeps going | Needs prediction (#80) |

The subject requires the server to "have authority on what happens in the
end", to keep running when a client crashes, and to notify the others. That
rules out lockstep (one crashed client stalls the game) and client authority.

- Clients send **inputs** (`INPUT`, 60 Hz). Each packet repeats the last 8
  inputs, so a lost datagram costs nothing.
- The server simulates and sends **snapshots** (continuous state, 30 Hz,
  unreliable) and **events** (discrete facts, reliable).
- Snapshots at 30 Hz instead of 60 halve the bandwidth; clients interpolate
  between them (#81).

---

## 6. Timing: fixed time step

A simulation that advances by the real frame duration gives different results
on different machines (a fast bullet can skip a thin enemy at 30 FPS but not
at 144 FPS). Since the server is authoritative and clients will predict it
(#80), both must compute the same thing: the simulation advances by a **fixed
1/60 s step** (`ecs::FixedTimestep`, `ecs::GameLoop`).

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

Today the server runs one loop: each frame it drains the socket (at most 1024
datagrams, so a flood cannot stall a frame), simulates, then sends. Nothing
ever blocks, which already satisfies "the game must run frame after frame
regardless of clients". The subject also requires a multithreaded server:
#45 will move socket I/O to its own thread, connected to the game thread by a
queue, once the single-threaded version works and is tested.

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
| Packets per address            | 120 per second (protocol §7.1, with sessions #46) |
| Reliability state per client   | 1024 sent records + 64 early messages     |
| Sessions                       | 4 players                                 |

Persistent data (accounts, scores) belongs to Part 2 (#73, #91) and will be
studied there.

---

## 11. Security

| Threat                              | Mitigation                                         | Status |
|-------------------------------------|----------------------------------------------------|--------|
| Malformed / truncated packets crash the server | Bounded reader, full validation (protocol §7), fuzz-style tests: every truncation, every single-bit flip, 20 000 random datagrams and 20 000 random payloads behind a valid header | Done |
| Huge counts trigger huge allocations | Counts checked against the bytes received before allocating | Done |
| Flood of garbage stalls the game    | At most 1024 datagrams per frame; drops counted, not logged one by one | Done |
| Flood fills the logs / disk         | Malformed packets are counted, never logged individually | Done |
| Packet injection into another session (spoofed address) | Random 32-bit session token from a secure source, checked with the address | With #46 |
| Reflection / amplification attacks  | Replies to unknown addresses are never larger than the request | Specified (§7.1), with #46 |
| One client floods the server        | 120 packets/s per address                          | With #46 |
| Reliable-buffer exhaustion          | 64 early messages, then disconnect (`PROTOCOL_ERROR`) | Done |
| Eavesdropping, tampering            | Authenticated encryption per session (#97)         | Planned |

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
  sudden rise means an attack or a broken client, and will be exposed in the
  metrics and the admin console (#69, #92);
- dependency versions are pinned; their release notes and security advisories
  are checked when bumping them.
