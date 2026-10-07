#pragma once

namespace ecs {

/// Timing information of the current frame, given to every system.
///
/// Gameplay must never depend on CPU speed. Instead of moving "5 pixels per
/// frame", systems move "N units per second" and scale by the frame duration:
///
///     position.x += velocity.dx * time.deltaSeconds();
///
/// `Time` is a plain value: it does not read any clock. Whoever runs the main
/// loop measures the elapsed time and builds the next `Time` with `advanced()`.
/// This keeps systems deterministic and trivial to test.
class Time {
public:
    /// Time of the very first frame: nothing has elapsed yet.
    constexpr Time() noexcept = default;

    constexpr Time(float deltaSeconds, float elapsedSeconds) noexcept
        : _deltaSeconds(deltaSeconds)
        , _elapsedSeconds(elapsedSeconds)
    {
    }

    /// Duration of the current frame, in seconds.
    [[nodiscard]]
    constexpr float deltaSeconds() const noexcept
    {
        return _deltaSeconds;
    }

    /// Total time elapsed since the start of the simulation, in seconds.
    [[nodiscard]]
    constexpr float elapsedSeconds() const noexcept
    {
        return _elapsedSeconds;
    }

    /// Returns the time of the next frame, `deltaSeconds` later.
    [[nodiscard]]
    constexpr Time advanced(float deltaSeconds) const noexcept
    {
        return Time{deltaSeconds, _elapsedSeconds + deltaSeconds};
    }

private:
    float _deltaSeconds{0.0F};
    float _elapsedSeconds{0.0F};
};

} // namespace ecs
