# R-Type

A networked R-Type game written in modern C++ (C++23), built on top of a
reusable Entity-Component-System (ECS) game engine.

> **Status:** early development. The build system, tooling and the first ECS
> bricks (entities and components) are in place. Gameplay, client and server come next.

---

## Requirements

| Tool         | Minimum version | Notes                                              |
|--------------|-----------------|----------------------------------------------------|
| CMake        | 3.21            |                                                    |
| C++ compiler | C++23 support   | GCC 13+, Clang 17+ or MSVC 19.36+ (VS 2022 17.6+)  |
| Ninja        | any             | Required by the CMake presets (bundled with VS)    |
| Git          | any             | Also used by CMake to fetch dependencies           |

Optional, for code quality: `clang-format` and `clang-tidy`.

On Windows, run the commands below from the **Developer PowerShell for VS**
(it provides the compiler and Ninja) or from Git Bash for the shell scripts.

Third-party libraries (currently only GoogleTest) are downloaded automatically
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

## Project structure

```text
.
├── CMakeLists.txt        # Root build script
├── CMakePresets.json     # Ready-made build configurations
├── cmake/                # Options, compiler warnings, dependencies (CPM)
├── scripts/              # format / lint / test helpers
├── src/
│   └── ecs/              # Generic ECS library (target: rtype_ecs)
│       ├── Entity/       # Entity handle and EntityManager
│       └── Component/    # Component concept, storage and ComponentManager
└── tests/
    └── ecs/              # Mirrors src/ecs (Entity/, Component/, ...)
```

Conventions:

- every `.hpp` sits in the same folder as its `.cpp`;
- each ECS concept has its own sub-folder (`Entity/`, `Component/`, ...);
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
