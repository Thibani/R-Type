# Server

`r-type_server`, the authoritative game server: whatever it decides, the
clients comply with.

| File                                | Content                                                  |
|-------------------------------------|----------------------------------------------------------|
| `main.cpp`                          | Entry point: options, network thread, World, GameLoop, Ctrl+C |
| `ServerOptions.hpp`                 | Command line parsing (`-p`, `-h`)                        |
| `ServerSystems.hpp`                 | `installSystems()`: every system, in order; `installSimulation()` |
| `Spawn.hpp`                         | Create ships, missiles and Bydos with all their components |
| `Network.hpp`                       | Frame time → time point, send a packet, shutdown notice  |
| `Components/ReceivedPacket.hpp`     | A valid packet received this frame; `IncompatibleClient` |
| `Components/NetClient.hpp`          | A connected player's session; `Disconnecting`            |
| `Components/ClientMessage.hpp`      | A game payload from a connected player                   |
| `Components/Player.hpp`             | `PlayerInput`, `RespawnTimer` (on the player), `PlayerShip` (on the ship) |
| `Components/Combat.hpp`             | `FireCooldown`, `Bydo`, `BydoSpawner`, `TeamScore`       |
| `Components/NetEntity.hpp`          | `NetEntity` (network id), `NetIdCounter`, `SimulationTick`, `LastSnapshot` |
| `Singleton.hpp`                     | `singleton<T>(world)`: state shared by several systems   |
| `NetIds.hpp`                        | `allocateNetId()`: ids for networked entities (§5.1)     |
| `Systems/NetReceiveSystem.hpp`      | Received datagrams → `ReceivedPacket`, rate limit, drops malformed |
| `Systems/ConnectionSystem.hpp`      | CONNECT_REQUEST → session + ACCEPT, or REJECT            |
| `Systems/SessionReceiveSystem.hpp`  | Packets → their session's channel → `ClientMessage`      |
| `Systems/TimeoutSystem.hpp`         | Silent for 5 s → `Disconnecting`                         |
| `Systems/SessionCleanupSystem.hpp`  | `Disconnecting` → PLAYER_LEFT to the others, entity gone |
| `Systems/InputSystem.hpp`           | INPUT → the player's `PlayerInput`                       |
| `Systems/PlayerShipSystem.hpp`      | One ship per connected player, back 3 s after its death  |
| `Systems/TickSystem.hpp`            | Counts simulation steps                                  |
| `Systems/PlayerControlSystem.hpp`   | `PlayerInput` → ship `Velocity` (normalised diagonals)   |
| `Systems/PlayerFireSystem.hpp`      | FIRE held → a player missile every 0.25 s                |
| `Systems/BydoSpawnSystem.hpp`       | A Bydo from the right every 1.5 s, while ships exist     |
| `Systems/BydoMovementSystem.hpp`    | Bydos fly left in a wave                                 |
| `Systems/BydoFireSystem.hpp`        | Each Bydo fires at the nearest ship every 2.5 s          |
| `Systems/PlayerBoundsSystem.hpp`    | Ships kept inside the arena                              |
| `Systems/DamageSystem.hpp`          | Collisions → hits, kills, team score, respawn timer      |
| `Systems/ReplicationSystem.hpp`     | ENTITY_SPAWN / ENTITY_DESTROY (real reason), PLAYER_DIED, GAME_STATE |
| `Systems/NetSendSystem.hpp`         | Reliable messages, resends, heartbeats                   |
| `Systems/SnapshotSystem.hpp`        | SNAPSHOT of every networked entity, 30 Hz                |
| `Systems/DiscardPacketsSystem.hpp`  | Destroys the messages no system handled                  |

Everything except `main.cpp` is the `rtype_server` library, so the tests can
use the systems directly.

---

## Startup and shutdown

1. Parse the command line. Invalid → usage on stderr, exit 84.
2. Open the `NetworkThread` on the port: it binds the UDP socket and starts
   the network thread. Port in use → message, exit 84.
3. Build an `ecs::World` and an `ecs::GameLoop` that sleeps between steps
   (no busy waiting, ~0% CPU when idle).
4. `installSystems()`, then install the `SIGINT` / `SIGTERM` handler.
5. `loop.run()` until `Ctrl+C`.
6. Queue DISCONNECT (SERVER_SHUTDOWN) 3 times for every client, then stop the
   network thread: it sends them first, then is joined. Exit 0.

**Two threads.** The game runs on the main thread, which owns the whole
`ecs::World`; the network thread only moves datagrams, so the game never
waits for clients. Every system runs on the game thread and none takes a
lock. See `docs/technical-study/network.md` §7.

