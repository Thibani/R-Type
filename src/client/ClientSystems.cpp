#include "client/ClientSystems.hpp"

#include "client/Systems/AudioSystem.hpp"
#include "client/Systems/InputSystem.hpp"
#include "client/Systems/InputTickSystem.hpp"
#include "client/Systems/InterfaceSystem.hpp"
#include "client/Systems/MotionSystem.hpp"
#include "client/Systems/NetReceiveSystem.hpp"
#include "client/Systems/NetSendSystem.hpp"
#include "client/Systems/RenderSystem.hpp"
#include "client/Systems/SettingsSystem.hpp"
#include "client/Systems/StarfieldSystem.hpp"
#include "ecs/Loop/GameLoop.hpp"
#include "ecs/World/World.hpp"

#include <utility>

namespace client {

void installSystems(ecs::World& world, ecs::GameLoop& loop, sf::RenderWindow& window,
    ServerConnection& connection, const Assets& assets,
    std::optional<std::filesystem::path> settingsFile)
{
    loop.frameStart().add<InputSystem>(window, loop);
    loop.frameStart().add<NetReceiveSystem>(connection);

    world.systems().add<MotionSystem>();
    world.systems().add<StarfieldSystem>();
    world.systems().add<InputTickSystem>();

    loop.frameEnd().add<NetSendSystem>(connection);
    loop.frameEnd().add<SettingsSystem>(std::move(settingsFile));
    loop.frameEnd().add<AudioSystem>(assets);
    loop.frameEnd().add<RenderSystem>(window, assets);
    loop.frameEnd().add<InterfaceSystem>(window, assets);
}

} // namespace client
