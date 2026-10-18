#include "game/Systems/ConfinementSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/Confinement.hpp"
#include "game/Components/Motion.hpp"

#include <algorithm>

namespace game {

namespace {

/// Keeps `value` within [low, high]. If it had to be moved, stops `speed`:
/// the entity is pushing against that limit.
///
/// If the range is empty (an entity bigger than the arena), the entity is
/// kept centred instead of letting std::clamp fail.
void keepWithin(float& value, float& speed, float low, float high) noexcept
{
    const float kept = (low > high) ? (low + high) / 2.0F : std::clamp(value, low, high);
    if (kept != value) {
        value = kept;
        speed = 0.0F;
    }
}

} // namespace

void ConfinementSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    world.query<Position, ConfinedToArena>([&world](ecs::Entity entity, Position& position,
                                               const ConfinedToArena& confined) {
        // Without a Velocity, the speed to stop is a throwaway value
        Velocity noVelocity;
        auto* velocity = world.tryGetComponent<Velocity>(entity);
        Velocity& speed = (velocity != nullptr) ? *velocity : noVelocity;

        keepWithin(position.x, speed.dx, confined.halfWidth, arena::Width - confined.halfWidth);
        keepWithin(position.y, speed.dy, confined.halfHeight, arena::Height - confined.halfHeight);
    });
}

} // namespace game
