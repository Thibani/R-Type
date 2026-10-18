# R-Type

A networked R-Type game written in modern C++ (C++23), built on top of a
reusable Entity-Component-System (ECS) game engine.

> **Status:** Part 1 prototype. Up to 4 players join an authoritative server
> with the graphical client, fly their ships, shoot the Bydos that come in
> waves and shoot back, and share a team score. A client that crashes or
> leaves does not stop the game. Next (Part 2): game phases (lobby, game
> over), levels, bosses, and the advanced networking track.

---

## Requirements

| Tool         | Minimum version | Notes                                              |
|--------------|-----------------|----------------------------------------------------|
| CMake        | 3.21            |                                                    |
| C++ compiler | C++23 support   | GCC 13+, Clang 19+ or MSVC 19.36+ (VS 2022 17.6+)  |
| Ninja        | any             | Required by the CMake presets (bundled with VS)    |
| Git          | any             | Also used by CMake to fetch dependencies           |

Optional, for code quality: `clang-format` and `clang-tidy`.

On Linux, the client (SFML) also needs the X11, OpenGL and udev headers:
`sudo apt install libx11-dev libxrandr-dev libxcursor-dev libxi-dev libudev-dev libgl-dev`.

On Windows, run the commands below from the **Developer PowerShell for VS**
(it provides the compiler and Ninja) or from Git Bash for the shell scripts.

Third-party libraries (GoogleTest, standalone Asio and SFML) are downloaded automatically
at configure time with [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake).
Nothing third-party is committed to the repository.

---

## First-time setup

Enable the Git hook that checks commit messages:

```sh
chmod +x .githooks/commit-msg
git config core.hooksPath .githooks
```

Recommended: share downloaded dependencies between build directories so that
a clean build does not download them again.

```sh
export CPM_SOURCE_CACHE=~/.cache/CPM                  # Linux / Git Bash
setx CPM_SOURCE_CACHE "%USERPROFILE%\.cache\CPM"      # Windows (reopen the terminal)
```

---

## Build

### With presets (recommended)

```sh
cmake --preset debug           # configure (once, or after editing a CMakeLists.txt)
cmake --build --preset debug   # build
ctest --preset debug           # run the tests
```

Build files go to `build/<preset>/`.

| Preset    | Purpose                                               |
|-----------|-------------------------------------------------------|
| `debug`   | Everyday development: Debug build with tests          |
| `release` | Optimized build, tests disabled                       |
| `ci`      | Debug build with warnings treated as errors           |

Before pushing, make sure the `ci` preset builds and passes:

```sh
cmake --preset ci && cmake --build --preset ci && ctest --preset ci
```

### Without presets

```sh
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

`--config` / `-C` are only needed with multi-configuration generators such as
Visual Studio; they are ignored elsewhere.

### Build options

Pass them at configure time, e.g. `cmake --preset debug -DRTYPE_BUILD_TESTS=OFF`.

| Option                     | Default | Description                         |
|----------------------------|---------|-------------------------------------|
| `RTYPE_BUILD_TESTS`        | `ON`    | Build the unit and integration tests |
| `RTYPE_WARNINGS_AS_ERRORS` | `OFF`   | Treat compiler warnings as errors   |

---

## Run

### Server

```sh
./build/debug/r-type_server              # listens on UDP port 4242
./build/debug/r-type_server -p 5000      # another port
./build/debug/r-type_server --help
```

The binary is written at the root of the build folder (`build/<preset>/`).
Stop it with `Ctrl+C`: it finishes the current frame and exits cleanly.

| Exit code | Meaning                                                  |
|-----------|----------------------------------------------------------|
| `0`       | Stopped normally, or `--help`                            |
| `84`      | Invalid arguments, port unavailable, or a fatal error    |

On Windows the binary is `build\<preset>\r-type_server.exe`; allow it through the
firewall when asked, or clients on other machines cannot reach it.

### Client

```sh
./build/debug/r-type_client                          # server 127.0.0.1:4242, name "Player"
./build/debug/r-type_client -s 10.0.0.2 -p 5000 -n Thibani
./build/debug/r-type_client --help
```

The options fill the connection form; press Enter to connect. In game: arrows to
move, Space to fire, Esc for the menu. The `assets/` folder is copied next to the
binary at build time. Exit code `84` on invalid arguments or missing assets.
Details: [`src/client/README.md`](src/client/README.md).

---

## Tests

Tests use [GoogleTest](https://github.com/google/googletest) and are run
through CTest.

```sh
ctest --preset debug                     # run every test
ctest --preset debug -R EntityManager    # run tests whose name matches a regex

# Run a test executable directly for GoogleTest's detailed output
./build/debug/tests/ecs/ecs_tests --gtest_filter='EntityManager.*'
```

Test files mirror the layout of `src/`: the tests of `src/ecs/Entity/` live in
`tests/ecs/Entity/`.

To add a test file, create it next to the others and list it in the
`rtype_add_test(...)` call of the matching `tests/**/CMakeLists.txt`.

---

## Scripts

Helper scripts live in `scripts/` (run them from Git Bash on Windows).

| Script                     | What it does                                                    |
|----------------------------|-----------------------------------------------------------------|
| `scripts/format.sh`        | Formats every C++ file with `clang-format`                      |
| `scripts/format.sh --check`| Only reports badly formatted files (no changes), for CI         |
| `scripts/lint.sh [dir]`    | Runs `clang-tidy` (needs a configured build, default `build/debug`) |
| `scripts/test.sh [preset]` | Configures, builds and runs the tests (default preset `debug`)  |

---

## Architecture

```text
 r-type_client (×4)                         r-type_server (authoritative)
