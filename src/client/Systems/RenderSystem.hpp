#pragma once

#include "ecs/System/System.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

namespace client {

struct Assets;

/// Frame end: clears the window and, while playing, draws the arena: the
/// starfield, then every entity with a `game::Position` and an `Appearance`.
/// It knows nothing else about them (no network, no health, no rules).
class RenderSystem final : public ecs::System {
public:
    RenderSystem(sf::RenderWindow& window, const Assets& assets) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    sf::RenderWindow& _window;
    const Assets& _assets;
};

} // namespace client
