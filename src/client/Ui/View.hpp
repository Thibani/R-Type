#pragma once

#include <SFML/Graphics/View.hpp>
#include <SFML/System/Vector2.hpp>

namespace client {

/// A view showing `logical` (e.g. the 1920 × 1080 arena) as large as the
/// window allows without stretching it: black bars fill the rest.
[[nodiscard]]
inline sf::View letterboxed(sf::Vector2f logical, sf::Vector2u window)
{
    sf::View view(sf::FloatRect({0.0F, 0.0F}, logical));
    const float windowRatio = static_cast<float>(window.x) / static_cast<float>(window.y);
    const float logicalRatio = logical.x / logical.y;
    if (windowRatio > logicalRatio) { // Too wide: bars left and right
        const float width = logicalRatio / windowRatio;
        view.setViewport(sf::FloatRect({(1.0F - width) / 2.0F, 0.0F}, {width, 1.0F}));
    } else { // Too tall: bars top and bottom
        const float height = windowRatio / logicalRatio;
        view.setViewport(sf::FloatRect({0.0F, (1.0F - height) / 2.0F}, {1.0F, height}));
    }
    return view;
}

} // namespace client
