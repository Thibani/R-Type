#include "server/Systems/ForceSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Collision.hpp"
#include "game/Components/Motion.hpp"
#include "game/Events/CollisionEvent.hpp"
#include "game/Events/DespawnEvent.hpp"
#include "game/Gadgets.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/Combat.hpp"
#include "server/Components/Gadgets.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/Player.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace server {

using namespace network::protocol;

namespace {

/// What a Force needs from its player's ship.
struct ShipState {
    game::Position position;
    game::Velocity velocity;
    bool firing{};
};

} // namespace

ForceSystem::ForceSystem(ecs::World& world)
    : _world(world)
    , _onCollision(world.events().subscribe<game::CollisionEvent>(
          [this](const game::CollisionEvent& collision) { onCollision(collision); }))
{
}

void ForceSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    std::array<bool, MaxPlayers> podHeld{};
    world.query<NetClient, PlayerInput>(
        [&](ecs::Entity /*entity*/, NetClient& client, PlayerInput& input) {
            podHeld.at(client.playerId) = input.received && (input.mask & input::Pod) != 0;
        });

    std::array<std::optional<ShipState>, MaxPlayers> ships{};
    world.query<PlayerShip, game::Position, game::Velocity, Trigger>(
        [&](ecs::Entity /*entity*/, PlayerShip& ship, game::Position& position,
            game::Velocity& velocity, Trigger& trigger) {
            ships.at(ship.playerId)
                = ShipState{.position = position, .velocity = velocity, .firing = trigger.pulled};
        });

    world.query<Force, game::Position, game::Velocity, Trigger>(
        [&](ecs::Entity entity, Force& force, game::Position& position, game::Velocity& velocity,
            Trigger& trigger) {
            const auto& ship = ships.at(force.playerId);
            if (!ship) {
                world.events().emit(game::DespawnRequest{.entity = entity,
                    .reason = game::DespawnReason::Expired,
                    .killer = ecs::NullEntity});
                return;
            }
            const bool held = podHeld.at(force.playerId);
            if (held && !force.podHeld) {
                force.slot = game::force::next(force.slot);
            }
            force.podHeld = held;

            const auto out = game::force::outwards(force.slot);
            position = {.x = ship->position.x + (out.x * game::force::GapX),
                .y = ship->position.y + (out.y * game::force::GapY)};
            velocity = ship->velocity;
            trigger.pulled = ship->firing;
            trigger.direction = {.dx = out.x, .dy = out.y};
        });
}

void ForceSystem::onCollision(const game::CollisionEvent& collision)
{
    block(collision.first, collision.second);
    block(collision.second, collision.first);
}

void ForceSystem::block(ecs::Entity blocker, ecs::Entity missile)
{
    if (!_world.isAlive(blocker) || !_world.isAlive(missile)
        || !_world.hasComponent<BlocksMissiles>(blocker)) {
        return;
    }
    const auto* box = _world.tryGetComponent<game::Hitbox>(missile);
    if (box == nullptr || box->layer != game::layer::EnemyMissile) {
        return;
    }
    _world.events().emit(game::DespawnRequest{
        .entity = missile, .reason = game::DespawnReason::Expired, .killer = blocker});
}

} // namespace server
