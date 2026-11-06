#pragma once

#include "ecs/World/World.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "server/Components/NetClient.hpp"

#include <cstdint>
#include <optional>
#include <string>

/// Helpers for server tests that run systems on a plain World, without sockets.
namespace testing_players {

/// A player in the game, as ConnectionSystem, LoginSystem and JoinSystem would make it.
inline ecs::Entity addPlayer(ecs::World& world, std::uint8_t playerId)
{
    const ecs::Entity player = world.createEntity();
    world.addComponent(player,
        server::NetClient{
            .endpoint = {},
            .token = playerId + 1U,
            .name = "Player" + std::to_string(playerId),
            .channel = network::ReliableChannel(playerId + 1U),
            .cipher = network::crypto::SessionCipher({}, network::crypto::Role::Server),
            .clientPublicKey = {},
            .serverPublicKey = {},
            .connectedAt = {},
            .loginRequested = true,
            .lastReceived = {},
            .lastSent = {},
            .knownEntities = {},
            .knownScore = std::nullopt,
        });
    world.addComponent(player, server::LoggedIn{});
    world.addComponent(player, server::InGame{.playerId = playerId});
    return player;
}

} // namespace testing_players
