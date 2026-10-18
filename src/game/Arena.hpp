#pragma once

#include <cstdint>

/// Size of the playing field, shared by the server and the client.
namespace game::arena {

// World units, not pixels: the client scales them to its window. A 16:9 field
// fits easily in the protocol's quantized range (±8192 units).
inline constexpr float Width = 1920.0F;
inline constexpr float Height = 1080.0F;

/// Where the ship of player `playerId` (0..3) appears: on the left, one lane
/// per player so the four ships never overlap.
struct SpawnPoint {
    float x;
    float y;
};

[[nodiscard]]
constexpr SpawnPoint playerSpawn(std::uint8_t playerId) noexcept
{
    constexpr float laneHeight = Height / 5.0F; // 4 lanes, centred
    return {.x = Width / 10.0F, .y = laneHeight * static_cast<float>(playerId + 1)};
}

} // namespace game::arena
