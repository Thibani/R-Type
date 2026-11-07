#pragma once

#include "TestPlayers.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Components/Player.hpp"
#include "server/ServerSystems.hpp"
#include "server/Spawn.hpp"
#include "server/Systems/PlayerShipSystem.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

/// The server's simulation on a plain World, step by step, without sockets:
/// what the gameplay tests (`GameplayTests`, `EnemyTests`, `WaveTests`) play on.
namespace testing_game {

/// One simulation step, as the server's GameLoop runs it.
inline constexpr float Step = 1.0F / 60.0F;

/// A World running the server's simulation, with its own clock.
class Game {
public:
    Game()
    {
        server::installSimulation(world);
    }

    void steps(int count)
    {
        for (int i = 0; i < count; ++i) {
            time = time.advanced(Step);
            world.update(time);
        }
    }

    void seconds(float duration)
    {
        steps(static_cast<int>(std::ceil(duration / Step)));
    }

    /// A connected player with a ship in its lane, as PlayerShipSystem makes it.
    ecs::Entity join(std::uint8_t playerId)
    {
        const ecs::Entity player = testing_players::addPlayer(world, playerId);
        world.addComponent(player, server::PlayerInput{});
        ships.update(world, ecs::Time{});
        return player;
    }

    void press(ecs::Entity player, std::uint8_t mask)
    {
        auto& input = world.getComponent<server::PlayerInput>(player);
        input.mask = mask;
        input.received = true;
    }

    template <typename T>
    std::vector<ecs::Entity> all()
    {
        std::vector<ecs::Entity> found;
        world.query<T>([&](ecs::Entity entity, T& /*component*/) { found.push_back(entity); });
        return found;
    }

    std::vector<ecs::Entity> ofKind(network::protocol::EntityKind kind)
    {
        std::vector<ecs::Entity> found;
        world.query<server::NetEntity>([&](ecs::Entity entity, server::NetEntity& net) {
            if (net.kind == kind) {
                found.push_back(entity);
            }
        });
        return found;
    }

    ecs::Entity shipOf(std::uint8_t playerId)
    {
        ecs::Entity found = ecs::NullEntity;
        world.query<server::PlayerShip>([&](ecs::Entity entity, server::PlayerShip& ship) {
            if (ship.playerId == playerId) {
                found = entity;
            }
        });
        return found;
    }

    ecs::World world;
    ecs::Time time;
    server::PlayerShipSystem ships;
};

/// Projectiles as the server's weapons fire them.
inline ecs::Entity playerMissile(ecs::World& world, game::Position from)
{
    return server::spawnProjectile(
        world, server::weapons::ShipCannon.projectile, 0, from, {.dx = 1.0F, .dy = 0.0F});
}

inline ecs::Entity enemyMissile(ecs::World& world, game::Position from)
{
    return server::spawnProjectile(world, server::weapons::BydoCannon.projectile,
        network::protocol::NoPlayer, from, {.dx = -1.0F, .dy = 0.0F});
}

} // namespace testing_game
