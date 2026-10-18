# Game

R-Type gameplay shared by `r-type_server` and `r-type_client`: the
components and constants (plain data), and the gameplay systems that act on
them.

| File                           | Content                                               |
|--------------------------------|-------------------------------------------------------|
| `Arena.hpp`                    | Size of the playing field, player spawn points        |
| `Components/Motion.hpp`        | `Position`, `Velocity`                                |
| `Components/Confinement.hpp`   | `ConfinedToArena`: keeps an entity inside the field   |
| `Systems/MovementSystem.hpp`   | Moves everything that has a velocity                  |
| `Systems/ConfinementSystem.hpp`| Puts confined entities back inside the field          |

The server's systems **decide** with these components (it is authoritative);
the client's systems **display** them. Keeping them here means both sides
agree on units and meaning without depending on each other.

The library is `rtype_game` (`rtype::game`). It depends on the ECS only: no
network, no rendering.

## Units

Positions are in **world units**, velocities in world units per second. The
field is `1920 × 1080` units with `(0, 0)` at the top-left and `y` going down;
the client scales it to its window. On the wire they are quantized to 1/4 unit
(`docs/protocol.md` §2.1).

`Position` is the **centre** of an entity.

---

## Movement

`MovementSystem` moves every entity that has a `Position` and a `Velocity`:

```cpp
position.x += velocity.dx * time.deltaSeconds();
position.y += velocity.dy * time.deltaSeconds();
```

- It does not know what it moves: ships, Bydos and missiles all move the same
  way. Giving an entity a `Velocity` is all it takes to make it move.
- It never changes the velocity. Who sets the velocity is the job of other
  systems: player controls for ships, behaviour patterns for enemies, the
  weapon for missiles.
- Speeds are **per second**, so the result does not depend on the frame rate
  (tested: one 1 s step and sixty 1/60 s steps give the same position).

## Confinement

Player ships must stay on screen; missiles and enemies must be able to leave
it. An entity is kept inside the field only if it has a `ConfinedToArena`:

```cpp
world.addComponent(ship, game::ConfinedToArena{.halfWidth = 16.0F, .halfHeight = 8.0F});
```

`ConfinementSystem` clamps its centre so that the whole entity stays visible:

```text
x in [halfWidth,  1920 - halfWidth]
y in [halfHeight, 1080 - halfHeight]
```

| Situation                                   | Result                                       |
|---------------------------------------------|----------------------------------------------|
| Inside the field                            | Unchanged                                    |
| Pushed past an edge by its velocity         | Put back exactly on the edge                 |
| No `ConfinedToArena` (missile, enemy)       | Free to leave the screen                     |
| Bigger than the field                       | Kept centred                                 |

Only the position is corrected: the velocity stays what the controls asked for.

---

## Running the systems

Order matters: confinement must see the position **after** movement. Add them
in this order, in the same stage of the simulation:

```cpp
world.systems().add<game::MovementSystem>();
world.systems().add<game::ConfinementSystem>();
```

On the server they belong to the World's own systems (run at the fixed
simulation rate by `ecs::GameLoop`), not to `frameStart()` / `frameEnd()`.

Tests: [`tests/game/`](../../tests/game).
