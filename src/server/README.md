# Server

`r-type_server`, the authoritative game server: whatever it decides, the
clients comply with.

| File                                | Content                                                  |
|-------------------------------------|----------------------------------------------------------|
| `main.cpp`                          | Entry point: options, socket, World, GameLoop, Ctrl+C    |
| `ServerOptions.hpp`                 | Command line parsing (`-p`, `-h`)                        |
| `ServerSystems.hpp`                 | `installSystems()`: every system, in order               |
| `Network.hpp`                       | Frame time → time point, send a packet, shutdown notice  |
| `Components/ReceivedPacket.hpp`     | A valid packet received this frame; `IncompatibleClient` |
| `Components/NetClient.hpp`          | A connected player's session; `Disconnecting`            |
| `Components/ClientMessage.hpp`      | A game payload from a connected player                   |
| `Components/Player.hpp`             | `PlayerInput` (on the player), `PlayerShip` (on the ship) |
| `Components/NetEntity.hpp`          | `NetEntity` (network id), `NetIdCounter`, `SimulationTick`, `LastSnapshot` |
| `Singleton.hpp`                     | `singleton<T>(world)`: state shared by several systems   |
| `NetIds.hpp`                        | `allocateNetId()`: ids for networked entities (§5.1)     |
| `Systems/NetReceiveSystem.hpp`      | Socket → `ReceivedPacket`, rate limit, drops malformed   |
| `Systems/ConnectionSystem.hpp`      | CONNECT_REQUEST → session + ACCEPT, or REJECT            |
| `Systems/SessionReceiveSystem.hpp`  | Packets → their session's channel → `ClientMessage`      |
| `Systems/TimeoutSystem.hpp`         | Silent for 5 s → `Disconnecting`                         |
| `Systems/SessionCleanupSystem.hpp`  | `Disconnecting` → PLAYER_LEFT to the others, entity gone |
| `Systems/InputSystem.hpp`           | INPUT → the player's `PlayerInput`                       |
| `Systems/PlayerShipSystem.hpp`      | One ship per connected player                            |
| `Systems/TickSystem.hpp`            | Counts simulation steps                                  |
| `Systems/PlayerControlSystem.hpp`   | `PlayerInput` → ship `Velocity` (normalised diagonals)   |
| `Systems/PlayerBoundsSystem.hpp`    | Ships kept inside the arena                              |
| `Systems/ReplicationSystem.hpp`     | ENTITY_SPAWN / ENTITY_DESTROY, so clients know who exists |
| `Systems/NetSendSystem.hpp`         | Reliable messages, resends, heartbeats                   |
| `Systems/SnapshotSystem.hpp`        | SNAPSHOT of every networked entity, 30 Hz                |
| `Systems/DiscardPacketsSystem.hpp`  | Destroys the messages no system handled                  |

Everything except `main.cpp` is the `rtype_server` library, so the tests can
use the systems directly.

---

## Startup and shutdown

1. Parse the command line. Invalid → usage on stderr, exit 84.
2. Open the UDP socket on the port. Port in use → message, exit 84.
3. Build an `ecs::World` and an `ecs::GameLoop` that sleeps between steps
   (no busy waiting, ~0% CPU when idle).
4. `installSystems()`, then install the `SIGINT` / `SIGTERM` handler.
5. `loop.run()` until `Ctrl+C`.
6. Send DISCONNECT (SERVER_SHUTDOWN) 3 times to every client, exit 0.

Any exception that escapes is caught in `main`: message on stderr, exit 84.

---

## Systems, in order

```
frameStart()  NetReceiveSystem        socket → ReceivedPacket / IncompatibleClient
              ConnectionSystem        CONNECT_REQUEST → NetClient entity, ACCEPT / REJECT
              SessionReceiveSystem    packet → its session's channel → ClientMessage
              TimeoutSystem           silent session → Disconnecting
              InputSystem             ClientMessage (INPUT) → the player's PlayerInput
              PlayerShipSystem        one ship per connected player
simulation    TickSystem              SimulationTick + 1, first of every step
              PlayerControlSystem     PlayerInput → ship Velocity
              game::MovementSystem    Position += Velocity × step (shared with the client)
              PlayerBoundsSystem      ships kept inside the arena, velocity into an edge → 0
frameEnd()    SessionCleanupSystem    Disconnecting → PLAYER_LEFT, entity destroyed
              ReplicationSystem       new NetEntity → ENTITY_SPAWN, gone → ENTITY_DESTROY
              NetSendSystem           reliable messages, resends, heartbeats
              SnapshotSystem          every 2 steps: SNAPSHOT of all NetEntity, to everyone
              DiscardPacketsSystem    unhandled messages destroyed
```

`main` and the tests both call `installSystems()`, so the tests always run
the same server, in the same order.

### Messages are entities

Every received packet becomes an entity with a `ReceivedPacket` component.
Systems that care about it query it, act, and destroy the entity. Once the
session is checked, game payloads become `ClientMessage` entities in the same
way, carrying the entity of the player who sent them:

```cpp
world.query<ClientMessage>([&](ecs::Entity entity, ClientMessage& message) {
    if (const auto* input = std::get_if<Input>(&message.payload)) {
        // message.client is the player's entity
        handled.push_back(entity); // destroyed after the query
    }
});
```

