#include "game/Systems/ArenaExitSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/ArenaExit.hpp"
#include "game/Components/Motion.hpp"
#include "game/Events/DespawnEvent.hpp"

namespace game {

namespace {

/// True if `position` is more than `margin` units outside the arena.
bool isOutside(const Position& position, float margin) noexcept
{
    return position.x < -margin || position.x > arena::Width + margin || position.y < -margin
        || position.y > arena::Height + margin;
}

} // namespace

void ArenaExitSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    world.query<Position, DespawnOutsideArena>(
        [&world](ecs::Entity entity, const Position& position, const DespawnOutsideArena& exit) {
            if (isOutside(position, exit.margin)) {
                world.events().emit(DespawnRequest{.entity = entity,
                    .reason = DespawnReason::OutOfBounds,
                    .killer = ecs::NullEntity});
            }
        });
}

} // namespace game
