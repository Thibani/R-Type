#include "server/Systems/TimeoutSystem.hpp"

#include "ecs/World/World.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Network.hpp"

#include <optional>
#include <vector>

namespace server {

using namespace network::protocol;

void TimeoutSystem::update(ecs::World& world, const ecs::Time& time)
{
    const TimePoint now = toTimePoint(time);

    std::vector<ecs::Entity> silent;
    world.query<NetClient>([&](ecs::Entity entity, NetClient& client) {
        if (now - client.lastReceived >= SessionTimeout || client.channel.timedOut(now)) {
            silent.push_back(entity);
        }
    });
    for (const ecs::Entity entity : silent) {
        if (!world.hasComponent<Disconnecting>(entity)) {
            // Nobody is listening at that address any more: nothing to notify
            world.addComponent(
                entity, Disconnecting{.reason = LeaveReason::Timeout, .notify = std::nullopt});
        }
    }
}

} // namespace server
