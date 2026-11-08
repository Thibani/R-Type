# Client

`r-type_client`: shows the game and sends the player's keys to the server.
It decides nothing: positions, spawns, deaths and the score come from the
server (`docs/protocol.md`).

```sh
./r-type_client [-s address] [-p port] [-n name]   # defaults: 127.0.0.1 4242 Player
```

The options fill the connection form; type a password, then Enter logs in, or
use Create account. Graphics, input and
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
| `Settings.hpp`                     | Settings file (read, write, where it lives), key names, `bindKey()` |
| `Gamepad.hpp`                      | Gamepads: stick, D-pad and bound buttons → INPUT mask; menu buttons |
| `ClientSystems.hpp`                | `installSystems()`: every system, in order                 |
| `Network/ServerConnection.hpp`     | Session with the server: connect, key exchange, encryption, retries, heartbeat, timeout |
| `Replication.hpp`                  | Server payloads → entities and `Session` (no window, testable) |
| `Assets.hpp`                       | Textures, fonts, sounds; sprite frame of each `Look`       |
| `Singleton.hpp`                    | `singleton<T>(world)`: state shared by several systems     |
| `Components/NetEntity.hpp`         | `NetEntity` (netId, kind, owner, health), `NetRegistry`    |
| `Components/Appearance.hpp`        | `Appearance` (what to draw), `Star`                        |
| `Components/Session.hpp`           | Current screen, form, players, score, notices              |
| `Components/Controls.hpp`          | `PlayerControls` (keys held), `InputHistory` (last 8 ticks)|
| `Components/SoundEvent.hpp`        | A sound to play this frame                                 |
| `Components/Settings.hpp`          | `Settings` (volume, colour mode, interface size, key bindings), `Action`                |
| `Ui/`                              | Dark theme, screen layout, drawing helpers, letterboxing, sprite animation |

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
              SettingsSystem      Settings changed → saved to the settings file
              AudioSystem         SoundEvent → sound, at the settings' volume
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
| Game          | After AUTH_RESULT (OK): a single player can play at once. HUD: team score and level top left; the team's lives (small ships) and each player's hearts top right; centred at the top, the distance to the next boss, then the boss's health |
| Waiting room  | Only if the server sends GAME_STATE = WAITING_FOR_PLAYERS         |
| Game over     | GAME_STATE = GAME_OVER: team score, level reached, crew; **Play again** (RESTART ×3) or Back to menu |

Every screen has the moving sky of `StarfieldSystem` behind it. The screens without
gameplay also have slow gold streaks: never in the game, where they could look like shots.

Esc opens the in-game menu (the game keeps running) or leaves the other
screens. Players are told apart by colour, pattern and P1-P4 tag. Each colour mode has
its own copy of the sprites, repainted at startup (`Assets.cpp`, `theme::Palettes`).

**Settings** open over the connection form (Settings button) or over the game
(Esc menu → Settings; the game keeps running). The actions are the 4
moves, Fire and **Move the Force** (Left Shift, or pad button 1: the Force
goes to its next slot around the ship). The arrows (or a pad's stick
and D-pad) move to the nearest row that way, across both columns. Enter (or A)
steps the volume, colour mode or interface size to its next value, wrapping
around, or waits for a new key or button: press it, or Esc to cancel. A key
already used by another action is swapped with it. Changes apply at once and
are saved.

**Interface size** keeps every position on the 1280 × 720 canvas: the view
shows more or less of it (`layout::visibleArea()`), and clicks use the same view.
The HUD stays in the corners and wraps its player cards. Above 100 %, the connection
form, waiting room and game over use a compact form. 150 % is the maximum because
it is the largest size at which every screen still fits.

---

## Settings file

Plain `key = value` text, saved whenever a setting changes:

```ini
# R-Type client settings
volume = 80
colour = Off
interface = 100
key.up = Up
key.down = Down
key.left = Left
key.right = Right
key.fire = Space
key.pod = LShift
button.up = none
button.down = none
button.left = none
button.right = none
button.fire = 0
button.pod = 1
```

| System  | Location                                                        |
|---------|-----------------------------------------------------------------|
| Linux   | `$XDG_CONFIG_HOME/r-type/settings.cfg`, else `~/.config/r-type/settings.cfg` |
| Windows | `%APPDATA%\R-Type\settings.cfg`                                 |

- Reading never fails: a missing file gives the defaults, unknown keys are
  ignored, a bad value keeps its default, two actions on one key reset the
  controls.
- Writing goes to `settings.cfg.tmp`, then a rename: a crash never leaves a
  half-written file. If it fails, the settings screen says why.
- Key names are the ones of `keyName()` (`A`–`Z`, `Num0`–`Num9`,
  `Numpad0`–`Numpad9`, arrows, `Space`, `Enter`, `LShift`, ...). Escape
  cannot be bound: it is the menu key.

---

## Protocol rules applied by the client

- CONNECT_REQUEST every 500 ms, 10 times at most (§8.1).
- The password is typed hidden, sent only sealed in AUTH_REQUEST (§8.1), and
  forgotten once logged in, on the form and in `ServerConnection`.
- HEARTBEAT after 250 ms without sending; session lost after 5 s of silence (§8.2).
- INPUT once per 60 Hz tick, repeating the last 8 masks (§4.2).
- Datagrams from another address or with another session token are ignored (§7).
- Older snapshots are ignored; a destroyed netId is ignored for 5 s (§4.3, §5.1).
- Unknown entity kinds are drawn as a "?" placeholder (§5.2).
- The PLAYER_JOINED sent right after AUTH_RESULT for the players already
  there fill the HUD without a notice; later ones are announced (§8.1).
- Leaving or quitting sends DISCONNECT 3 times (§8.3).

---

## How to…

### Draw a new kind of entity

1. Add a value to `Look` and a case to `lookOf()` in `Components/Appearance.hpp`.
2. Give it a frame in `spriteFrame()` (`Assets.cpp`): texture, rectangle in
   the sheet, scale. A new sheet goes in `assets/sprites/`, in `Skin` and in
   `SharedSheets` (`Assets.cpp`) so that every colour mode loads it.
   Ships are the exception: `widgets::ship` draws `assets/ship/hull.png`, then
   `colour.png` tinted with the player's colour of the current mode.
3. Until then, the kind is drawn as a "?": nothing else to change, the
   network code already creates the entity.

To animate it, pick the frame from the time `spriteFrame()` receives, as the
boss does (`bossFrame()`: 32 frames of its sheet played back and forth with
`pingPongFrame()` of `Ui/Animation.hpp`, 10 per second) or the Force
(`forceFrame()`: 12 frames turning round with `loopFrame()`). Player
missiles are turned the way they fly, since the Force fires up, down and
backwards (`RenderSystem`). The repair kit has no sheet: `repairKitImage()`
draws it at startup.

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

### Add a setting

1. Add the field, with its default, to `Settings` (`Components/Settings.hpp`).
2. Read and write it in `parseSettings()` / `formatSettings()`
   (`Settings.cpp`), as a new `key = value` line; a bad value keeps the
   default.
3. Add a row: a constant in `Ui/Layout.hpp` (before `ResetRow`), its drawing
   in `drawSettings()` and its keys in `activateSettings()` / `onKey()`.
4. Set `settings.dirty = true` when it changes: `SettingsSystem` saves it.
5. Use it where it matters (like `AudioSystem` reads `volume`), and test the
   file round trip in `tests/client/SettingsTests.cpp`.

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
