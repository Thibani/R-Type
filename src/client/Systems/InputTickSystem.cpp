#include "client/Systems/InputTickSystem.hpp"

#include "client/Components/Controls.hpp"
#include "client/Components/Session.hpp"
#include "client/Singleton.hpp"
#include "ecs/World/World.hpp"

namespace client {

void InputTickSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    const Session& session = singleton<Session>(world);
    if (!session.playerId) {
        return; // Not connected
    }
    // In the menu or on another screen, the ship gets "no key" (the game goes on)
    const bool playing = session.screen == Screen::Game && !session.menuOpen;
    const std::uint8_t mask = playing ? singleton<PlayerControls>(world).mask : 0;

    InputHistory& history = singleton<InputHistory>(world);
    history.masks.push_back(mask);
    if (history.masks.size() > network::protocol::MaxInputMasks) {
        history.masks.erase(history.masks.begin());
        ++history.firstTick;
    }
    history.unsent = true;
}

} // namespace client
