# Server

`r-type_server`, the authoritative game server: whatever it decides, the
clients comply with.

| File                                | Content                                                  |
|-------------------------------------|----------------------------------------------------------|
| `main.cpp`                          | Entry point: options, network thread, World, GameLoop, Ctrl+C |
| `ServerOptions.hpp`                 | Command line parsing (`-p`, `--accounts`, `-h`)          |
| `AccountStore.hpp`                  | The accounts, in memory and in their file (no ECS, no network) |
| `ServerSystems.hpp`                 | `installSystems()`: every system, in order; `installSimulation()` |
| `Spawn.hpp`                         | The weapons as data (`weapons::`); create ships, Bydos, projectiles |
| `Run.hpp`                           | `currentState()` (what GAME_STATE says), `startNewGame()` |
| `Network.hpp`                       | Frame time → time point, send a packet, shutdown notice  |
| `Components/ReceivedPacket.hpp`     | A valid packet received this frame; `IncompatibleClient` |
| `Components/NetClient.hpp`          | A session; `LoggedIn` (part of the game); `Disconnecting` |
| `Components/ClientMessage.hpp`      | A game payload from a connected player                   |
| `Components/Player.hpp`             | `PlayerInput`, `RespawnTimer`, `Eliminated` (on the player), `PlayerShip` (on the ship) |
| `Components/Match.hpp`              | `Match`: game running or over, team lives left           |
| `Components/Combat.hpp`             | Combat as data: `Weapon`, `Projectile`, `Trigger`, `Damage`, `ScoreValue`; `TeamScore` |
| `Components/Waves.hpp`              | The endless mode's state: `RunProgress` (time, level), `WaveDirector` (next arrival, pending members) |
| `Components/Behaviour.hpp`          | Enemy behaviours as data: `Enemy`, `WaveFlight`, `Seeker`, `AimsAtShips` |
| `Components/Gadgets.hpp`            | `Pickup`, `Force`, `BlocksMissiles`, `DropTable` (random drops) |
| `Components/NetEntity.hpp`          | `NetEntity` (network id), `NetIdCounter`, `SimulationTick`, `LastSnapshot` |
| `Singleton.hpp`                     | `singleton<T>(world)`: state shared by several systems   |
| `NetIds.hpp`                        | `allocateNetId()`: ids for networked entities (§5.1)     |
| `Systems/NetReceiveSystem.hpp`      | Received datagrams → `ReceivedPacket`, rate limit, drops malformed |
| `Systems/ConnectionSystem.hpp`      | CONNECT_REQUEST → session + ACCEPT, or REJECT            |
| `Systems/SessionReceiveSystem.hpp`  | Packets → their session's channel → `ClientMessage`      |
| `Systems/LoginSystem.hpp`           | AUTH_REQUEST → password hashed on its own thread → AUTH_RESULT, player admitted |
| `Systems/TimeoutSystem.hpp`         | Silent for 5 s → `Disconnecting`                         |
| `Systems/SessionCleanupSystem.hpp`  | `Disconnecting` → PLAYER_LEFT to the others, entity gone |
| `Systems/InputSystem.hpp`           | INPUT → the player's `PlayerInput`                       |
| `Systems/RestartSystem.hpp`         | RESTART after GAME OVER → a new game                     |
| `Systems/PlayerShipSystem.hpp`      | One ship per player still in the game, back 3 s after its death |
| `Systems/TickSystem.hpp`            | Counts simulation steps                                  |
| `Systems/MatchSystem.hpp`           | No life and no ship left → GAME OVER; nobody left → new game |
| `Systems/PlayerControlSystem.hpp`   | `PlayerInput` → ship `Velocity` (normalised diagonals)   |
| `Systems/ProgressSystem.hpp`        | Seconds played (while ships exist) → level, distance points |
| `Systems/WaveSystem.hpp`            | Endless enemies and formations from the right, faster each level |
| `Systems/WaveFlightSystem.hpp`      | `WaveFlight` enemies fly left in a wave                  |
| `Systems/SeekerSystem.hpp`          | `Seeker` enemies fly left, steering towards a ship       |
| `Systems/CircleFlightSystem.hpp`    | `CircleFlight` enemies loop while drifting left          |
| `Systems/HopSystem.hpp`             | `Hop` enemies jump along the floor                       |
| `Systems/HoverSystem.hpp`           | `Hover` enemies stop, hold, then leave (Gunner, boss)    |
| `Systems/BossSystem.hpp`            | The boss: arrival every 90 s of play, attacks, defeat    |
| `Systems/AimSystem.hpp`             | `AimsAtShips` enemies pull their trigger, aimed at the nearest ship |
| `Systems/WeaponSystem.hpp`          | Any pulled `Trigger` + ready `Weapon` → a projectile     |
| `Systems/DamageSystem.hpp`          | Collisions + `Damage` → hits, kills, team score          |
| `Systems/LivesSystem.hpp`           | Ship killed → one team life less, respawn or `Eliminated` |
| `Systems/DeathBurstSystem.hpp`      | `BurstOnDeath` killed → projectiles all around (Mine)    |
| `Systems/ForceSystem.hpp`           | The Force: POD → next slot, follows its ship, fires with it, stops enemy missiles |
| `Systems/PickupSystem.hpp`          | Ship touches a pickup → repaired, or a Force (points if it has one) |
| `Systems/DropSystem.hpp`            | Enemy killed → maybe a repair kit or a Force capsule; the boss drops both |
| `Systems/ReplicationSystem.hpp`     | ENTITY_SPAWN / ENTITY_DESTROY (real reason), PLAYER_DIED, GAME_STATE |
| `Systems/NetSendSystem.hpp`         | Reliable messages, resends, heartbeats                   |
| `Systems/SnapshotSystem.hpp`        | SNAPSHOT of every networked entity, 30 Hz                |
| `Systems/DiscardPacketsSystem.hpp`  | Destroys the messages no system handled                  |

