# Game

R-Type gameplay shared by `r-type_server` and `r-type_client`: components,
constants, and the systems both sides must run identically (the server to
decide, the client to predict).

| File                            | Content                                              |
|---------------------------------|------------------------------------------------------|
| `Arena.hpp`                     | Size of the playing field, player spawn points       |
| `Ship.hpp`                      | Ship speed, size and health (3 hearts)               |
| `Components/Motion.hpp`         | `Position`, `Velocity`                               |
| `Components/Confinement.hpp`    | `ConfinedToArena`: keeps an entity inside the field  |
| `Components/Collision.hpp`      | `Hitbox` and collision layers                        |
| `Events/CollisionEvent.hpp`     | `CollisionEvent`: two hitboxes overlap               |
| `Systems/MovementSystem.hpp`    | `Position += Velocity × delta` for every entity      |
| `Systems/ConfinementSystem.hpp` | Puts confined entities back inside the field         |
| `Systems/CollisionSystem.hpp`   | Detects overlapping hitboxes, emits `CollisionEvent` |

The server's systems **decide** with these components (it is authoritative);
the client's systems **display** them. Keeping them here means both sides
agree on units and meaning without depending on each other.

The library is `rtype_game` (`rtype::game`). It depends on the ECS only: it
knows nothing about the network or rendering. The server and the client
translate between packets and game state.

## Units

Positions are in **world units**, velocities in world units per second. The
field is `1920 × 1080` units with `(0, 0)` at the top-left and `y` going down;
the client scales it to its window. On the wire they are quantized to 1/4 unit
(`docs/protocol.md` §2.1).

`Position` is the **centre** of an entity.

## Ships

A ship moves at `ship::Speed` (480 units/s, ≈ 4 s to cross the arena) in any
direction, diagonals included, and its centre stays `ship::HalfWidth` /
`ship::HalfHeight` away from the edges so the whole ship stays visible.

A ship starts with `ship::MaxHealth` = **3 hit points**, the 3 hearts shown to
the player.

---

## Movement

`MovementSystem` moves every entity that has a `Position` and a `Velocity`:

```cpp
position.x += velocity.dx * time.deltaSeconds();
position.y += velocity.dy * time.deltaSeconds();
```

- It does not know what it moves: ships, Bydos and missiles all move the same
  way. Giving an entity a `Velocity` is all it takes to make it move.
- It never changes the velocity. Setting it is the job of other systems:
  player controls for ships, behaviour patterns for enemies, the weapon for
  missiles.
- It runs in the fixed-step simulation, and speeds are **per second**, so the
  result does not depend on the frame rate (tested: one 1 s step and sixty
  1/60 s steps give the same position).

## Confinement

Player ships must stay on screen; missiles and enemies must be able to leave
it. An entity is kept inside the field only if it has a `ConfinedToArena`:

```cpp
world.addComponent(ship, game::ConfinedToArena{
    .halfWidth = game::ship::HalfWidth,
    .halfHeight = game::ship::HalfHeight,
});
```

`ConfinementSystem` clamps its centre so that the whole entity stays visible:

```text
x in [halfWidth,  1920 - halfWidth]
y in [halfHeight, 1080 - halfHeight]
```

| Situation                                   | Result                                               |
|---------------------------------------------|------------------------------------------------------|
| Inside the field                            | Unchanged (position and velocity)                    |
| Pushed past an edge                         | Put back exactly on the edge; velocity on that axis set to 0 |
| Pushed into a wall while sliding along it   | Blocked axis stopped, the other axis keeps its speed |
| No `Velocity`                               | Only the position is corrected                       |
| No `ConfinedToArena` (missile, enemy)       | Free to leave the screen                             |
| Bigger than the field                       | Kept centred                                         |

Why stop the velocity: clients extrapolate positions from velocities between
two snapshots. A ship stuck against the right edge with a velocity still
pointing right would be drawn beyond the edge until the next snapshot.

---

## Collisions

An entity can collide only if it has a `Hitbox`: a box centred on its
`Position`, with a **layer** (what it is) and a **mask** (what it can hit).

| Layer           | Typical `collidesWith`                |
|-----------------|---------------------------------------|
| `PlayerShip`    | `Enemy`, `EnemyMissile`               |
| `PlayerMissile` | `Enemy`                               |
| `Enemy`         | `PlayerShip`, `PlayerMissile`         |
| `EnemyMissile`  | `PlayerShip`                          |

So a player missile flies through the other ships (no friendly fire), and two
missiles never hit each other. Two entities collide when their boxes overlap
**and** at least one of them lists the other's layer.

```cpp
world.addComponent(missile, game::Hitbox{
    .halfWidth = 8.0F, .halfHeight = 2.0F,
    .layer = game::layer::PlayerMissile,
    .collidesWith = game::layer::Enemy,
});
```

`CollisionSystem` checks every pair of entities with a `Position` and a
`Hitbox`, and emits a `CollisionEvent{first, second}` on the World's
[EventBus](../ecs/Event/README.md) for each pair that collides.

| Situation                                      | Result                                  |
|------------------------------------------------|-----------------------------------------|
| Boxes overlap on both axes, layers match       | One `CollisionEvent` for the pair       |
| Boxes only touch (shared edge)                 | No collision                            |
| Layers do not match (missile through a ship)   | No collision                            |
| One entity overlaps several others             | One event per pair                      |
| The overlap lasts several steps                | One event per step, while it lasts      |
| No `Hitbox`                                    | Never collides                          |

`first` is always the entity with the smaller id: the same inputs give the
same events in the same order on every run.

**The system only detects.** It never applies damage nor destroys anything:
the damage, death and score systems listen to `CollisionEvent` and decide what
a collision means. Detection and consequences stay independent.

---

## Running the systems

Order matters: confinement must see the position **after** movement, and
collisions the final position of the step. Add them in this order, in the same
stage of the simulation:

```cpp
world.systems().add<game::MovementSystem>();
world.systems().add<game::ConfinementSystem>();
world.systems().add<game::CollisionSystem>();
```

The collision events are delivered at the end of the stage (see
[Scheduler](../ecs/Scheduler/README.md)), once every entity has moved.

On the server they belong to the World's own systems (run at the fixed
simulation rate by `ecs::GameLoop`), not to `frameStart()` / `frameEnd()`.

Tests: [`tests/game/`](../../tests/game).
