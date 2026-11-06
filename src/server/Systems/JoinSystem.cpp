#include "server/Systems/JoinSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/Player.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <variant>
#include <vector>

namespace server {

using namespace network::protocol;

namespace {

using Places = std::array<bool, MaxPlayers>;

Places takenPlaces(ecs::World& world)
{
    Places taken{};
    world.query<InGame>(
        [&](ecs::Entity /*entity*/, InGame& player) { taken.at(player.playerId) = true; });
    return taken;
}

/// Back to the main page: the session stays open, but holds nothing of the game.
void leaveGame(ecs::World& world, ecs::Entity player)
{
    if (!world.hasComponent<InGame>(player) || world.hasComponent<Disconnecting>(player)) {
        return; // Not in the game, or SessionCleanupSystem is announcing its departure
    }
    const std::uint8_t playerId = world.getComponent<InGame>(player).playerId;
    world.removeComponent<InGame>(player);
    if (world.hasComponent<PlayerInput>(player)) {
        world.removeComponent<PlayerInput>(player);
    }
    if (world.hasComponent<RespawnTimer>(player)) {
        world.removeComponent<RespawnTimer>(player);
    }

    NetClient& client = world.getComponent<NetClient>(player);
    client.knownEntities.clear();
    client.knownScore.reset();
    std::cout << "r-type_server: player " << static_cast<int>(playerId) << " '" << client.name
              << "' went back to the main page\n";

    world.query<NetClient, InGame>([&](ecs::Entity /*entity*/, NetClient& other, InGame&) {
        other.channel.queueReliable(PlayerLeft{.playerId = playerId, .reason = LeaveReason::Quit});
    });
}

/// JOIN_RESULT comes first, then everyone learns about everyone.
void joinGame(ecs::World& world, ecs::Entity player, Places& taken)
{
    auto* client = world.tryGetComponent<NetClient>(player);
    if (client == nullptr || world.hasComponent<InGame>(player)
        || world.hasComponent<Disconnecting>(player)) {
        return;
    }

    std::uint8_t playerId = 0;
    while (playerId < MaxPlayers && taken.at(playerId)) {
        ++playerId;
    }
    if (playerId == MaxPlayers) {
        client->channel.queueReliable(JoinResult{.code = JoinCode::GameFull, .playerId = NoPlayer});
        return;
    }
    taken.at(playerId) = true;

    client->channel.queueReliable(JoinResult{.code = JoinCode::Ok, .playerId = playerId});
    const PlayerJoined newcomer{.playerId = playerId, .name = client->name};
    world.query<NetClient, InGame>([&](ecs::Entity /*entity*/, NetClient& other, InGame& inGame) {
        other.channel.queueReliable(newcomer);
        client->channel.queueReliable(
            PlayerJoined{.playerId = inGame.playerId, .name = other.name});
    });
    world.addComponent(player, InGame{.playerId = playerId});

    std::cout << "r-type_server: player " << static_cast<int>(playerId) << " '" << client->name
              << "' joined from " << client->endpoint << '\n';
}

} // namespace

void JoinSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    std::vector<ecs::Entity> messages;
    std::vector<ecs::Entity> leaving;
    std::vector<ecs::Entity> joining;
    world.query<ClientMessage>([&](ecs::Entity entity, ClientMessage& message) {
        if (std::holds_alternative<LeaveGame>(message.payload)) {
            leaving.push_back(message.client);
            messages.push_back(entity);
        } else if (std::holds_alternative<JoinGame>(message.payload)) {
            joining.push_back(message.client);
            messages.push_back(entity);
        }
    });
    for (const ecs::Entity message : messages) {
        world.destroyEntity(message);
    }

    // Read before the departures: what they free stays taken during this frame.
    // Departures first: a client can only ask to join again after leaving.
    Places taken = takenPlaces(world);
    for (const ecs::Entity player : leaving) {
        leaveGame(world, player);
    }
    for (const ecs::Entity player : joining) {
        joinGame(world, player, taken);
    }
}

} // namespace server
