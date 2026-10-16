#pragma once

/// Gameplay components shared by the server (which decides) and the client
/// (which displays). Plain data: behaviour lives in systems.
namespace game {

/// Position in world units (see `Arena`). On the wire, quantized to 1/4 unit.
struct Position {
    float x{};
    float y{};
};

/// Velocity in world units per second.
struct Velocity {
    float dx{};
    float dy{};
};

} // namespace game
