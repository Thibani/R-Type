# System

This folder contains the behavior layer of the ECS:

| File                | Content                                                 |
|---------------------|---------------------------------------------------------|
| `System.hpp`        | `System`, the base class of every system                |
| `SystemManager.hpp` | `SystemManager`, which owns the systems and runs them in order |
| `SystemManager.cpp` | The non-template part of `SystemManager`                |

---

## What is a system?

In an ECS:

- **entities** are identifiers ([`../Entity`](../Entity/README.md));
- **components** are data ([`../Component`](../Component/README.md));
- **systems** are behavior.

A system runs once per frame. It reads and writes components through the
`World`, usually with a [query](../World/README.md#queries):

```cpp
class MovementSystem final : public ecs::System {
public:
    void update(ecs::World& world, const ecs::Time& time) override
    {
        world.query<Position, Velocity>(
            [&](ecs::Entity, Position& position, Velocity& velocity) {
                position.x += velocity.dx * time.deltaSeconds();
                position.y += velocity.dy * time.deltaSeconds();
            });
    }
};
```

The `MovementSystem` does not know whether it moves a player, an enemy or a
missile. It only knows `Position + Velocity`.

### Rules

1. **One small responsibility per system.** Movement, collision, rendering and
   damage are four different systems.
2. **A system never calls another system.** Systems communicate through
   components, events, commands and resources. A system holding a reference to
   another system is a design error.
3. **A system works on component types**, never on "player" or "enemy" classes.
4. **Use the delta time** for anything that changes over time
   (see [`../Time`](../Time/README.md)).

---

## `System`

```cpp
class System {
public:
    virtual ~System() = default;
    virtual void update(World& world, const Time& time) = 0;
};
```

- `update` is the only method to implement.
- `world` gives access to entities, components and queries.
- `time` gives the duration of the frame and the total elapsed time.
- Systems are not copyable: they are owned by a `SystemManager` and live as
  long as it does.

A system may keep its own state (a timer, a configuration value...) as member
variables, and receive dependencies through its constructor.

---

## `SystemManager`

| Method                   | Behavior                                                       |
|--------------------------|----------------------------------------------------------------|
| `add<T>(args...)`        | Builds a `T` from `args`, appends it to the execution order, returns `T&`. Throws `std::logic_error` if a `T` was already added |
| `remove<T>()`            | Removes the `T`. `false` if there was none                     |
| `has<T>()`               | `true` if a `T` was added                                      |
| `find<T>()`              | Returns `T*`, or `nullptr` if there is none                    |
| `update(world, time)`    | Calls `update` on every system, **in insertion order**         |
| `size()`                 | Number of systems                                              |
| `clear()`                | Removes every system                                           |

- **Each system type can be added only once.** This makes `find<T>()`
  unambiguous.
- **The execution order is the insertion order.** It matters: input must be
  read before movement, and movement must happen before collision. The
  Scheduler (coming later) will add finer control (stages).
- **Do not add or remove systems from inside a system's `update`**: the list
  being iterated would change.

The `World` owns a `SystemManager`: in practice you use `world.systems()` and
`world.update(time)`.

```cpp
ecs::World world;
world.systems().add<InputSystem>();
world.systems().add<MovementSystem>();
world.systems().add<CollisionSystem>();

world.update(time); // Input, then Movement, then Collision
```

---

## Example

Output of a real program, with a `MovementSystem` followed by a system printing
the positions. Frames have different durations on purpose:

```cpp
world.systems().add<MovementSystem>();
world.systems().add<PrintSystem>();

ship:  Position{0, 0},   Velocity{100, 0}    // 100 units per second
enemy: Position{500, 0}, Velocity{-50, 0}

ecs::Time time;
for (float dt : {0.5F, 0.25F, 0.25F}) {
    time = time.advanced(dt);
    world.update(time);
}
```

```text
t=0.5s  (dt=0.5)   #0 x=50   #1 x=475
t=0.75s (dt=0.25)  #0 x=75   #1 x=462.5
t=1s    (dt=0.25)  #0 x=100  #1 x=450
```

After exactly 1 second, the ship has moved 100 units, whatever the duration of
each frame.

---

## Adding a new system

1. Create a class deriving from `ecs::System` in the layer it belongs to
   (game systems go in the game layer, never in `src/ecs/`).
2. Implement `update(World&, const Time&)`.
3. Register it: `world.systems().add<MySystem>(constructor arguments...)`.

No existing system needs to be modified.

Tests: [`tests/ecs/System/`](../../../tests/ecs/System) and
[`tests/ecs/World/WorldSystemsTests.cpp`](../../../tests/ecs/World/WorldSystemsTests.cpp).
