#pragma once

#include "ecs/System/System.hpp"

#include <SFML/Audio/Sound.hpp>

#include <array>
#include <vector>

namespace client {

struct Assets;

/// Frame end: plays each `SoundEvent` of the frame, then destroys it.
/// A few voices per sound let the same effect overlap (several shots).
class AudioSystem final : public ecs::System {
public:
    static constexpr std::size_t VoicesPerSound = 4;

    explicit AudioSystem(const Assets& assets);

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    std::array<std::vector<sf::Sound>, 4> _voices; // Indexed by `Sound`
};

} // namespace client
