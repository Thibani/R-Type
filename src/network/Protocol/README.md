# Protocol

| File              | Content                                                       |
|-------------------|---------------------------------------------------------------|
| `Protocol.hpp`    | Constants, enumerations, quantization helpers                 |
| `Packets.hpp`     | One struct per packet type, `Packet`, `PacketHeader`          |
| `PacketCodec.hpp` | `encode()` / `decode()`: packets to datagrams and back        |

The specification is [`docs/protocol.md`](../../../docs/protocol.md). This
folder is its C++ implementation: when one changes, the other must too.

---

## Sending

```cpp
using namespace network::protocol;

Packet packet{
    .sessionToken = session.token,
    .sequence = session.nextSequence++,
    .ack = session.currentAck(),           // std::nullopt before anything was received
    .reliableId = 0,                       // only used by reliable payloads
    .payload = EntityDestroy{.netId = 12, .reason = DestroyReason::Killed, .killerNetId = 4},
};

DatagramBuffer buffer{};
if (const auto size = encode(packet, buffer)) {
    socket.send(std::span(buffer).first(*size));
}
```

The packet type, payload size and RELIABLE flag are derived from the payload.
`encode()` refuses a payload the receiver would reject (name too long,
player id out of range, ...).

## Receiving

```cpp
const auto packet = decode(datagram, Direction::ClientToServer); // on the server
if (!packet) {
    log("dropped: {}", toString(packet.error()));
    return;
}
std::visit(overloaded{
    [&](const Input& input) { /* ... */ },
    [&](const Disconnect& disconnect) { /* ... */ },
    [](const auto&) {},
}, packet->payload);
```

`decode()` applies every rule of §7 of the specification that does not need
session state, and reports the first failure as a `DecodeError`. The
connection layer still has to check that `sessionToken` matches the sender.

To answer a `CONNECT_REQUEST` of another version with `VERSION_MISMATCH`,
use `decodeHeader()`: it reads the header without checking the version.

---

## Safety guarantees

- The decoder never reads outside the datagram (`ByteReader`).
- Counts (`INPUT` masks, `SNAPSHOT` entities) are checked against the
  received size **before** allocating, so a datagram can never make the
  decoder allocate more than its own size.
- Every enumeration, player id and name is validated.
- `encode()` and `decode()` share the same `validate()` functions: what one
  produces, the other accepts.

The robustness tests feed the decoder every truncation and single-bit
corruption of every packet type, plus 40 000 random datagrams.

Tests: [`tests/network/Protocol/`](../../../tests/network/Protocol).
