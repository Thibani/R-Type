#include "server/Systems/ConnectionSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/ReceivedPacket.hpp"
#include "server/Network.hpp"

#include <array>
#include <iostream>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace server {

using namespace network::protocol;

namespace {

/// Answer to an address without a session: no channel, so no sequence or ack.
void reject(network::NetworkThread& socket, const network::Endpoint& to, RejectReason reason)
{
    const Packet packet{
        .sessionToken = 0,
        .sequence = 0,
        .ack = std::nullopt,
        .reliableId = 0,
        .payload = ConnectReject{.reason = reason},
    };
    sendPacket(socket, packet, to);
}

void accept(network::NetworkThread& socket, NetClient& client, TimePoint now)
{
    const ConnectAccept payload{
        .playerId = client.playerId,
        .tickRate = ConnectionSystem::TickRate,
        .snapshotRate = ConnectionSystem::SnapshotRate,
        .maxPlayers = MaxPlayers,
    };
    sendPacket(socket, client.channel.wrap(payload, now), client.endpoint);
    client.lastSent = now;
}

/// The lowest player id nobody uses, or nullopt when the game is full.
std::optional<std::uint8_t> freePlayerId(ecs::World& world)
{
    std::array<bool, MaxPlayers> used{};
    world.query<NetClient>(
        [&](ecs::Entity /*entity*/, NetClient& client) { used.at(client.playerId) = true; });
    for (std::uint8_t id = 0; id < MaxPlayers; ++id) {
        if (!used.at(id)) {
            return id;
        }
    }
    return std::nullopt;
}

/// Lets the session's channel see the request, only so that the next packets
/// acknowledge its sequence. Its payload was already handled here.
void trackRequest(network::ReliableChannel& channel, const Packet& request, TimePoint now)
{
    // A CONNECT_REQUEST is unreliable: the reorder buffer cannot overflow
    [[maybe_unused]] const auto delivered = channel.receive(request, now);
}

NetClient* findClient(ecs::World& world, const network::Endpoint& endpoint)
{
    NetClient* found = nullptr;
    world.query<NetClient>([&](ecs::Entity /*entity*/, NetClient& client) {
        if (client.endpoint == endpoint) {
            found = &client;
        }
    });
    return found;
}

} // namespace

ConnectionSystem::ConnectionSystem(network::NetworkThread& socket)
    : _socket(socket)
{
}

void ConnectionSystem::update(ecs::World& world, const ecs::Time& time)
{
    const TimePoint now = toTimePoint(time);

    // Collected first: entities are created and destroyed below
    std::vector<ecs::Entity> incompatible;
    world.query<IncompatibleClient>([&](ecs::Entity entity, IncompatibleClient& /*client*/) {
        incompatible.push_back(entity);
    });
    for (const ecs::Entity entity : incompatible) {
        reject(_socket, world.getComponent<IncompatibleClient>(entity).from,
            RejectReason::VersionMismatch);
        world.destroyEntity(entity);
    }

    std::vector<ReceivedPacket> requests;
    std::vector<ecs::Entity> consumed;
    world.query<ReceivedPacket>([&](ecs::Entity entity, ReceivedPacket& received) {
        if (std::holds_alternative<ConnectRequest>(received.packet.payload)) {
            requests.push_back(std::move(received));
            consumed.push_back(entity);
        }
    });
    for (const ecs::Entity entity : consumed) {
        world.destroyEntity(entity);
    }

    for (ReceivedPacket& request : requests) {
        if (request.packet.sessionToken != 0) {
            continue; // §4.1: MUST be 0
        }
        if (NetClient* existing = findClient(world, request.from)) {
            trackRequest(existing->channel, request.packet, now);
            existing->lastReceived = now;
            accept(_socket, *existing, now); // The previous ACCEPT was lost
            continue;
        }

        std::string name = std::get<ConnectRequest>(request.packet.payload).name;
        if (name.empty()) {
            reject(_socket, request.from, RejectReason::InvalidName);
            continue;
        }
        const auto playerId = freePlayerId(world);
        if (!playerId) {
            reject(_socket, request.from, RejectReason::ServerFull);
            continue;
        }

        // Everyone already here learns about the newcomer, and the other way round
        const PlayerJoined newcomerJoined{.playerId = *playerId, .name = name};
        std::vector<PlayerJoined> alreadyHere;
        world.query<NetClient>([&](ecs::Entity /*entity*/, NetClient& other) {
            other.channel.queueReliable(newcomerJoined);
            alreadyHere.push_back(PlayerJoined{.playerId = other.playerId, .name = other.name});
        });

        const std::uint32_t token = newToken(world);
        const ecs::Entity entity = world.createEntity();
        NetClient& client = world.addComponent(entity,
            NetClient{
                .endpoint = request.from,
                .token = token,
                .playerId = *playerId,
                .name = std::move(name),
                .channel = network::ReliableChannel(token),
                .lastReceived = now,
                .lastSent = now,
                .knownEntities = {},
            });
        trackRequest(client.channel, request.packet, now);
        for (const PlayerJoined& other : alreadyHere) {
            client.channel.queueReliable(other);
        }
        accept(_socket, client, now);

        std::cout << "r-type_server: player " << static_cast<int>(client.playerId) << " '"
                  << client.name << "' joined from " << client.endpoint << '\n';
    }
}

std::uint32_t ConnectionSystem::newToken(ecs::World& world)
{
    std::uniform_int_distribution<std::uint32_t> distribution(1); // 1..max: never 0
    while (true) {
        const std::uint32_t token = distribution(_random);
        bool taken = false;
        world.query<NetClient>([&](ecs::Entity /*entity*/, NetClient& client) {
            taken = taken || client.token == token;
        });
        if (!taken) {
            return token;
        }
    }
}

} // namespace server
