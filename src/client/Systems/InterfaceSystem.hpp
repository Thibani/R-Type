#pragma once

#include "ecs/System/System.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

namespace client {

struct Assets;

/// Frame end, after RenderSystem: draws the current screen of the `Session`
/// (connection form, waiting room, in-game HUD and menu, game over) on the
/// 1280 × 720 interface canvas, then shows the frame.
class InterfaceSystem final : public ecs::System {
public:
    InterfaceSystem(sf::RenderWindow& window, const Assets& assets) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    sf::RenderWindow& _window;
    const Assets& _assets;
};

} // namespace client
