# Entity

This folder contains the entity layer of the ECS:

| File                | Content                                                |
|---------------------|--------------------------------------------------------|
| `Entity.hpp`        | `Entity`, the lightweight handle identifying an entity |
| `EntityManager.hpp` | `EntityManager`, which creates, destroys and validates handles |
| `EntityManager.cpp` | Implementation of `EntityManager`                      |

---

## What is an entity?

An entity is **only an identifier**. It contains no data and no behavior:

- data lives in **components** (see [`../Component`](../Component/README.md));
- behavior lives in **systems**.

A player, an enemy or a missile are all the same thing from the ECS point of
view: an `Entity` with a different set of components attached.

```cpp
// There is no Player class. A "player" is just an entity...
ecs::Entity player = entities.create();
// ...that will later receive components: Transform, Health, PlayerTag, ...
```

---

## `Entity`: id + generation

```cpp
class Entity {
    Id _id;                 // index, may be reused after destruction
    Generation _generation; // incremented every time the index is reused
};
```

An `Entity` is 8 bytes: copy it freely and pass it by value.

| Member                 | Meaning                                                     |
|------------------------|-------------------------------------------------------------|
| `id()`                 | Index of the entity. Used as an array index by the storages |
| `generation()`         | How many times this index had been recycled when the handle was created |
| `valid()`              | `false` only for the null handle. **It does not mean "alive"** |
| `operator==`           | Two handles are equal only if both id **and** generation match |
| `std::hash<Entity>`    | Allows `std::unordered_map<Entity, ...>`                    |
| `ecs::NullEntity`      | The null handle (`Entity{}`), useful as a "no entity" value |

> Notation used below: `#2g1` means *id 2, generation 1*.

### Why a generation?

Ids are recycled. Without a generation, an old handle would silently point to
whatever new entity reused its id:

```text
missile targets enemy #1
enemy #1 dies
a bonus is created and reuses id #1
missile still targets "#1"  ->  it now chases the bonus!   <- bug
```

With a generation, the old handle is `#1g0` while the new entity is `#1g1`.
They are different, so the old handle is detected as **stale**.

---

## `EntityManager`

### Internal state

```cpp
struct Slot {
    Generation generation; // current generation of this id
    bool alive;            // is the id currently used?
};

std::vector<Slot> _slots;          // _slots[id] describes entity id
std::vector<Entity::Id> _freeIds;  // destroyed ids, ready to be reused
std::size_t _aliveCount;
```

The entity id is directly the index in `_slots`, so every operation is O(1).

### API

| Method              | Behavior                                                      |
|---------------------|---------------------------------------------------------------|
| `create()`          | Reuses a free id if there is one, otherwise appends a new one. Marks it alive and returns `Entity{id, slot.generation}` |
| `destroy(entity)`   | If alive: marks the slot free, **increments its generation**, pushes the id into `_freeIds`, returns `true`. Otherwise returns `false` and changes nothing |
| `isAlive(entity)`   | `true` if the slot is alive **and** its generation equals the handle's generation |
| `aliveCount()`      | Number of alive entities                                      |
| `clear()`           | Destroys every entity (all handles become stale); ids are reused from 0 |

`isAlive()` is the only reliable way to know whether a handle still refers to
an existing entity:

```cpp
return slot.alive && slot.generation == entity.generation();
```

### All the cases of `isAlive(entity)`

| Handle                                | Result  | Why                                   |
|---------------------------------------|---------|---------------------------------------|
| Just created                          | `true`  | Slot alive, same generation           |
| Destroyed                             | `false` | Slot not alive                        |
| Destroyed, then its id was reused     | `false` | Slot alive, but generation differs    |
| The new entity that reused the id     | `true`  | Slot alive, same generation           |
| `NullEntity`                          | `false` | Not `valid()`                         |
| Id never created (e.g. `Entity{999,0}`) | `false` | Id out of `_slots` bounds           |
| Any handle after `clear()`            | `false` | Every generation was incremented      |

---

## Step-by-step example

```cpp
ecs::EntityManager entities;

ecs::Entity ship    = entities.create();
ecs::Entity enemy   = entities.create();
ecs::Entity missile = entities.create();
ecs::Entity target  = enemy;      // a copy of the handle, kept by the missile

entities.destroy(enemy);
ecs::Entity bonus = entities.create();

entities.isAlive(target);         // ?
entities.destroy(target);         // ?
```

| Step                  | `_slots` (id: gen, state)                 | `_freeIds` | Result            |
|-----------------------|-------------------------------------------|------------|-------------------|
| `create()` x3         | 0: g0 alive, 1: g0 alive, 2: g0 alive     | `[]`       | `#0g0 #1g0 #2g0`  |
| `destroy(enemy)`      | 0: g0 alive, 1: **g1 free**, 2: g0 alive  | `[1]`      | `true`            |
| `bonus = create()`    | 0: g0 alive, 1: g1 **alive**, 2: g0 alive | `[]`       | `bonus = #1g1`    |
| `isAlive(target)`     |                                           |            | `false` (g0 ≠ g1) |
| `destroy(target)`     | unchanged                                 |            | `false`: the bonus is **not** destroyed |

Note that `destroy(enemy)` cannot modify the copies of the handle that exist
elsewhere (`enemy`, `target`): they keep `#1g0` forever. This is why the
generation is stored **in the slot**: the slot is the source of truth.

---

## Design notes

- **Ids are recycled** so that every array indexed by id (`_slots`, the
  component storages) stays compact instead of growing forever.
- **Free ids are reused in LIFO order** (last destroyed, first reused). This is
  simple and fast; the order has no functional impact.
- **The manager knows nothing about components.** Removing the components of a
  destroyed entity is done by the `World`, which coordinates the
  `EntityManager` and the `ComponentManager`.
- **Limits:** up to 2³² − 1 entities alive at the same time (`create()` throws
  `std::length_error` beyond that). Generations wrap around after 2³² reuses of
  the same id, which is not a practical concern.

Tests: [`tests/ecs/Entity/`](../../../tests/ecs/Entity).