Any exception that escapes is caught in `main`: message on stderr, exit 84.

---

## Systems, in order

```
frameStart()  NetReceiveSystem        network thread inbox → ReceivedPacket / IncompatibleClient
              ConnectionSystem        CONNECT_REQUEST → NetClient entity, ACCEPT / REJECT
              SessionReceiveSystem    packet → its session's channel → ClientMessage
              TimeoutSystem           silent session → Disconnecting
              InputSystem             ClientMessage (INPUT) → the player's PlayerInput
              PlayerShipSystem        one ship per connected player, after its respawn delay
simulation    TickSystem              SimulationTick + 1, first of every step
              PlayerControlSystem     PlayerInput → ship Velocity
              PlayerFireSystem        FIRE → player missile (cooldown 0.25 s)
              BydoSpawnSystem         a Bydo enters from the right every 1.5 s
              BydoMovementSystem      Bydo Velocity: left, in a wave
              BydoFireSystem          Bydo missile at the nearest ship every 2.5 s
              game::MovementSystem    Position += Velocity × step (shared with the client)
              PlayerBoundsSystem      ships kept inside the arena, velocity into an edge → 0
              game::ArenaExitSystem   missiles and Bydos that left → DespawnRequest (OUT_OF_BOUNDS)
              game::CollisionSystem   overlapping hitboxes → CollisionEvent
              DamageSystem            CollisionEvent → −1 HP, kill → DespawnRequest (KILLED), score
              game::DespawnSystem     DespawnRequest → DespawnEvent, entity destroyed
frameEnd()    SessionCleanupSystem    Disconnecting → PLAYER_LEFT, entity destroyed
              ReplicationSystem       new NetEntity → ENTITY_SPAWN, gone → ENTITY_DESTROY,
                                      ship killed → PLAYER_DIED, score changed → GAME_STATE
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

## The first game (#54 – #61)

Everything that moves is a networked entity made by `Spawn.hpp`, so
`ReplicationSystem` and `SnapshotSystem` send it without extra code:

| Entity          | Kind             | Hitbox hits         | Health | Leaves when                         |
|-----------------|------------------|---------------------|--------|-------------------------------------|
| Player ship     | `PLAYER_SHIP`    | Bydos, Bydo missiles | 3     | killed (back 3 s later, full health) |
| Player missile  | `PLAYER_MISSILE` | Bydos               | –      | it hits, or leaves the arena        |
| Bydo            | `BYDO`           | ships, player missiles | 1   | killed (+100 team score), or leaves on the left |
| Bydo missile    | `ENEMY_MISSILE`  | ships               | –      | it hits, or leaves the arena        |

The rules (speeds, sizes, rates, points) are constants in `src/game/Ship.hpp`
and `src/game/Bydo.hpp`, shared with the client.

**Systems talk through events, never through calls.** `game::CollisionSystem`
only detects overlaps (`CollisionEvent`); `DamageSystem` decides what they
mean and asks for despawns (`DespawnRequest`); `game::DespawnSystem` announces
them (`DespawnEvent`, the entity still alive) and destroys the entity;
`ReplicationSystem` listens to `DespawnEvent` to send ENTITY_DESTROY with the
real reason and killer, and PLAYER_DIED for a ship.

**What clients are told:**

| Event in the game              | Messages                                          |
|--------------------------------|---------------------------------------------------|
| A player fires                 | ENTITY_SPAWN `PLAYER_MISSILE`, `owner` = the shooter |
| A Bydo appears / fires         | ENTITY_SPAWN `BYDO` / `ENEMY_MISSILE`             |
| Something is hit               | its `health` drops in the next SNAPSHOT           |
| Something is killed            | ENTITY_DESTROY `KILLED`, `killerNetId`            |
| Something leaves the arena     | ENTITY_DESTROY `OUT_OF_BOUNDS`                    |
| A ship is killed               | PLAYER_DIED (`playerId`, `killerNetId`), then ENTITY_DESTROY |
| The team score changes         | GAME_STATE (`RUNNING`, score); a newcomer gets it at once |

shortcut: the match never ends and ships come back forever; lobby, game over
and the return to the lobby are the game phases of #110.

Tests: `tests/server/GameplayTests.cpp` (the rules, step by step) and
`Session.FiringAndBydosReachTheClients` (over UDP).

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
| Flood from everywhere       | The network thread queues 1024 datagrams at most; the rest is dropped, counted |
| Injection into a session    | Address *and* random 32-bit token must match                |
| Reflection / amplification  | A REJECT (21 bytes) only answers a request at least as large |
| Guessable tokens            | `std::random_device`, backed by the OS secure generator     |

Tests: [`tests/server/`](../../tests/server).
