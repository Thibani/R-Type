# Resource

This folder contains two ways to share data that does not belong to an entity:

| File               | Content                                                           |
|--------------------|-------------------------------------------------------------------|
| `Resources.hpp`    | `Resources`: one value per type, shared by systems (score, config, inputs...) |
| `AssetManager.hpp` | `AssetManager`: files loaded once and shared (textures, sounds, fonts, maps...) |

---

## Resources

### Why?

Components belong to **one entity**. Some data belongs to **nobody in
particular**, and there is exactly one of it per game:

| Data                               | Component? | Resource? |
|------------------------------------|:----------:|:---------:|
| A ship's position                  | yes        |           |
| An enemy's health                  | yes        |           |
| The shared team score              |            | yes       |
| The game configuration (speeds...) |            | yes       |
| The inputs received this frame     |            | yes       |
| The asset manager                  |            | yes       |

Without resources, such data ends up in a global variable (forbidden), in a
fake "score entity", or passed by hand to every system's constructor.

### Usage

```cpp
struct Score {
    std::uint32_t points{};
};

world.resources().insert(Score{});                    // setup

// In any system:
world.resources().get<Score>().points += 100;
```

Two systems can share data without knowing each other: one writes the score,
another displays it.

### API

| Method                    | Behavior                                                       |
|---------------------------|----------------------------------------------------------------|
| `insert(value)`           | Stores the value, **replacing** any existing one of that type. Returns `T&` |
| `emplace<T>(args...)`     | Same, building the `T` from `args`                             |
| `get<T>()`                | Returns `T&`. Throws `std::out_of_range` if there is none      |
| `tryGet<T>()`             | Returns `T*`, or `nullptr`                                     |
| `contains<T>()`           | `true` if a `T` is stored                                      |
| `remove<T>()`             | Destroys the `T`. `false` if there was none                    |
| `size()` / `clear()`      | Number of resources / destroy them all                         |

`get` and `tryGet` also exist in `const` versions. Any movable type works,
including move-only types.

### Guarantees

- **At most one value per type.** For two values of the same type, wrap them in
  two distinct structs (`struct PlayerOneInput`, `struct PlayerTwoInput`), or
  store a container.
- **Stable addresses.** A resource never moves while it is stored. Even
  `insert()` on an existing type replaces the value *in place*: references
  obtained earlier remain valid and see the new value. Only `remove()` and
  `clear()` invalidate them.

  This differs from components (see the
  [Component pitfalls](../Component/README.md#pitfalls)): each resource lives in
  its own heap allocation, so adding other resources never moves it.

### In the World

`world.resources()` gives access to the World's resources. `world.clear()`
**keeps** them: a new round may keep its configuration. Reset what needs to be
reset explicitly (`world.resources().insert(Score{})`).

---

## AssetManager

### Why?

A texture must be read from disk **once**, then shared by every entity that
displays it. The spec also requires the ECS to stay independent from SFML: it
cannot know what a texture is.

The `AssetManager` solves both. It caches assets by **(type, path)**, and the
code that owns a backend (the client) tells it **how** to load each type:

```cpp
// Client setup (SFML lives here, not in the ECS)
auto& assets = world.resources().emplace<ecs::AssetManager>();

assets.setLoader<sf::Texture>([](const std::string& path) {
    auto texture = std::make_unique<sf::Texture>();
    if (!texture->loadFromFile(path)) {
        return std::unique_ptr<sf::Texture>{};   // failure -> nullptr
    }
    return texture;
});

// Anywhere, e.g. in a rendering system
auto& assets = world.resources().get<ecs::AssetManager>();
std::shared_ptr<sf::Texture> ship = assets.load<sf::Texture>("assets/textures/ship.png");
```

### API

| Method                  | Behavior                                                         |
|-------------------------|------------------------------------------------------------------|
| `setLoader<T>(loader)`  | Registers how to build a `T` from a path. `loader` returns `nullptr` on failure |
| `hasLoader<T>()`        | `true` if a loader is registered for `T`                         |
| `load<T>(path)`         | Returns the cached asset, or loads it the first time. `nullptr` if the loader fails. Throws `std::logic_error` if no loader is registered |
| `get<T>(path)`          | Returns the asset if it is already loaded, `nullptr` otherwise. **Never loads** |
| `isLoaded<T>(path)`     | `true` if the asset is in the cache                              |
| `unload<T>(path)`       | Removes the asset from the cache                                 |
| `size()` / `clear()`    | Number of loaded assets / unload everything (loaders are kept)   |

### Behavior

| Situation                                   | Behavior                                                  |
|---------------------------------------------|-----------------------------------------------------------|
| `load` twice the same path                  | The file is read once; both calls return the same object  |
| The file is missing or invalid              | `load` returns `nullptr`; nothing is cached, a later `load` retries |
| No loader registered for the type           | `load` throws: forgetting to register a loader is a programming error |
| Same path, two types (`Texture`, `Sound`)   | Two independent assets                                    |
| `unload` while someone uses the asset       | Safe: the user keeps it alive (`std::shared_ptr`), the cache forgets it |

### Why `std::shared_ptr`?

Several entities, systems or screens may use the same texture. Shared
ownership guarantees that unloading an asset from the cache (or clearing the
cache between two levels) can never leave a dangling pointer in a system that
still uses it.

---

## Which one should I use?

| I want to share...                                   | Use                    |
|------------------------------------------------------|------------------------|
| Data attached to one entity                          | a component            |
| One value for the whole game (score, config, inputs) | `Resources`            |
| A file loaded from disk                              | `AssetManager`, stored as a resource |
| The fact that something happened                     | an [event](../Event/README.md) |

Tests: [`tests/ecs/Resource/`](../../../tests/ecs/Resource) and
[`tests/ecs/World/WorldResourcesTests.cpp`](../../../tests/ecs/World/WorldResourcesTests.cpp).