Everything except `main.cpp` is the `rtype_server` library, so the tests can
use the systems directly.

---

## Startup and shutdown

1. Parse the command line. Invalid → usage on stderr, exit 84. Then load the
   accounts file. It cannot be read or written → message, exit 84.
2. Open the `NetworkThread` on the port: it binds the UDP socket and starts
   the network thread. Port in use → message, exit 84.
3. Build an `ecs::World` and an `ecs::GameLoop` that sleeps between steps
   (no busy waiting, ~0% CPU when idle).
4. `installSystems()`, then install the `SIGINT` / `SIGTERM` handler.
5. `loop.run()` until `Ctrl+C`.
6. Queue DISCONNECT (SERVER_SHUTDOWN) 3 times for every client, then stop the
   network thread: it sends them first, then is joined. Exit 0.

**Threads.** The game runs on the main thread, which owns the whole
`ecs::World`; the network thread only moves datagrams, so the game never
waits for clients. `LoginSystem` hashes passwords on a short-lived thread that
only receives copies and returns a hash. Every system runs on the game thread
and none takes a lock. See `docs/technical-study/network.md` §7.

Any exception that escapes is caught in `main`: message on stderr, exit 84.

---

## Systems, in order

```
frameStart()  NetReceiveSystem        network thread inbox → ReceivedPacket / IncompatibleClient
              ConnectionSystem        CONNECT_REQUEST → NetClient entity, ACCEPT / REJECT
              SessionReceiveSystem    packet → its session's channel → ClientMessage
              LoginSystem             AUTH_REQUEST → account checked, player admitted (LoggedIn)
              TimeoutSystem           silent session → Disconnecting
              InputSystem             ClientMessage (INPUT) → the player's PlayerInput
              RestartSystem           ClientMessage (RESTART) after GAME OVER → new game
              PlayerShipSystem        one ship per player still in the game, after its respawn delay
simulation    TickSystem              SimulationTick + 1, first of every step
              MatchSystem             no life and no ship left → GAME OVER; nobody left → new game
              PlayerControlSystem     PlayerInput → ship Velocity, FIRE → ship Trigger
              ProgressSystem          seconds played (while ships exist) → level, distance points
              WaveSystem              an enemy or a formation from the right, faster each level
              WaveFlightSystem        WaveFlight Velocity: left, in a wave
              SeekerSystem            Seeker Velocity: left, steering towards a ship
              CircleFlightSystem      CircleFlight Velocity: loops while drifting left
              HopSystem               Hop Velocity: jumps along the floor
              HoverSystem             Hover Velocity: comes in, stops and bobs, leaves
              AimSystem               AimsAtShips Trigger: pulled on screen, aimed at the nearest ship
              WeaponSystem            pulled Trigger + Weapon ready → projectile (any shooter)
              game::MovementSystem    Position += Velocity × step (shared with the client)
              game::ConfinementSystem ships kept inside the arena, velocity into an edge → 0
              game::ArenaExitSystem   missiles and Bydos that left → DespawnRequest (OUT_OF_BOUNDS)
              game::CollisionSystem   overlapping hitboxes → CollisionEvent
              DamageSystem            CollisionEvent + Damage → −HP, kill → DespawnRequest (KILLED), score
              LivesSystem             ship killed → one team life less, RespawnTimer or Eliminated
              DeathBurstSystem        BurstOnDeath killed → projectiles all around
              game::DespawnSystem     DespawnRequest → DespawnEvent, entity destroyed
frameEnd()    SessionCleanupSystem    Disconnecting → PLAYER_LEFT, entity destroyed
              ReplicationSystem       new NetEntity → ENTITY_SPAWN, gone → ENTITY_DESTROY,
                                      ship killed → PLAYER_DIED, state changed → GAME_STATE
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
| Ship velocity       | `game::Velocity` on the ship            | `PlayerControlSystem`, `game::ConfinementSystem` |
| Ship position       | `game::Position` on the ship            | `game::MovementSystem`, `game::ConfinementSystem` |
| Network identity    | `NetEntity {netId, kind, owner}` on the ship | `PlayerShipSystem`    |
| Simulation tick     | `SimulationTick` singleton              | `TickSystem`               |

**Inputs (§4.2, §7.1).** Each INPUT repeats the client's last masks, so a lost
datagram costs nothing. For each player, only ticks newer than the last one
applied count; ticks more than 120 ahead are ignored, so a client cannot jump
its input clock. The newest mask wins: ticks are not replayed one by one.

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
| Player ship     | `PLAYER_SHIP`    | –                   | 3     | killed (back 3 s later, full health) |
| Player missile  | `PLAYER_MISSILE` | enemies (1 damage)  | –      | it hits, or leaves the arena        |
| Enemy           | see below        | ships (crashes)     | 1–15   | killed (+points to the team score), or leaves on the left |
| Enemy missile   | `ENEMY_MISSILE`  | ships (1 damage)    | –      | it hits, or leaves the arena        |
| Force           | `FORCE`          | enemy missiles (stops them) | – | its ship is gone                  |
| Pickup          | `REPAIR_KIT`, `FORCE_CAPSULE` | taken by ships | – | a ship takes it, or it leaves on the left |

The rules (speeds, sizes, rates, points) are constants in `src/game/Ship.hpp`,
`src/game/Bydo.hpp`, `src/game/Enemies.hpp` and `src/game/Boss.hpp`, shared with the client.

**The enemies.**

| Enemy     | Kind        | Behaviours (`Behaviour.hpp`)   | Weapon                    | HP | Points | Ram    | From level |
|-----------|-------------|--------------------------------|---------------------------|---:|-------:|--------|-----------:|
| Bydo      | `BYDO`      | `WaveFlight`, `AimsAtShips`    | `BydoCannon`: 1 missile   |  1 |    100 | 1, dies | 1 |
| Pata-Pata | `PATA_PATA` | `WaveFlight` (fast, wide)      | –                         |  1 |     50 | 1, dies | 1 |
| Bug       | `BUG`       | `Seeker`                       | –                         |  1 |    150 | 1, dies | 2 |
| Cancer    | `CANCER`    | `WaveFlight` (slow), `AimsAtShips` | `CancerFan`: 3 missiles |  3 |    300 | 1, dies | 3 |
| Cruiser   | `CRUISER`   | none: straight, very slow      | `CruiserBattery`: 5 missiles | 15 | 1000 | 3, survives | 4 |
| Hopper    | `HOPPER`    | `Hop` (on the floor), `AimsAtShips` | `HopperGun`: 1 missile | 2 |  200 | 1, dies | 2 |
| Spinner   | `SPINNER`   | `CircleFlight`                 | –                         |  2 |    250 | 1, dies | 3 |
| Gunner    | `GUNNER`    | `Hover` (6 s), `AimsAtShips`   | `GunnerBlaster`: bursts of 3 | 4 | 400 | 1, dies | 4 |
| Mine      | `MINE`      | `WaveFlight` (slow), `BurstOnDeath` | 8 missiles when killed |  6 |    500 | 2, dies | 5 |

Each one is a `spawn…()` function in `Spawn.cpp` that combines the same
components; no system knows the kinds. Enemies only fire once inside the
arena (`AimSystem`).

**Endless waves.** The run is split into levels of 30 s of play
(`game/Waves.hpp`; the clock stops while no ship is in the game).
`WaveSystem` brings an *arrival* every `spawnInterval(level)` seconds (1.6 s
at level 1, × 0.88 per level, 0.5 s at least). An arrival is one enemy, or a
**formation** with probability `formationChance(level)` (10 % per level, 50 %
at most):

| Formation      | Members                                   | From level |
|----------------|-------------------------------------------|-----------:|
| Pata-Pata line | 5 Pata-Patas in single file               | 1 |
| Bydo V         | 5 Bydos in a V                            | 2 |
| Bug rush       | 4 Bugs from different heights             | 3 |
| Cancer pair    | 2 Cancers side by side                    | 4 |
| Escort         | a Cruiser and 2 Cancers                   | 5 |
| Hopper patrol  | 3 Hoppers in single file                  | 3 |
| Spinner pair   | 2 Spinners, one above the other           | 4 |
| Gunner squad   | 3 Gunners stopping in a column            | 6 |
| Minefield      | 3 Mines, one after the other              | 7 |

A single enemy is drawn among the unlocked ones with weight
`weight + growth × (level − unlock)`: Bugs, Cancers and Cruisers grow more
common as the levels go by. Both tables are in `WaveSystem.cpp`; the next
members of a formation wait in `WaveDirector::pending`. The random source is
seeded once, so the same seed gives the same waves.

**The boss** (`game/Boss.hpp`, `BossSystem`). Every 90 s of play, a boss
(`BOSS`, 160 HP) flies in from the right, stops (`Hover`, forever) and moves
up and down. No new wave arrives while it is there. It cycles through three
attacks, 4 s each: **Fan** (7 aimed missiles), **Ring** (16 missiles all
around) and **Summon** (3 Bugs come out of it); below half health its weapons
reload faster. Killing it gives 5000 points and one life (5 at most); the
next one comes 90 s later with 30 HP more. GAME_STATE's `bossProgress` tells
the clients how far it is (0–99 %, 100 while it is there): their distance bar.

**Gadgets** (`game/Gadgets.hpp`). A killed enemy may drop a pickup where it
was (`DropSystem`): a **repair kit** (8 %) or a **Force capsule** (5 %); the
boss drops both. Pickups drift left; the ship that touches one takes it
(`PickupSystem`). The repair kit gives the ship its 3 hearts back. The
capsule gives the player a **Force**, an invulnerable pod stuck to the ship
(`ForceSystem`): each press of POD (input bit 5) moves it to the next slot,
front → above → behind → below; it fires the ship's missiles away from the
ship while FIRE is held, and absorbs the enemy missiles it touches. It is
lost with the ship; a second capsule gives 500 points instead.

GAME_STATE is sent at once when the phase, level, lives or boss distance
change, but at most every 250 ms when only the score moved (distance points
change it nearly every step).

**Combat is data, not code.** Systems work on components, never on entity
types, so new content needs no new system:

| Component (`Components/Combat.hpp`) | Read by        | Means                                         |
|-------------------------------------|----------------|-----------------------------------------------|
| `Weapon` (+ its `Projectile`)       | `WeaponSystem` | What it fires, how often, from where, how many at once (`shots`, `spread`) |
| `Trigger`                           | `WeaponSystem` | Fire now, in this direction. Set by a "brain": `PlayerControlSystem` (keys), `AimSystem` (aim) |
| `Damage`                            | `DamageSystem` | Hurts what its `Hitbox::collidesWith` names; dies on hit or not |
| `ScoreValue`                        | `DamageSystem` | Team points when it is killed                 |
| `ConfinedToArena` (game)            | `game::ConfinementSystem` | Cannot leave the arena             |

The weapons themselves are constants in `Spawn.hpp` (`weapons::ShipCannon`,
`weapons::BydoCannon`, `weapons::CancerFan`, `weapons::CruiserBattery`). A new weapon, a Force, a turret or a new enemy is a
new constant or `spawn…()` function; a new behaviour is one small "brain"
system that sets a `Trigger` or a `Velocity`.

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
| Score, level or lives change   | GAME_STATE (phase, score, level, lives); a newcomer gets it at once |
| The last life is lost          | GAME_STATE (`GAME_OVER`)                          |
| A player sends RESTART         | GAME_STATE (`RUNNING`, score 0, level 1, full lives) |

**From start to game over** (`game/Run.hpp`). The team shares 3 lives; each
ship destroyed costs one (`LivesSystem`). While lives are left, the ship comes
back after 3 s; once none is left, its player is `Eliminated`. When no ship is
in the game and none is about to come back, `MatchSystem` sets GAME OVER: the
waves stop, and any player can send RESTART for a new game (`RestartSystem` →
`startNewGame()`: score, level, waves and lives reset, leftover enemies and
missiles despawned as EXPIRED). When every player has left, the next one
starts a new game too.

The team score counts kills and distance: while a ship is in the game,
`10 × level` points per second (`ProgressSystem`).

shortcut: no lobby (WAITING_FOR_PLAYERS): a game starts as soon as a player
connects, and newcomers join the game in progress.

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
| Injection into a session    | Address, token *and* session key must match: anything not sealed with the key is dropped |
| Reading, modifying or replaying packets | Every packet of a session is encrypted (protocol §3.3) |
| Reflection / amplification  | A REJECT (21 bytes) only answers a request at least as large |
| Guessable tokens            | `std::random_device`, backed by the OS secure generator     |

Tests: [`tests/server/`](../../tests/server).