┌───────────────────────┐   UDP, binary   ┌──────────────────────────────────┐
│ keyboard → INPUT ─────┼────────────────►│ network thread: socket, queues   │
│                       │   protocol      │ game thread: ecs::World          │
│ render ◄── SNAPSHOT,  │◄────────────────┼── inputs → simulation (60 Hz)    │
│    ENTITY_SPAWN, ...  │  (docs/protocol)│   → snapshots (30 Hz) + events   │
└───────────────────────┘                 └──────────────────────────────────┘
```

- **The server decides everything.** Clients send which keys are held; the
  server moves ships, spawns Bydos, detects collisions, applies damage and
  keeps the score, then tells every client what happened.
- **Everything is ECS.** Game state lives in components on an `ecs::World`,
  behaviour in systems that query them; systems talk through events, never
  by calling each other. Server and client both run the same `ecs::GameLoop`
  (frame start → fixed 60 Hz simulation → frame end).
- **Decoupled layers**, one library each, each depending only on the ones
  below it:

| Layer              | Folder          | Depends on      | Role                                          |
|--------------------|-----------------|-----------------|-----------------------------------------------|
| Engine             | `src/ecs/`      | the standard library | Entities, components, systems, events, game loop |
| Networking         | `src/network/`  | Asio            | Binary protocol, UDP transport, reliability; no game knowledge |
| Game logic         | `src/game/`     | `ecs`           | Rules and systems shared by server and client (movement, collisions, despawn) |
| Server             | `src/server/`   | `ecs`, `network`, `game` | Sessions, gameplay rules, replication |
| Client             | `src/client/`   | `ecs`, `network`, `game`, SFML | Rendering, input, audio |

Developer documentation:

| Topic                               | Where                                                           |
|-------------------------------------|-----------------------------------------------------------------|
| How to work in the codebase         | [`docs/developer-guide.md`](docs/developer-guide.md)           |
| Accessibility                       | [`docs/accessibility.md`](docs/accessibility.md)               |
| Network protocol (wire format)      | [`docs/protocol.md`](docs/protocol.md)                         |
| Why these technologies              | [`docs/technical-study/network.md`](docs/technical-study/network.md) |
| ECS engine                          | [`src/ecs/`](src/ecs) (a README per concept)                   |
| Gameplay rules                      | [`src/game/README.md`](src/game/README.md), [`src/server/README.md`](src/server/README.md) |
| Server internals, threads, security | [`src/server/README.md`](src/server/README.md)                 |
| Client internals                    | [`src/client/README.md`](src/client/README.md)                 |

---

## Project structure

```text
.
├── CMakeLists.txt        # Root build script
├── CMakePresets.json     # Ready-made build configurations
├── cmake/                # Options, compiler warnings, dependencies (CPM)
├── assets/               # Client sprites, fonts and sounds
├── docs/                 # Protocol, developer guide, accessibility, technical studies
├── scripts/              # format / lint / test helpers
├── src/
│   ├── ecs/              # Generic ECS library (target: rtype_ecs)
│   │   ├── Entity/       # Entity handle and EntityManager
│   │   ├── Component/    # Component concept, storage and ComponentManager
│   │   ├── World/        # World facade and queries
│   │   ├── System/       # System base class and SystemManager
│   │   ├── Time/         # Frame timing, fixed timestep and frame clock
│   │   ├── Event/        # EventBus: systems communicate without knowing each other
│   │   ├── Command/      # CommandBuffer: deferred structural changes
│   │   ├── Scheduler/    # Stages: the order of the frame
│   │   ├── Resource/     # Resources (shared data) and AssetManager (files)
│   │   └── Loop/         # GameLoop shared by the server and the client
│   ├── network/          # Network library (target: rtype_network)
│   │   ├── Serialization/ # Bounded big-endian ByteReader / ByteWriter
│   │   ├── Protocol/     # Packet types and encode / decode (docs/protocol.md)
│   │   ├── Transport/    # UDP socket (Asio), and the server's network thread
│   │   └── Reliability/  # Sequences, acks, resends, ordering (docs/protocol.md §6)
│   ├── game/             # Gameplay shared by server and client (target: rtype_game)
│   ├── server/           # r-type_server: sessions, gameplay rules, replication, main.cpp
│   └── client/           # r-type_client: rendering, input, audio (SFML)
└── tests/
    ├── ecs/              # Mirrors src/ecs (Entity/, Component/, ...)
    ├── network/          # Mirrors src/network
    ├── game/             # Mirrors src/game
    ├── server/           # Mirrors src/server
    └── client/           # Mirrors src/client
```

Conventions:

- every `.hpp` sits in the same folder as its `.cpp`;
- each ECS concept has its own sub-folder (`Entity/`, `Component/`, ...),
  with a `README.md` explaining how it works;
- headers are included from the `src/` root: `#include "ecs/Entity/Entity.hpp"`;
- the ECS depends on nothing but the C++ standard library.

---

## Contributing

Commit messages must follow the convention enforced by the Git hook:

```text
type(scope): description
```

- **types:** `feat` `fix` `docs` `style` `refactor` `perf` `test` `build` `ci` `chore` `revert`
- **scope:** lowercase, e.g. `ecs`, `server`, `client`
- **description:** 72 characters max

Example: `feat(ecs): add component storage`

Run `scripts/format.sh` before committing, and never push directly to `main`:
open a pull request using the provided template.

---

## License

See [LICENSE](LICENSE).
