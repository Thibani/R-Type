#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

/// The endless mode: how the game gets harder over time, shared by the server
/// (which applies it) and the client (which can show the level).
///
/// The run is split into levels of `LevelDuration` seconds. Each level:
///  - enemies come more often (`spawnInterval`), down to `MinSpawnInterval`;
///  - formations (several enemies at once) are more likely (`formationChance`);
///  - new enemies and formations unlock (see the spawn table on the server).
///
///     level    1     2     3     4     5     6    ...  10
///     every  1.60  1.41  1.24  1.09  0.96  0.84 ...  0.50 s
///     groups  10%   20%   30%   40%   50%   50% ...   50 %
namespace game::waves {

/// Seconds of play per level.
inline constexpr float LevelDuration = 30.0F;

/// Seconds between two arrivals (single enemy or formation) at level 1.
inline constexpr float BaseSpawnInterval = 1.6F;

/// The interval is multiplied by this factor at each new level...
inline constexpr float SpawnIntervalFactor = 0.88F;

/// ...but never goes below this.
inline constexpr float MinSpawnInterval = 0.5F;

/// Chance that an arrival is a formation: `FormationChancePerLevel` per level,
/// up to `MaxFormationChance`.
inline constexpr float FormationChancePerLevel = 0.1F;
inline constexpr float MaxFormationChance = 0.5F;

/// The level after `seconds` of play: 1 for the first `LevelDuration`.
[[nodiscard]]
inline std::uint32_t levelAt(float seconds) noexcept
{
    if (seconds <= 0.0F) {
        return 1;
    }
    return 1U + static_cast<std::uint32_t>(seconds / LevelDuration);
}

/// Seconds between two arrivals at `level`.
[[nodiscard]]
inline float spawnInterval(std::uint32_t level) noexcept
{
    const auto steps = static_cast<float>(std::max(level, 1U) - 1U);
    return std::max(MinSpawnInterval, BaseSpawnInterval * std::pow(SpawnIntervalFactor, steps));
}

/// Probability (0..1) that an arrival at `level` is a formation.
[[nodiscard]]
inline float formationChance(std::uint32_t level) noexcept
{
    return std::min(MaxFormationChance, FormationChancePerLevel * static_cast<float>(level));
}

} // namespace game::waves
