#pragma once

#include <algorithm>
#include <stdexcept>

namespace ecs {

/// Turns irregular frame durations into a whole number of fixed-size steps.
///
/// The simulation must advance by the same amount every update, whatever the
/// frame rate: then the same inputs always give the same result, on a slow
/// laptop as on the server. The main loop measures how long the last frame
/// took and asks how many fixed steps to run now:
///
///     ecs::FixedTimestep timestep(1.0F / 60.0F);    // 60 updates per second
///     ecs::FrameClock clock;
///     ecs::Time time;
///     while (running) {
///         for (int i = timestep.advance(clock.tick()); i > 0; --i) {
///             time = time.advanced(timestep.stepSeconds());
///             world.update(time);
///         }
///         render(timestep.alpha());                  // client only
///     }
///
/// Time that does not fill a whole step is kept for the next frame, so no
/// time is ever lost or invented, except after a freeze (see `advance()`).
///
/// It does not read any clock: give it measured durations (`FrameClock`).
class FixedTimestep {
public:
    /// `stepSeconds`: duration of one update. `maxStepsPerFrame`: catch-up limit.
    /// Throws `std::invalid_argument` if either is not strictly positive.
    explicit constexpr FixedTimestep(float stepSeconds, int maxStepsPerFrame = 5)
        : _stepSeconds(stepSeconds)
        , _maxStepsPerFrame(maxStepsPerFrame)
    {
        if (!(stepSeconds > 0.0F) || maxStepsPerFrame < 1) {
            throw std::invalid_argument("FixedTimestep: step and max steps must be positive");
        }
    }

    /// Adds the duration of the last frame and returns how many steps to run now.
    ///
    /// After a freeze (debugger, window dragged, overloaded machine), at most
    /// `maxStepsPerFrame` steps are run and the rest of the backlog is
    /// dropped: the simulation slows down instead of trying to catch up
    /// forever. Negative or NaN durations count as 0.
    [[nodiscard]]
    constexpr int advance(float frameSeconds) noexcept
    {
        const float maxFrame = _stepSeconds * static_cast<float>(_maxStepsPerFrame);
        // Written as !(x > 0) so that NaN is rejected too
        const float frame = !(frameSeconds > 0.0F) ? 0.0F : std::min(frameSeconds, maxFrame);

        // The accumulator is always below one step, so this stays <= maxStepsPerFrame
        const auto step = static_cast<double>(_stepSeconds);
        _accumulator += static_cast<double>(frame);
        const int steps = std::min(static_cast<int>(_accumulator / step), _maxStepsPerFrame);
        _accumulator = std::max(_accumulator - (steps * step), 0.0);
        return steps;
    }

    /// Duration of one step, in seconds: the delta to give every update.
    [[nodiscard]]
    constexpr float stepSeconds() const noexcept
    {
        return _stepSeconds;
    }

    /// How far the real time is into the next step, from 0 to 1.
    /// The client uses it to draw between the last two simulated states.
    [[nodiscard]]
    constexpr float alpha() const noexcept
    {
        return std::min(static_cast<float>(_accumulator / static_cast<double>(_stepSeconds)), 1.0F);
    }

private:
    float _stepSeconds;
    int _maxStepsPerFrame;
    // Real time not simulated yet. Double: at thousands of frames per second,
    // float rounding of each tiny frame would lose whole steps
    double _accumulator{0.0};
};

} // namespace ecs
