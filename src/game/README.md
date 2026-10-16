# Game

R-Type gameplay data shared by `r-type_server` and `r-type_client`:
components and constants, no behaviour.

| File                    | Content                                               |
|-------------------------|-------------------------------------------------------|
| `Components/Motion.hpp` | `Position`, `Velocity`                                |
| `Arena.hpp`             | Size of the playing field, player spawn points        |

The server's systems **decide** with these components (it is authoritative);
the client's systems **display** them. Keeping them here means both sides
agree on units and meaning without depending on each other.

## Units

Positions are in **world units**, velocities in world units per second. The
field is `1920 × 1080` units with `(0, 0)` at the top-left and `y` going down;
the client scales it to its window. On the wire they are quantized to 1/4 unit
(`docs/protocol.md` §2.1).
