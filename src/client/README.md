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
(explosion), PLAYER_DIED and another player joining. The current server sends
only the last one, so in game only "player joined" is heard for now. Bydos and
missiles are drawn as soon as the server sends them.

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
