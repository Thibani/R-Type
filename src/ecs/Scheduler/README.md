# Scheduler

This folder contains the `Scheduler`: **the order of the frame**.

| File            | Content                                       |
|-----------------|-----------------------------------------------|
| `Scheduler.hpp` | `Scheduler`, the ordered list of stages       |
| `Scheduler.cpp` | Its implementation                            |

---

## Why stages?

The order of systems matters: input must be read before moving, moving must
happen before collisions, and the network must send the state once the frame
is resolved.

Without stages, that order is hidden in the order of the `add()` calls,
scattered across files. One misplaced line and collisions run before movement.

With stages, the order of the frame is **declared once**, by name:

```text
Input -> Update -> Collision -> NetworkSend -> Render
```

Each system says **which stage** it belongs to. The order in which systems are
added no longer matters.

---

## API

### `Scheduler`

| Method                            | Behavior                                                   |
|-----------------------------------|------------------------------------------------------------|
| `Scheduler()`                     | Starts with one stage: `DefaultStage` (`"Update"`)          |
| `addStage(name)`                  | Appends a stage at the end of the frame                    |
| `addStageBefore(name, anchor)`    | Inserts a stage just before `anchor`                       |
| `addStageAfter(name, anchor)`     | Inserts a stage just after `anchor`                        |
| `hasStage(name)`                  | `true` if the stage exists                                 |
| `stages()`                        | The stages, in execution order                             |

Errors (all `std::invalid_argument`): empty name, name already used, unknown
anchor.

The ECS defines only `"Update"`: it knows nothing about input, network or
rendering. Each executable (client, server, tests) builds the pipeline it
needs around it.

### `SystemManager`: systems in stages

| Method                             | Behavior                                                  |
|------------------------------------|-----------------------------------------------------------|
| `scheduler()`                      | The `Scheduler` of these systems                          |
| `add<T>(args...)`                  | Adds `T` to the default stage (`"Update"`)                |
| `addToStage<T>(stage, args...)`    | Adds `T` to `stage`. Throws `std::invalid_argument` if the stage does not exist |
| `stageOf<T>()`                     | The stage of `T`, or nothing if it was not added           |
| `update(world, time)`              | Runs every stage in order                                 |
| `updateStage(world, time, stage)`  | Runs the systems of one stage only                        |

---

## Execution order

1. Stages run in the scheduler's order.
2. Inside a stage, systems run in the order they were added.

```cpp
auto& scheduler = world.systems().scheduler();
scheduler.addStageBefore("Input", ecs::Scheduler::DefaultStage);
scheduler.addStage("Collision");
scheduler.addStage("NetworkSend");

// Added in any order:
world.systems().addToStage<NetworkSendSystem>("NetworkSend");
world.systems().addToStage<CollisionSystem>("Collision");
world.systems().add<MovementSystem>();                 // "Update"
world.systems().addToStage<InputSystem>("Input");

// Runs: Input -> Movement -> Collision -> NetworkSend
```

---

## Synchronization between stages

`World::update()` adds one more guarantee: **after every stage**, the commands
are applied and the events are delivered.

```text
world.update(time)
 for each stage:
 ├── 1. the systems of the stage run
 ├── 2. commands().apply()        <- structural changes of the stage
 ├── 3. events().dispatch()       <- events of the stage
 └── 4. commands().apply()        <- changes made by event handlers
```

So **each stage sees everything the previous stages did**. Output of a real
program where an enemy dies in the `Collision` stage:

```text
Stages: Input Update Collision NetworkSend
Frame 1
  [Input]       read the keys
  [Update]      movement
  [Collision]   #0 dies -> commands + DeathEvent
  (sync)        DeathEvent delivered: ENTITY_DESTROY #0 prepared
  [NetworkSend] snapshot with 1 entity        <- the dead enemy is already gone
```

Without this synchronization, `NetworkSend` would still see the dead enemy and
send it in the snapshot of the same frame.

---

## Choosing stages

A stage is a **synchronization point**, not a label for every system. Create
one when a group of systems needs to see the *finished* result of the previous
group. Systems that only need an order between them can share a stage: their
insertion order is kept.

A typical pipeline for this project:

| Stage            | Typical systems                                   | Owner       |
|------------------|---------------------------------------------------|-------------|
| `Input`          | read local input (client) / apply received inputs | client / server |
| `NetworkReceive` | apply snapshots and events from the network       | client / server |
| `Update`         | gameplay: player control, weapons, enemies, movement | game     |
| `Collision`      | collision detection, damage                       | game        |
| `NetworkSend`    | snapshots, reliable events                        | server      |
| `Render`         | drawing, audio                                    | client      |

The server simply has no `Render` stage, and the client no `NetworkSend` for
snapshots: each executable declares only what it runs.

---

## Rules

- Configure the stages **before** the game starts. Adding a stage while a
  frame runs is safe (the frame uses the list it started with), but it makes
  the order hard to follow.
- Never rely on the order of two systems of **different** stages being added:
  only the stage order counts.

## Future evolution

Explicit dependencies between systems ("A after B") and running independent
systems of a stage in parallel can be added on top of stages without changing
the systems themselves.

Tests: [`tests/ecs/Scheduler/`](../../../tests/ecs/Scheduler),
[`tests/ecs/System/SystemStagesTests.cpp`](../../../tests/ecs/System/SystemStagesTests.cpp) and
[`tests/ecs/World/WorldStagesTests.cpp`](../../../tests/ecs/World/WorldStagesTests.cpp).