**Message handlers must run in `frameStart()`.** A frame can run zero
simulation steps (fast frame), so a message handled by a simulation system
could be discarded before it is ever seen. Handlers turn messages into state
(a player's latest input...) that the simulation then reads.

---

## Game state: the server decides

The server is **authoritative**: clients send which keys are pressed, never
where things are. There is no packet a client could use to move its ship
directly; only the server's systems change `Position`.

```
player entity   NetClient, PlayerInput            the person: session + latest keys
ship entity     PlayerShip, Position, Velocity,   what is in the game: can die (#59)
                NetEntity                          while its player stays connected
```

| Data                | Where                                   | Written by                 |
|---------------------|-----------------------------------------|----------------------------|
| Latest input        | `PlayerInput` on the player             | `InputSystem`              |
| Ship velocity       | `game::Velocity` on the ship            | `PlayerControlSystem`, `PlayerBoundsSystem` |
| Ship position       | `game::Position` on the ship            | `game::MovementSystem`, `PlayerBoundsSystem` |
| Network identity    | `NetEntity {netId, kind, owner}` on the ship | `PlayerShipSystem`    |
| Simulation tick     | `SimulationTick` singleton              | `TickSystem`               |

**Inputs (§4.2, §7.1).** Each INPUT repeats the client's last masks, so a lost
datagram costs nothing. For each player, only ticks newer than the last one
applied count; ticks more than 120 ahead are ignored, so a client cannot jump
its input clock. The newest mask wins (replaying every tick comes with
client-side prediction, #80).

**Network ids (§5.1).** Every networked entity gets a `netId`, distinct from
its ECS handle. Ids are taken in a cycle, skipping those in use and `0xFFFF`
("none"), so a freed id is not reused within 5 seconds.

**What clients see (§4.3).** Clients only display what the server sends:

| Message        | When                                           | How                                |
|----------------|------------------------------------------------|------------------------------------|
| ENTITY_SPAWN   | A `NetEntity` the client was not told about (a new one, or every one for a newcomer) | Reliable, `ReplicationSystem` |
| ENTITY_DESTROY | A netId the client knows is gone               | Reliable, `ReplicationSystem`      |
| SNAPSHOT       | Every 2 simulation steps (30 Hz)               | Unreliable, `SnapshotSystem`; split every 89 entities |

Each `NetClient` keeps the netIds it was told about (`knownEntities`), so
there is a single rule for newcomers and for new entities, and any future
networked entity (missiles, Bydos) is replicated without new code. Each
player gets its own `lastInputTick` in the snapshot, so it knows which of
its inputs the state already includes.

**Singletons.** State shared by several systems (the tick, the netId counter)
lives on its own entity, reached with `singleton<T>(world)`: no globals, no
state hidden in a system.

---

## Sessions (docs/protocol.md §8)

A connected player is an entity with a `NetClient`: its address, its random
token, its `playerId` (0–3), its name, and a `ReliableChannel` that numbers,
acknowledges and resends its packets.

| Event                                    | Server answer                                         |
|------------------------------------------|-------------------------------------------------------|
| CONNECT_REQUEST, room left               | New session; CONNECT_ACCEPT; PLAYER_JOINED to everyone already here, and about each of them to the newcomer |
| Same address asks again                  | The same CONNECT_ACCEPT (the first one may be lost)   |
| 4 players already                        | CONNECT_REJECT (SERVER_FULL)                          |
| Empty name                               | CONNECT_REJECT (INVALID_NAME)                         |
| Other protocol version                   | CONNECT_REJECT (VERSION_MISMATCH)                     |
| Wrong token for the address, or unknown address | Dropped, counted                               |
| HEARTBEAT                                | Session kept alive                                    |
| DISCONNECT                               | Session closed; PLAYER_LEFT (QUIT) to the others      |
| Nothing received for 5 s, or a reliable message unacknowledged for 5 s | Session closed; PLAYER_LEFT (TIMEOUT) to the others |
| Reliable reorder buffer overflow         | DISCONNECT (PROTOCOL_ERROR); PLAYER_LEFT (KICKED)     |
| Server sent nothing for 250 ms           | HEARTBEAT                                             |

A crashed client is a timeout: the server keeps running and tells the others.

**Time:** systems never read a clock. They turn the frame's `ecs::Time` into
a time point (`toTimePoint()`), so the tests can jump 5 seconds ahead
instantly.

---

## Protection against hostile traffic

| Threat                      | Protection                                                  |
|-----------------------------|-------------------------------------------------------------|
| Malformed datagram          | `decode()` rejects it (protocol §7): dropped, counted, never logged one by one |
| Packet only servers may send | Rejected by `decode(…, ClientToServer)`                    |
| Flood from one address      | 120 datagrams per second and per address, the rest dropped  |
| Flood from everywhere       | 1024 datagrams per frame at most; the rest waits            |
| Injection into a session    | Address *and* random 32-bit token must match                |
| Reflection / amplification  | A REJECT (21 bytes) only answers a request at least as large |
| Guessable tokens            | `std::random_device`, backed by the OS secure generator     |

Tests: [`tests/server/`](../../tests/server).
