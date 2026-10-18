#pragma once

#include "network/Protocol/Protocol.hpp"

#include <cstdint>
#include <vector>

namespace client {

/// Singleton: keys held right now, as an INPUT mask (§4.2).
struct PlayerControls {
    std::uint8_t mask{0};
};

/// Singleton: the last input masks, one per simulation tick (60 Hz). Every
/// INPUT repeats up to 8 of them, so a lost datagram costs nothing (§4.2).
struct InputHistory {
    std::uint32_t firstTick{0}; // Tick of masks[0]
    std::vector<std::uint8_t> masks; // At most MaxInputMasks, oldest first
    bool unsent{false}; // A tick was recorded since the last INPUT
};

} // namespace client
