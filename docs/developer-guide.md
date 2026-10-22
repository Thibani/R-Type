# Developer guide

How to work in this codebase. For the big picture, start with the
[README's Architecture section](../README.md#architecture); each folder of
`src/` has its own README with the details.

## Rules of the codebase

- **Everything is ECS.** State is plain data in components; behaviour is in
  systems (`ecs::System::update`) that query components. No `Player` or
  `Enemy` classes, no system calling another system.
- **Systems talk through events.** Emit with `world.events().emit(MyEvent{})`,
  listen with `world.events().subscribe<MyEvent>(...)`; events are delivered
  at the end of the stage. Example: `CollisionSystem` → `CollisionEvent` →
  `DamageSystem` → `DespawnRequest` → `DespawnSystem` → `DespawnEvent` →
  `ReplicationSystem`.
- **Never change the World inside a query.** Collect what to create or destroy,
  and do it after the query, or record it with `world.commands()`.
- **Gameplay entities leave through `DespawnRequest`**, never `destroyEntity`:
  that is how the clients get the reason (killed, out of bounds, expired).
- **The server decides.** A rule that changes the game runs on the server;
  the client only displays (and, later, predicts with the same `src/game` code).
- **Errors are values.** Network input is untrusted: return `std::expected`,
  never throw on bad packets.
- **Shared rules are constants in `src/game`** (`Ship.hpp`, `Bydo.hpp`,
  `Arena.hpp`), so server and client use the same numbers.

## Where things run (server)

```text
frameStart()   once per frame    receive packets, sessions, inputs, ships
simulation     0..N × 60 Hz      gameplay: control, fire, Bydos, movement, collisions, damage
frameEnd()     once per frame    replication, sending, snapshots
```

The full order is in `src/server/ServerSystems.hpp`. Message handlers belong
in `frameStart()` (a frame may run no simulation step); gameplay belongs in
the simulation, so it runs at a fixed rate whatever the frame rate.

## How to…

### Add a gameplay rule

1. Put its numbers in `src/game/` (a constant per value, with its unit).
2. Write a system in `src/server/Systems/` (`.hpp` with a comment saying what
   it does and when it runs, `.cpp`), add both to `src/server/CMakeLists.txt`.
3. Add it to `installSimulation()` in `src/server/ServerSystems.cpp`, at the
   right place in the order, and to the order comment in `ServerSystems.hpp`.
4. Test it in `tests/server/GameplayTests.cpp`: `Game` runs the real
   simulation step by step on a plain World.

### Add a weapon or an enemy (no new system needed)

Combat is data (`src/server/Components/Combat.hpp`):

1. A **weapon** is a `Weapon` constant in `src/server/Spawn.hpp`
   (`weapons::`): its `Projectile` (kind, speed, size, layers, damage), its
   rate and its muzzle. Give it to any entity with a `Trigger`.
2. An **enemy** is a `spawn…()` function that combines components: `Hitbox`,
   `Health`, `Damage` (if it hurts on contact), `ScoreValue`, a `Weapon` and
   a `Trigger`.
3. Only a new **behaviour** needs code: a small "brain" system that sets the
   `Trigger` (when and where to shoot) or the `Velocity` (how to move), like
   `BydoAimSystem` and `BydoMovementSystem`.

`WeaponSystem` fires every pulled trigger and `DamageSystem` applies every
`Damage`, whatever the entity is.

### Add a new kind of networked entity

1. Give it a kind in `network::protocol::EntityKind` (values 4–255 are free)
   and document it in `docs/protocol.md` §5.2.
2. Add a `spawn…()` function in `src/server/Spawn.cpp` that gives it a
   `Position`, a `Velocity`, a `Hitbox` and a `NetEntity`.
3. Nothing else for the network: `ReplicationSystem` sends its ENTITY_SPAWN
   and ENTITY_DESTROY, `SnapshotSystem` its state, to every client.

### Add a packet type

1. Add the struct in `src/network/Protocol/Packets.hpp` (with `Type`, `Way`,
   `Reliable`) and to the `Payload` variant.
2. Encode and decode it in `PacketCodec.cpp`, with its validation rules.
3. Document its layout in `docs/protocol.md`; add round-trip and robustness
   tests in `tests/network/Protocol/`.
4. Server side, handle it in a `frameStart()` system that queries
   `ClientMessage`.

### Run the tests

```sh
scripts/test.sh                                   # build + all tests
./build/debug/tests/server/server_tests --gtest_filter='Gameplay.*'
```

Socket tests use the loopback interface only; the session tests run the whole
server, with its network thread, against real UDP clients.

## Conventions

- Commits: `type(scope): description` (enforced by `.githooks/commit-msg`),
  one pull request per issue, using the PR template; never push to `main`.
- Format with `scripts/format.sh` (clang-format), lint with `scripts/lint.sh`.
- Comments explain *why*, not *what*; every system's header says when it runs.
- A known shortcut is marked `shortcut: <limit>, <when to upgrade>`.
