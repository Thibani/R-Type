#pragma once

namespace game {

/// Gives an entity a limited time to live, in seconds.
///
/// `LifetimeSystem` counts it down every simulation step; when it reaches 0,
/// the entity is despawned with the reason `Expired`:
///
///     // A missile that disappears after 3 seconds, even if it hit nothing
///     world.addComponent(missile, game::Lifetime{.remainingSeconds = 3.0F});
///
/// Entities without a `Lifetime` (ships, most enemies) live until something
/// else removes them.
struct Lifetime {
    float remainingSeconds{};
};

} // namespace game
