#include "server/Systems/SeekerSystem.hpp"

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "server/Components/Behaviour.hpp"
#include "server/Components/Player.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

namespace server {

namespace {

/// The ship nearest to `from`, if any.
std::optional<game::Position> nearest(
    const std::vector<game::Position>& ships, const game::Position& from)
{
    std::optional<game::Position> best;
    float bestDistance = 0.0F;
    for (const game::Position& ship : ships) {
        const float distance = std::hypot(ship.x - from.x, ship.y - from.y);
        if (!best || distance < bestDistance) {
            best = ship;
            bestDistance = distance;
        }
    }
    return best;
}

} // namespace

void SeekerSystem::update(ecs::World& world, const ecs::Time& time)
{
    std::vector<game::Position> ships;
    world.query<PlayerShip, game::Position>(
        [&](ecs::Entity /*entity*/, const PlayerShip& /*ship*/, const game::Position& position) {
            ships.push_back(position);
        });

    const float deltaSeconds = time.deltaSeconds();
    world.query<Seeker, game::Position, game::Velocity>(
        [&](ecs::Entity /*entity*/, const Seeker& seeker, const game::Position& position,
            game::Velocity& velocity) {
            velocity.dx = -seeker.speed;
            const auto target = nearest(ships, position);
            if (!target) {
                return; // Keeps its current course
            }
            // The vertical speed it would like: full speed towards the ship,
            // slower when almost aligned so it does not overshoot
            const float wanted = std::clamp(
                (target->y - position.y) * 4.0F, -seeker.maxVerticalSpeed, seeker.maxVerticalSpeed);
            const float maxChange = seeker.verticalAcceleration * deltaSeconds;
            velocity.dy += std::clamp(wanted - velocity.dy, -maxChange, maxChange);
        });
}

} // namespace server
