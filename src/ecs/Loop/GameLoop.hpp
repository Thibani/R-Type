#pragma once

#include "ecs/System/SystemManager.hpp"
#include "ecs/Time/FixedTimestep.hpp"
#include "ecs/Time/Time.hpp"

#include <atomic>

namespace ecs {

class World;

/// The main loop shared by the server and the client.
///
/// Each frame runs three groups of systems, kept separate so that
/// simulation, networking and presentation never depend on each other:
///
///   1. `frameStart()` systems, once per frame: read input, receive packets
///   2. the World's own systems, 0..N fixed steps: the simulation
///   3. `frameEnd()` systems, once per frame: render, send packets
///
///     ecs::GameLoop loop(world, {.sleepUntilNextStep = true}); // server
///     loop.frameStart().add<NetReceiveSystem>(socket);
///     world.systems().add<MovementSystem>();
///     loop.frameEnd().add<NetSendSystem>(socket);
///     loop.run(); // until loop.stop()
///
/// Simulation systems receive a `Time` advanced by the fixed step; frame
/// systems receive one advanced by the real frame duration.
class GameLoop {
public:
    struct Settings {
        float stepSeconds{1.0F / 60.0F}; // Simulation rate: 60 updates per second
        int maxStepsPerFrame{5}; // Catch-up limit after a freeze
        bool sleepUntilNextStep{false}; // true on the server: no busy waiting
    };

    /// Default settings: 60 Hz, no sleeping (the client).
    explicit GameLoop(World& world);

    /// Throws `std::invalid_argument` if the settings are not positive.
    GameLoop(World& world, Settings settings);

    /// Systems run once per frame, before the simulation.
    [[nodiscard]]
    SystemManager& frameStart() noexcept;

    /// Systems run once per frame, after the simulation.
    [[nodiscard]]
    SystemManager& frameEnd() noexcept;

    /// Runs frames until `stop()` is called. A system that throws stops the
    /// loop and the exception reaches the caller.
    void run();

    /// Runs one frame that lasted `frameSeconds` of real time.
    /// `run()` calls it with the measured duration; tests call it directly.
    void runFrame(float frameSeconds);

    /// Asks `run()` to return after the current frame.
    /// Safe to call from a system, another thread or a signal handler.
    void stop() noexcept;

    [[nodiscard]]
    bool stopRequested() const noexcept;

    /// Time of the last simulation step (elapsed = simulated seconds).
    [[nodiscard]]
    const Time& simulationTime() const noexcept;

    /// Progress into the next step, 0..1: frameEnd() systems use it to draw
    /// between the last two simulated states.
    [[nodiscard]]
    float alpha() const noexcept;

private:
    World& _world;
    bool _sleepUntilNextStep;
    FixedTimestep _timestep;
    SystemManager _frameStart;
    SystemManager _frameEnd;
    Time _simulationTime;
    Time _frameTime;
    // Totals kept in double: summing millions of float deltas drifts
    long long _steps{0};
    double _frameElapsed{0.0};
    std::atomic<bool> _stopRequested{false};

    // A lock-free atomic is the only kind a signal handler may touch
    static_assert(std::atomic<bool>::is_always_lock_free);
};

} // namespace ecs
