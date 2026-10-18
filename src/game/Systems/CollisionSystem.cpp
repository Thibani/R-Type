#include "game/Systems/CollisionSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Collision.hpp"
#include "game/Components/Motion.hpp"
#include "game/Events/CollisionEvent.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace game {

namespace {

/// What the collision test needs about one entity, copied out of the query.
struct Body {
    ecs::Entity entity;
    Position position;
    Hitbox hitbox;
};

/// True if at least one of the two wants to hit the other.
bool layersMatch(const Hitbox& first, const Hitbox& second) noexcept
{
    return (first.collidesWith & second.layer) != 0 || (second.collidesWith & first.layer) != 0;
}

/// True if the two boxes overlap. Boxes that only touch do not overlap.
bool boxesOverlap(const Body& first, const Body& second) noexcept
{
    const float distanceX = std::abs(first.position.x - second.position.x);
    const float distanceY = std::abs(first.position.y - second.position.y);
    return distanceX < first.hitbox.halfWidth + second.hitbox.halfWidth
        && distanceY < first.hitbox.halfHeight + second.hitbox.halfHeight;
}

} // namespace

void CollisionSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    std::vector<Body> bodies;
    world.query<Position, Hitbox>([&](ecs::Entity entity, Position& position, Hitbox& hitbox) {
        bodies.push_back(Body{.entity = entity, .position = position, .hitbox = hitbox});
    });

    // Sorted by id: the events come out in the same order on every run
    std::ranges::sort(bodies, {}, [](const Body& body) { return body.entity.id(); });

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const Body& first = bodies[i];
            const Body& second = bodies[j];
            if (layersMatch(first.hitbox, second.hitbox) && boxesOverlap(first, second)) {
                world.events().emit(CollisionEvent{.first = first.entity, .second = second.entity});
            }
        }
    }
}

} // namespace game
