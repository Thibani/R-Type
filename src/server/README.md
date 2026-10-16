# Server

`r-type_server`, the authoritative game server: whatever it decides, the
clients comply with.

| File                               | Content                                                   |
|------------------------------------|-----------------------------------------------------------|
| `main.cpp`                         | Entry point: options, socket, World, GameLoop, Ctrl+C     |
| `ServerOptions.hpp`                | Command line parsing (`-p`, `-h`)                         |
| `Components/ReceivedPacket.hpp`    | A valid packet received this frame, on its own entity     |
| `Systems/NetReceiveSystem.hpp`     | Socket → `ReceivedPacket` entities, drops malformed ones  |
| `Systems/DiscardPacketsSystem.hpp` | Destroys the packets no system handled                    |

Everything except `main.cpp` is the `rtype_server` library, so the tests can
use the systems directly.

---

## Startup

1. Parse the command line. Invalid → usage on stderr, exit 84.
2. Open the UDP socket on the port. Port in use → message, exit 84.
3. Build an `ecs::World` and an `ecs::GameLoop` that sleeps between steps
   (no busy waiting, ~0% CPU when idle).
4. Register the systems (below), install the `SIGINT` / `SIGTERM` handler.
5. `loop.run()` until `Ctrl+C`, then exit 0.

Any exception that escapes is caught in `main`: message on stderr, exit 84.

---

## Systems, in order

```
frameStart()  NetReceiveSystem        socket → ReceivedPacket entities
              (packet handlers)       #46: connection, input... consume them
simulation    (gameplay systems)      60 Hz fixed steps
frameEnd()    (snapshot / send)       #60
              DiscardPacketsSystem    destroy the ReceivedPacket left over
```

### Received packets are entities

Every valid datagram becomes an entity with a `ReceivedPacket` component
(sender + decoded packet). Systems that care about a packet type query it,
act, and destroy the entity:

```cpp
world.query<ReceivedPacket>([&](ecs::Entity entity, ReceivedPacket& received) {
    if (const auto* request = std::get_if<ConnectRequest>(&received.packet.payload)) {
        // ...
        handled.push_back(entity); // destroyed after the query
    }
});
```

**Packet handlers must run in `frameStart()`, after `NetReceiveSystem`.** A
frame can run zero simulation steps (fast frame), so a packet handled by a
simulation system could be discarded before it is ever seen. Handlers turn
packets into state (a player's latest input, a new player entity...) that the
simulation then reads.

### Malformed packets

`NetReceiveSystem` decodes with `Direction::ClientToServer`, so everything
`decode()` rejects (docs/protocol.md §7), including packets only the server
may send, is dropped and counted in `droppedCount()`, never logged one by
one: a flood of garbage must not flood the logs.

At most `MaxDatagramsPerFrame` (1024) datagrams are read per frame, so a flood
cannot keep a frame from ending; the rest waits in the OS buffer.

Tests: [`tests/server/`](../../tests/server).
