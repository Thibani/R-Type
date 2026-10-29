#include "client/Systems/AudioSystem.hpp"

#include "client/Assets.hpp"
#include "client/Components/Settings.hpp"
#include "client/Components/SoundEvent.hpp"
#include "client/Singleton.hpp"
#include "ecs/World/World.hpp"

#include <cstddef>

namespace client {

AudioSystem::AudioSystem(const Assets& assets)
{
    for (std::size_t sound = 0; sound < _voices.size(); ++sound) {
        for (std::size_t voice = 0; voice < VoicesPerSound; ++voice) {
            _voices.at(sound).emplace_back(assets.sounds.at(sound));
        }
    }
}

void AudioSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    const auto volume = static_cast<float>(singleton<Settings>(world).volume); // 0 = muted
    std::vector<ecs::Entity> played;
    world.query<SoundEvent>([&](ecs::Entity entity, SoundEvent& event) {
        auto& voices = _voices.at(static_cast<std::size_t>(event.sound));
        sf::Sound* chosen = &voices.front(); // All busy: restart the first one
        for (sf::Sound& voice : voices) {
            if (voice.getStatus() != sf::Sound::Status::Playing) {
                chosen = &voice;
                break;
            }
        }
        chosen->setVolume(volume);
        chosen->play();
        played.push_back(entity);
    });
    for (const ecs::Entity entity : played) {
        world.destroyEntity(entity);
    }
}

} // namespace client
