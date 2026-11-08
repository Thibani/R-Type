#include "server/Systems/RestartSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/Match.hpp"
#include "server/Run.hpp"
#include "server/Singleton.hpp"

#include <variant>
#include <vector>

namespace server {

using namespace network::protocol;

void RestartSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    std::vector<ecs::Entity> requests;
    world.query<ClientMessage>([&](ecs::Entity entity, const ClientMessage& message) {
        if (std::holds_alternative<Restart>(message.payload)) {
            requests.push_back(entity);
        }
    });
    for (const ecs::Entity request : requests) {
        world.destroyEntity(request);
    }
    if (!requests.empty() && singleton<Match>(world).phase == GamePhase::GameOver) {
        startNewGame(world);
    }
}

} // namespace server
