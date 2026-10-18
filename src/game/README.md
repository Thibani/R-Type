# Game

R-Type gameplay shared by `r-type_server` and `r-type_client`: components,
constants, and the systems both sides must run identically.

| File                          | Content                                              |
|-------------------------------|------------------------------------------------------|
| `Components/Motion.hpp`       | `Position`, `Velocity`                               |
| `Arena.hpp`                   | Size of the playing field, player spawn points       |
| `Ship.hpp`                    | Ship speed and size                                  |
| `Systems/MovementSystem.hpp`  | `Position += Velocity × delta` for every entity      |

The server's systems **decide** with these (it is authoritative); the client
**displays** them, and later **predicts** the local ship with the very same
`MovementSystem` and `ship::Speed` (#80), so both compute the same positions.
Keeping them here means both sides agree on units and rules without depending
on each other. The library knows nothing about the network.

## Units

Positions are in **world units**, velocities in world units per second. The
field is `1920 × 1080` units with `(0, 0)` at the top-left and `y` going down;
the client scales it to its window. On the wire they are quantized to 1/4 unit
(`docs/protocol.md` §2.1).

A ship moves at `ship::Speed` (480 units/s, ≈ 4 s to cross the arena) in any
direction, diagonals included, and its centre stays `ship::HalfWidth` /
`ship::HalfHeight` away from the edges so the whole ship stays visible.
