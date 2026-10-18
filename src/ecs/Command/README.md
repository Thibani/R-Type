# Command

This folder contains the `CommandBuffer`: **deferred structural changes**.

| File                | Content                                                     |
|---------------------|-------------------------------------------------------------|
| `CommandBuffer.hpp` | `CommandBuffer` and `EntityBuilder`                         |
| `CommandBuffer.cpp` | Their non-template parts                                    |

---

## The problem

A *structural change* is anything that modifies the storages: creating or
destroying an entity, adding or removing a component.

Doing one **inside a query** is a bug. The query is walking over the packed
array of a storage, and the change moves elements around in that very array
(the swap-and-pop of [`ComponentStorage`](../Component/README.md#removal-swap-and-pop)):

```cpp
world.query<Health>([&](ecs::Entity entity, Health& health) {
    if (health.current <= 0) {
        world.destroyEntity(entity); // WRONG: modifies the array being iterated
    }
});
```

This is undefined behavior. Worse, it often *looks* like it works. In a real
test with 6 dead enemies, the output was correct, but AddressSanitizer reported
the truth:

```text
ERROR: AddressSanitizer: container-overflow
READ of size 8 ... in query<Health, ...> src/ecs/World/World.hpp
```

The query kept reading array slots that had already been removed. Their old
values were simply still in memory.

---

## The solution: record now, apply later

```cpp
world.query<Health>([&](ecs::Entity entity, Health& health) {
    if (health.current <= 0) {
        world.commands().destroyEntity(entity); // OK: recorded, applied after the query
    }
});
```

`commands()` only **records** the change. Nothing is modified until
`apply()` runs, at a safe point where no query is iterating. The same program
under AddressSanitizer: no error, same result.

---

## API

### `CommandBuffer`

| Method                        | Recorded action, run by `apply()`                               |
|-------------------------------|-----------------------------------------------------------------|
| `createEntity()`              | Creates an entity. Returns an `EntityBuilder` to give it components |
| `destroyEntity(e)`            | Destroys `e` and all its components                             |
| `addComponent(e, value)`      | Adds or replaces a component on `e`                             |
| `removeComponent<T>(e)`       | Removes the `T` of `e`                                          |
| `run(function)`               | Runs any `function(World&)`: the escape hatch for other deferred actions |
| `apply(world)`                | Runs every recorded command in recording order, then forgets them |
| `size()` / `empty()`          | Number of commands waiting                                      |
| `clear()`                     | Forgets every command without running it                        |

### `EntityBuilder`

```cpp
world.commands().createEntity()
    .with(Position{.x = 100.0F, .y = 50.0F})
    .with(Velocity{.dx = 300.0F, .dy = 0.0F})
    .with(MissileTag{})
    .then([](ecs::World& world, ecs::Entity missile) {
        world.events().emit(MissileFiredEvent{.missile = missile});
    });
```

| Method          | Behavior                                                          |
|-----------------|-------------------------------------------------------------------|
| `with(value)`   | The entity receives this component when it is created             |
| `then(step)`    | Runs `step(world, entity)` once the entity exists and has its components |

The entity does not exist yet when the builder is used: `then` is how to get
its handle.

---

## Behavior

| Situation                                                     | Behavior                                  |
|---------------------------------------------------------------|-------------------------------------------|
| Commands are recorded                                         | Nothing changes until `apply()`           |
| Several commands                                              | Applied in recording order                |
| `addComponent` / `removeComponent` on a dead or stale entity  | Skipped silently                          |
| `destroyEntity` twice, or on a stale handle                   | Harmless: the second one does nothing     |
| A command (`run`, `then`) records new commands                | They are applied in the same `apply()`    |
| `apply()` called from inside a command                        | Ignored (not re-entrant)                  |
| A command throws                                              | The exception propagates; the remaining commands are dropped |
| Move-only components (e.g. holding a `std::unique_ptr`)       | Supported (`std::move_only_function`)     |

Skipping commands on dead entities is deliberate. Two systems may both decide
to destroy the same enemy in the same frame, or a missile may hit an enemy
that a previous command already destroyed. This must not crash.

---

## When are commands applied?

The `World` owns a `CommandBuffer` (`world.commands()`), and
`world.update(time)` applies it twice:

```text
world.update(time)
 ├── 1. every system runs              -> records commands, emits events
 ├── 2. commands().apply()             -> structural changes of the systems
 ├── 3. events().dispatch()            -> handlers run, may record commands
 └── 4. commands().apply()             -> structural changes of the handlers
```

So at the end of `update()`, everything recorded during the frame has been
applied. Step 2 also guarantees that event handlers see a consistent World:
an entity destroyed by a system is already gone when they run.

---

## Rules

- **Inside a query or an event handler: always use `commands()`** for
  structural changes.
- **Outside** (setup code, tests, between two updates): the direct `World`
  methods are fine.
- Modifying the **values** of components (`health.current -= 10`) is never a
  structural change: do it directly.

---

## Implementation notes

- Each command is a `std::move_only_function<void(World&)>`. It is
  `move_only` so that commands can hold move-only components.
- The template methods take the World as `auto&` inside their lambdas: this
  lets `CommandBuffer.hpp` avoid including `World.hpp` (which includes
  `CommandBuffer.hpp`). The type only has to be complete where the command is
  created.
- `apply()` walks the commands with an index rather than a range-for: a
  command may record new commands, which can reallocate the vector. Each
  command is moved out before it runs.

Tests: [`tests/ecs/Command/`](../../../tests/ecs/Command) and
[`tests/ecs/World/WorldCommandsTests.cpp`](../../../tests/ecs/World/WorldCommandsTests.cpp).
