#include "server/Systems/ConnectionSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Crypto/Crypto.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/ReceivedPacket.hpp"
#include "server/Network.hpp"

#include <cstddef>
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
        .playerId = NoPlayer, // A place in the game comes later, with JOIN_RESULT
        .tickRate = ConnectionSystem::TickRate,
        .snapshotRate = ConnectionSystem::SnapshotRate,
        .maxPlayers = MaxPlayers,
        .publicKey = client.serverPublicKey,
    };
    sendPacket(socket, client.channel.wrap(payload, now), client.endpoint);
    client.lastSent = now;
}

std::size_t sessionCount(ecs::World& world)
{
    std::size_t count = 0;
    world.query<NetClient>([&](ecs::Entity /*entity*/, NetClient& /*client*/) { ++count; });
    return count;
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
        const auto& connectRequest = std::get<ConnectRequest>(request.packet.payload);
        if (NetClient* existing = findClient(world, request.from)) {
            if (connectRequest.publicKey != existing->clientPublicKey) {
                // Anybody can fake an address: this key must not replace the
                // session's. A real new client retries until this session times out.
                continue;
            }
            trackRequest(existing->channel, request.packet, now);
            existing->lastReceived = now;
            accept(_socket, *existing, now); // The previous ACCEPT was lost
            continue;
        }

        std::string name = connectRequest.name;
        if (name.empty()) {
            reject(_socket, request.from, RejectReason::InvalidName);
            continue;
        }
        if (sessionCount(world) >= ConnectionSystem::MaxSessions) {
            reject(_socket, request.from, RejectReason::ServerFull);
            continue;
        }

        // Here, not earlier: the key exchange costs CPU, and a refused request must
        // not trigger it. The secret key is not kept once the exchange is done.
        const network::crypto::KeyPair keys = network::crypto::generateKeyPair();
        const auto sessionKey = network::crypto::deriveSessionKey(
            keys, connectRequest.publicKey, network::crypto::Role::Server);
        if (!sessionKey) {
            continue; // No honest client sends an invalid key: no answer
        }

        const std::uint32_t token = newToken(world);
        const ecs::Entity entity = world.createEntity();
        NetClient& client = world.addComponent(entity,
            NetClient{
                .endpoint = request.from,
                .token = token,
                .name = std::move(name),
                .channel = network::ReliableChannel(token),
                .cipher
                = network::crypto::SessionCipher(*sessionKey, network::crypto::Role::Server),
                .clientPublicKey = connectRequest.publicKey,
                .serverPublicKey = keys.publicKey,
                .connectedAt = now,
                .loginRequested = false,
                .lastReceived = now,
                .lastSent = now,
                .knownEntities = {},
                .knownState = std::nullopt,
            });
        trackRequest(client.channel, request.packet, now);
        accept(_socket, client, now);
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
