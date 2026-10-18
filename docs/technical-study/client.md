# Technical study: client

This page explains **why** the client (`r-type_client`) is built the way it
is. The network layer it uses is studied in [`network.md`](network.md); the
code lives in `src/client/` (see its [README](../../src/client/README.md)).

| Decision            | Choice                                         | Main alternatives                   |
|---------------------|------------------------------------------------|-------------------------------------|
| Graphics library    | SFML 3.1                                       | raylib 6, SDL 3                     |
| Architecture        | The team's ECS, with singleton components      | Object-oriented scene (`Player`, `Enemy` classes) |
| Smoothing           | Extrapolate with the last velocity, correct on each snapshot | Raw snapshots, interpolation, prediction |
| Storage             | Nothing written to disk; read-only assets      | Settings file (INI, JSON)           |

---

## 1. Graphics library: SFML 3.1

The library only handles the window, drawing, keyboard, text and sound:
networking and game state are the same whatever the choice. So we wrote
**the same prototype three times** and measured it: a 1280×720 window showing
the 1920×1080 world, 200 stars, a ship from the subject's sprite sheet, arrow
keys, HUD text and a sound. Each was fetched with CPM and built cold in
Release with g++ 13.3 and **the team's warning flags with `-Werror`**.

| Criterion                          | **SFML 3.1.0**     | raylib 6.0        | SDL 3.4 (+ image + ttf)  |
|------------------------------------|--------------------|-------------------|--------------------------|
| Libraries to add                   | 1                  | 1                 | 3                        |
| Builds with our flags + `-Werror`  | yes                | yes               | no: casts in SDL macros  |
| Options to stay self-contained     | 1                  | 0                 | 11                       |
| Cold download + build              | 112 s              | 58 s              | 226 s                    |
| Library-specific lines             | 50                 | 41                | 74                       |
| Resource cleanup                   | automatic (RAII)   | manual `Unload*`  | manual `Destroy*`        |
| Frame rate / memory                | 194 fps / 131 MB   | 178 fps / 131 MB  | 249 fps / 144 MB         |

Performance does not separate them: all are far above 60 fps.

```c++
// SFML 3: C++ objects, released automatically
sf::Texture sheet("ship.gif");
sf::Sprite ship(sheet, sf::IntRect({66, 0}, {33, 17}));
window.draw(ship);

// raylib: C structs, global window state, manual release
Texture2D sheet = LoadTexture("ship.gif");
DrawTexturePro(sheet, src, dst, {0, 0}, 0.0F, WHITE);
UnloadTexture(sheet);
```

**Why SFML:** same style as the rest of the project (C++ with RAII, like
`rtype_ecs` and `rtype_network`), one library for sprites, text and sound,
and it passes our strict warnings with no workaround. Its cost, a slower
first build, is paid once thanks to the CPM cache.

**Rejected:** raylib (shortest code, but a C API with hidden global state and
manual cleanup); SDL 3 (three libraries, fails our warning policy, 1.8 GB of
sources).

---

## 2. Architecture: the team's ECS

| Criterion                          | ECS (`rtype_ecs`)                        | Object-oriented scene          |
|------------------------------------|------------------------------------------|--------------------------------|
| Same model as the server           | Yes: same `World`, `GameLoop`, `Position` | No: two ways of thinking       |
| Rendering / network / state apart  | Systems only share components             | Classes tend to draw and update themselves |
| New entity kind                    | A `Look` and a sprite frame              | A new class                    |
| Testable without a window          | Yes: replication is plain functions on a `World` | Harder: objects own SFML resources |

An entity sent by the server becomes an ECS entity with `NetEntity`,
`Position`, `Velocity` and `Appearance`. Systems run in a fixed order
(input → network → 60 Hz simulation → send → audio → render) and never call
each other.

State that exists **once** (the current screen, the players, the score, the
`netId → entity` table) is a **singleton component**: a component on one
dedicated entity, reached with `singleton<T>(world)`. It is shared the same
way as any other component, so systems stay decoupled. The client was written
before the event bus reached the ECS (#116); a sound to play is a short-lived
`SoundEvent` entity, which the audio system plays and destroys.

---

## 3. Smoothing between snapshots

The server sends snapshots 30 times per second; the client draws at 60 fps
or more.

| Technique                          | Smoothness           | Delay        | Complexity |
|------------------------------------|----------------------|--------------|------------|
| Draw snapshots as they are         | Jerky at 30 Hz       | None         | Lowest     |
| **Extrapolate with the velocity**  | Smooth on straight lines | None     | Low        |
| Interpolate between two snapshots  | Smooth               | One snapshot | Medium     |
| Client prediction + reconciliation | Smooth, instant own ship | None     | High       |

Snapshots carry each entity's velocity, and R-Type movement is mostly
straight. So `MotionSystem` moves every entity by `velocity × step` between
snapshots, and the next snapshot sets the real position. Interpolation and
prediction (#80, #81) belong to Part 2.

---

## 4. Algorithms and data structures

| Where                         | Technique                               | Why                                          |
|-------------------------------|-----------------------------------------|----------------------------------------------|
| `NetRegistry`                 | Hash map `netId → entity`               | O(1) lookup for every entity of every snapshot |
| Destroyed entities            | Map `netId → time`, entries kept 5 s    | A late snapshot cannot revive a destroyed entity (protocol §5.1) |
| Snapshot order                | Newest `serverTick` kept                | Older snapshots are dropped (protocol §4.3)  |
| `InputHistory`                | Last 8 input masks                      | Each INPUT repeats them: a lost packet loses no key (protocol §4.2) |
| Packets                       | `std::variant` + `std::visit`           | One `apply()` per packet type, checked at compile time |
| Starfield                     | Stars wrap from left to right           | Constant number of entities, no allocation   |
| Window scaling                | Letterboxed `sf::View`                  | Any window size, proportions kept            |

---

## 5. Storage

The client **writes nothing to disk**. Server address, port and name come
from the command line and fill the connection form. Sprites, fonts and sounds
are read once at startup from `assets/` (next to the executable); a missing
file stops the client with its path and exit code 84, instead of showing an
incomplete game.

A settings file (key bindings, volume, accessibility options) is planned for
Part 2, with these options; it will be studied then:

| Format     | Editable by hand | Dependency        |
|------------|------------------|-------------------|
| INI / TOML | Yes              | Small parser      |
| JSON       | Yes              | JSON library      |
| Binary     | No               | None              |

---

## 6. Security

Everything the client receives is untrusted.

| Threat                                   | Mitigation                                                       |
|------------------------------------------|------------------------------------------------------------------|
| Malformed packet                         | Decoded by the team's `PacketCodec` (fully validated, see `network.md` §11); errors are values, never a crash |
| Packet from another host                 | Datagrams not from the server's address are dropped              |
| Packet from an old session               | Datagrams with another session token are dropped                 |
| Packet meant for a server                | Decoded as server-to-client only: client-to-server types are rejected |
| Unknown entity kind or player id         | Kind drawn as a "?"; player ids above 3 ignored                  |
| Replayed or out-of-order state           | Older snapshots and destroyed `netId`s ignored                   |
| Vulnerable dependency                    | SFML pinned to 3.1.0 in CPM; its bundled codecs (freetype, vorbis, flac) come with it |

**Monitoring:** the CI builds with warnings as errors and runs the client
tests on every pull request; the SFML version is changed only on purpose,
after reading its release notes and security advisories.
