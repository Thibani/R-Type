# Client

`r-type_client`: shows the game and sends the player's keys to the server.
It decides nothing: positions, spawns, deaths and the score come from the
server (`docs/protocol.md`).

```sh
./r-type_client [-s address] [-p port] [-n name]   # defaults: 127.0.0.1 4242 Player
```

The options fill the connection form; Enter connects. Graphics, input and
audio use **SFML 3.1** (fetched by CPM). Sprites, fonts and sounds are in
`assets/`, copied next to the executable at build time.

Sound effects only play on server events: a missile spawn (shot), a kill
(explosion), PLAYER_DIED and another player joining. Each one is also visible
on screen, so no information is only audible.

Technology choices: [`docs/technical-study/client.md`](../../docs/technical-study/client.md).

| File                               | Content                                                    |
|------------------------------------|------------------------------------------------------------|
| `main.cpp`                         | Entry point: options, assets, socket, window, World, loop  |
| `ClientOptions.hpp`                | Command line parsing (`-s`, `-p`, `-n`, `-h`)               |
| `ClientSystems.hpp`                | `installSystems()`: every system, in order                 |
| `Network/ServerConnection.hpp`     | Session with the server: connect, retries, heartbeat, timeout |
| `Replication.hpp`                  | Server payloads → entities and `Session` (no window, testable) |
| `Assets.hpp`                       | Textures, fonts, sounds; sprite frame of each `Look`       |
| `Singleton.hpp`                    | `singleton<T>(world)`: state shared by several systems     |
| `Components/NetEntity.hpp`         | `NetEntity` (netId, kind, owner, health), `NetRegistry`    |
| `Components/Appearance.hpp`        | `Appearance` (what to draw), `Star`                        |
| `Components/Session.hpp`           | Current screen, form, players, score, notices              |
| `Components/Controls.hpp`          | `PlayerControls` (keys held), `InputHistory` (last 8 ticks)|
| `Components/SoundEvent.hpp`        | A sound to play this frame                                 |
| `Ui/`                              | Hangar theme, screen layout, drawing helpers, letterboxing |

Everything except `main.cpp` is the `rtype_client` library, so the tests can
use it directly (`tests/client/`, no window needed).

---

## Systems, in order

```
frameStart()  InputSystem         keyboard/mouse → PlayerControls, Session (form, menu)
              NetReceiveSystem    socket → ServerConnection → Replication → World
simulation    MotionSystem        Position += Velocity × step  (between snapshots, stars)
(60 Hz)       StarfieldSystem     stars leaving on the left come back on the right
              InputTickSystem     keys of this tick → InputHistory
frameEnd()    NetSendSystem       connect / leave requests, INPUT, retries, heartbeat
              AudioSystem         SoundEvent → sound
              RenderSystem        arena: starfield, then Position + Appearance
              InterfaceSystem     current screen / HUD, then display
```

Rendering, networking and game state stay apart: systems never call each
other, they share components. `RenderSystem` only reads `Position` and
`Appearance`; the network code never includes SFML.

---

## Screens

| Screen        | When                                                              |
|---------------|-------------------------------------------------------------------|
| Connect       | At start, and after any failure (with its reason)                 |
| Game          | After CONNECT_ACCEPT: a single player can play at once            |
| Waiting room  | Only if the server sends GAME_STATE = WAITING_FOR_PLAYERS         |
| Game over     | GAME_STATE = GAME_OVER: team score and crew                       |

Esc opens the in-game menu (the game keeps running) or leaves the other
screens. Players are told apart by colour, pattern and P1-P4 tag.

---

## Protocol rules applied by the client

- CONNECT_REQUEST every 500 ms, 10 times at most (§8.1).
- HEARTBEAT after 250 ms without sending; session lost after 5 s of silence (§8.2).
- INPUT once per 60 Hz tick, repeating the last 8 masks (§4.2).
- Datagrams from another address or with another session token are ignored (§7).
- Older snapshots are ignored; a destroyed netId is ignored for 5 s (§4.3, §5.1).
- Unknown entity kinds are drawn as a "?" placeholder (§5.2).
- The PLAYER_JOINED sent right after CONNECT_ACCEPT for the players already
  there fill the HUD without a notice; later ones are announced (§8.1).
- Leaving or quitting sends DISCONNECT 3 times (§8.3).

---

## How to…

### Draw a new kind of entity

1. Add a value to `Look` and a case to `lookOf()` in `Components/Appearance.hpp`.
2. Give it a frame in `spriteFrame()` (`Assets.cpp`): texture, rectangle in
   the sheet, scale. A new sheet goes in `assets/sprites/` and in `Assets`.
3. Until then, the kind is drawn as a "?": nothing else to change, the
   network code already creates the entity.

### Add a sound

1. Put the WAV file in `assets/sounds/`, add a value to `Sound`
   (`Components/SoundEvent.hpp`) and the file name to `Assets::load()`, in the
   same order.
2. Where the event is applied (`Replication.cpp`), call
   `playSound(world, Sound::…)`. `AudioSystem` plays it.

### Add a screen

1. Add a value to `Screen` (`Components/Session.hpp`) and set
   `session.screen` where it starts (usually in `Replication.cpp`).
2. Draw it: a `draw…()` function and a `case` in `InterfaceSystem.cpp`.
3. Handle its keys and clicks: a `case` in `InputSystem.cpp`.

### Add a system

1. Write a class deriving from `ecs::System` in `Systems/`: it reads and
   writes components, never calls another system.
2. Add it to `src/client/CMakeLists.txt` and to `installSystems()`
   (`ClientSystems.cpp`): `frameStart` for once-per-frame input,
   the simulation for 60 Hz updates, `frameEnd` for output.
3. State shared with other systems goes in a component, or in a singleton
   (`singleton<T>(world)`) when there is only one.

### Test

```sh
./build/debug/tests/client/client_tests   # no window or server needed
```

`Replication` and `ServerConnection` are tested by feeding them packets
directly (`tests/client/`).
