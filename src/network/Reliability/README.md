# Reliability

| File                  | Content                                                     |
|-----------------------|-------------------------------------------------------------|
| `ReliableChannel.hpp` | `ReliableChannel`: sequences, acks, resends and ordering of one connection; `isNewer()` |

The implementation of [`docs/protocol.md` §6](../../../docs/protocol.md#6-reliability-and-ordering).
UDP may lose, duplicate or reorder datagrams. This module decides what to do
about it: snapshots can be lost (the next one replaces them), but events such
as `ENTITY_SPAWN` or `PLAYER_LEFT` must arrive, exactly once, in order.

A `ReliableChannel` is the state of **one** connection on **one** side: the
server keeps one per client, the client keeps one. It never touches a socket
and never reads a clock: the caller sends the packets it returns and passes
the current time, which makes every behaviour testable without a network.

---

## Sending

```cpp
// Unreliable (HEARTBEAT, INPUT, SNAPSHOT): sent once, may be lost
socket.sendTo(encode(channel.wrap(snapshot, now)), peer);

// Reliable (ENTITY_SPAWN, PLAYER_JOINED...): queued, sent until acknowledged
channel.queueReliable(EntitySpawn{...});

// Once per frame: new reliable messages + those due for a resend
for (const Packet& packet : channel.takeDue(now)) {
    socket.sendTo(encode(packet), peer);
}
if (channel.timedOut(now)) {
    // A reliable message stayed unacknowledged for 5 s: the peer is gone
}
```

Every packet gets the next **sequence** (0, 1, ... 65535, 0, ...) and carries
the acknowledgements of what was received from the peer (`ack` + `ackBits`).

A reliable message is resent, with a **new sequence but the same
`reliableId`**, when it is not acknowledged after `max(100 ms, 2 × RTT)`.
The round-trip time (RTT) is measured from the acknowledgements and smoothed
like TCP does (each sample moves it by 1/8).

**Sending window:** a message more than 64 `reliableId`s ahead of the oldest
unacknowledged one waits. The receiver only buffers 64 messages ahead of the
one it waits for, so sending further would get a healthy peer disconnected
when a single message is lost.

## Receiving

```cpp
auto payloads = channel.receive(packet, now); // after decode() and the session check
if (!payloads) {
    // ReliableBufferFull: the peer misbehaves, disconnect it (PROTOCOL_ERROR)
}
for (const Payload& payload : *payloads) {
    // handle it
}
```

| What arrives                                    | What `receive()` returns           |
|-------------------------------------------------|------------------------------------|
| A new packet                                    | Its payload                        |
| A duplicate (sequence already received)         | Nothing                            |
| A packet more than 32 sequences old             | Nothing (cannot tell if duplicate) |
| A late but new packet                           | Its payload (unreliable order is not enforced) |
| A reliable message already delivered            | Nothing (its ack was lost, it was resent) |
| A reliable message arriving early               | Nothing yet: buffered              |
| The reliable message everyone was waiting for   | It, then every buffered one after it |

---

## Wrap-around

Sequences and `reliableId`s are 16-bit: after 65535 comes 0. "Newer" is
therefore not `a > b` but `isNewer(a, b)`, RFC 1982 serial arithmetic:
`a` is newer when it is ahead of `b` by at most half the range.

```cpp
static_assert(isNewer(1, 0));
static_assert(isNewer(0, 65535)); // 0 comes right after 65535
```

## Bounded memory

| State                    | Bound                                                    |
|--------------------------|----------------------------------------------------------|
| Sent packets (for acks and RTT) | Ring of 1024 entries indexed by sequence          |
| Early reliable messages  | 64, then `ReliableBufferFull`                            |
| Pending reliable messages | Until acknowledged; the peer times out after 5 s       |

Tests: [`tests/network/Reliability/`](../../../tests/network/Reliability).
