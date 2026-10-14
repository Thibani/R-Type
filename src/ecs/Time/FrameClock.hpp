#pragma once

#include <chrono>

namespace ecs {

/// Measures the real time between two calls to `tick()`.
///
/// The only class of the ECS that reads a clock. It is meant for the main
/// loop; systems receive a `Time` and never call it.
///
/// `Clock` defaults to `std::chrono::steady_clock`, which never jumps when the
/// system time changes. Tests give a fake clock instead of sleeping.
template <typename Clock = std::chrono::steady_clock>
class BasicFrameClock {
public:
    BasicFrameClock() noexcept
        : _last(Clock::now())
    {
    }

    /// Seconds elapsed since the previous `tick()` (or since construction).
    float tick() noexcept
    {
        const auto now = Clock::now();
        const std::chrono::duration<float> elapsed = now - _last;
        _last = now;
        return elapsed.count();
    }

private:
    Clock::time_point _last;
};

using FrameClock = BasicFrameClock<>;

} // namespace ecs
