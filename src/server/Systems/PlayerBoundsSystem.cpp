#include "server/Systems/PlayerBoundsSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/Motion.hpp"
#include "game/Ship.hpp"
#include "server/Components/Player.hpp"

#include <algorithm>

namespace server {

namespace {

/// Clamps `value` to [low, high]; stops `speed` if it was pushing past a limit.
void keepWithin(float& value, float& speed, float low, float high) noexcept
{
    const float clamped = std::clamp(value, low, high);
    if (clamped != value) {
        value = clamped;
        speed = 0.0F;
    }
}

} // namespace

void PlayerBoundsSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    using namespace game;

    world.query<PlayerShip, Position, Velocity>(
        [](ecs::Entity /*entity*/, PlayerShip& /*ship*/, Position& position, Velocity& velocity) {
            keepWithin(position.x, velocity.dx, ship::HalfWidth, arena::Width - ship::HalfWidth);
            keepWithin(position.y, velocity.dy, ship::HalfHeight, arena::Height - ship::HalfHeight);
        });
}

} // namespace server
