#pragma once

#include <cstdint>

namespace game {

/// Hit points left. A hit removes one; at 0 the entity is killed.
struct Health {
    std::uint8_t current{};
};

} // namespace game
