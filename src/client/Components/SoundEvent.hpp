#pragma once

#include <cstdint>

namespace client {

enum class Sound : std::uint8_t {
    Shot, // A missile appeared (someone fired)
    Explosion, // An entity was killed
    PlayerDown, // A player's ship was destroyed
    PlayerJoined,
};

/// A sound to play this frame: created by the network system, played and
/// destroyed by the audio system.
struct SoundEvent {
    Sound sound{Sound::Shot};
};

} // namespace client
