#pragma once

#include <cstddef>

namespace client {

/// Which of `frameCount` frames an animation played back and forth shows at
/// `seconds`, at `framesPerSecond`: 0, 1, …, n−1, n−2, …, 1, 0, 1… No frame
/// is shown twice in a row, so the loop has no visible jump.
[[nodiscard]]
constexpr std::size_t pingPongFrame(
    std::size_t frameCount, float framesPerSecond, float seconds) noexcept
{
    if (frameCount <= 1 || framesPerSecond <= 0.0F || seconds <= 0.0F) {
        return 0;
    }
    const std::size_t period = 2 * (frameCount - 1); // There and back, ends not repeated
    const auto step = static_cast<std::size_t>(seconds * framesPerSecond) % period;
    return step < frameCount ? step : period - step;
}

} // namespace client
