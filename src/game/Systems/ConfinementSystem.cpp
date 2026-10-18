#include "game/Systems/ConfinementSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/Confinement.hpp"
#include "game/Components/Motion.hpp"

#include <algorithm>

namespace game {

namespace {

/// Clamps `value` to [low, high]. If the range is empty (an entity bigger
/// than the arena), keeps it centred instead of letting std::clamp fail.
float clampInside(float value, float low, float high) noexcept
{
    if (low > high) {
        return (low + high) / 2.0F;
    }
    return std::clamp(value, low, high);
}

} // namespace

void ConfinementSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    world.query<Position, ConfinedToArena>([](ecs::Entity /*entity*/, Position& position,
                                               const ConfinedToArena& confined) {
        position.x = clampInside(position.x, confined.halfWidth, arena::Width - confined.halfWidth);
        position.y
            = clampInside(position.y, confined.halfHeight, arena::Height - confined.halfHeight);
    });
}

} // namespace game
