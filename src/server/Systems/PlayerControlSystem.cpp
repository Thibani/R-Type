#include "server/Systems/PlayerControlSystem.hpp"

#include "ecs/World/World.hpp"
#include "game/Components/Motion.hpp"
#include "game/Ship.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/Player.hpp"

#include <array>
#include <cmath>
#include <cstdint>

namespace server {

using namespace network::protocol;

namespace {

/// -1, 0 or +1 along one axis, from the two keys of that axis.
float axis(std::uint8_t mask, std::uint8_t negative, std::uint8_t positive) noexcept
{
    const bool towardNegative = (mask & negative) != 0;
    const bool towardPositive = (mask & positive) != 0;
    return static_cast<float>(towardPositive) - static_cast<float>(towardNegative);
}

game::Velocity velocityFor(std::uint8_t mask) noexcept
{
    const float x = axis(mask, input::Left, input::Right);
    const float y = axis(mask, input::Up, input::Down); // y grows downwards
    const float length = std::hypot(x, y);
    if (length == 0.0F) {
        return {};
    }
    return {.dx = x / length * game::ship::Speed, .dy = y / length * game::ship::Speed};
}

} // namespace

void PlayerControlSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    // Each player's keys, by playerId: inputs live on the player, ships apart
    std::array<std::uint8_t, MaxPlayers> masks{};
    world.query<NetClient, PlayerInput>(
        [&](ecs::Entity /*entity*/, NetClient& client, PlayerInput& input) {
            masks.at(client.playerId) = input.received ? input.mask : 0;
        });

    world.query<PlayerShip, game::Velocity>(
        [&](ecs::Entity /*entity*/, PlayerShip& ship, game::Velocity& velocity) {
            velocity = velocityFor(masks.at(ship.playerId));
        });
}

} // namespace server
