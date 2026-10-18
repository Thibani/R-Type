#pragma once

#include "ecs/World/World.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "server/Components/NetClient.hpp"

#include <cstdint>
#include <optional>
#include <string>

/// Helpers for server tests that run systems on a plain World, without sockets.
namespace testing_players {

/// A connected player, as ConnectionSystem would create it.
inline ecs::Entity addPlayer(ecs::World& world, std::uint8_t playerId)
{
    const ecs::Entity player = world.createEntity();
    world.addComponent(player,
        server::NetClient{
            .endpoint = {},
            .token = playerId + 1U,
            .playerId = playerId,
            .name = "Player" + std::to_string(playerId),
            .channel = network::ReliableChannel(playerId + 1U),
            .lastReceived = {},
            .lastSent = {},
            .knownEntities = {},
            .knownScore = std::nullopt,
        });
    return player;
}

} // namespace testing_players
