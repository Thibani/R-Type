#include "server/Systems/SessionReceiveSystem.hpp"

#include "ecs/World/World.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/ReceivedPacket.hpp"
#include "server/Network.hpp"

#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace server {

using namespace network::protocol;

namespace {

std::optional<ecs::Entity> findSession(ecs::World& world, const ReceivedPacket& received)
{
    std::optional<ecs::Entity> found;
    world.query<NetClient>([&](ecs::Entity entity, NetClient& client) {
        if (client.endpoint == received.from && client.token == received.packet.sessionToken) {
            found = entity;
        }
    });
    return found;
}

} // namespace

void SessionReceiveSystem::update(ecs::World& world, const ecs::Time& time)
{
    const TimePoint now = toTimePoint(time);

    std::vector<std::pair<ecs::Entity, ReceivedPacket>> received;
    world.query<ReceivedPacket>([&](ecs::Entity entity, ReceivedPacket& packet) {
        received.emplace_back(entity, std::move(packet));
    });

    for (auto& [packetEntity, packet] : received) {
        world.destroyEntity(packetEntity);

        const auto session = findSession(world, packet);
        if (!session || world.hasComponent<Disconnecting>(*session)) {
            ++_unknownSenders;
            continue;
        }
        auto& client = world.getComponent<NetClient>(*session);
        auto delivered = client.channel.receive(packet.packet, now);
        if (!delivered) {
            // Too many reliable messages out of order: not an honest client
            world.addComponent(*session,
                Disconnecting{
                    .reason = LeaveReason::Kicked,
                    .notify = DisconnectReason::ProtocolError,
                });
            continue;
        }
        client.lastReceived = now;

        for (Payload& payload : *delivered) {
            if (std::holds_alternative<Heartbeat>(payload)) {
                continue; // Its only job, keeping the session alive, is done
            }
            if (std::holds_alternative<Disconnect>(payload)) {
                world.addComponent(
                    *session, Disconnecting{.reason = LeaveReason::Quit, .notify = std::nullopt});
                break;
            }
            world.addComponent(world.createEntity(),
                ClientMessage{.client = *session, .payload = std::move(payload)});
        }
    }
}

std::size_t SessionReceiveSystem::unknownSenderCount() const noexcept
{
    return _unknownSenders;
}

} // namespace server
