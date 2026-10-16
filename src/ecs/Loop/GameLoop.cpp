#include "ecs/Loop/GameLoop.hpp"

#include "ecs/Time/FrameClock.hpp"
#include "ecs/World/World.hpp"

#include <chrono>
#include <thread>

namespace ecs {

// Not a default argument: Settings is incomplete inside its own class
GameLoop::GameLoop(World& world)
    : GameLoop(world, Settings{})
{
}

GameLoop::GameLoop(World& world, Settings settings)
    : _world(world)
    , _sleepUntilNextStep(settings.sleepUntilNextStep)
    , _timestep(settings.stepSeconds, settings.maxStepsPerFrame)
{
}

SystemManager& GameLoop::frameStart() noexcept
{
    return _frameStart;
}

SystemManager& GameLoop::frameEnd() noexcept
{
    return _frameEnd;
}

void GameLoop::run()
{
    FrameClock clock;
    while (!stopRequested()) {
        runFrame(clock.tick());

        if (_sleepUntilNextStep && !stopRequested()) {
            // Nothing to simulate before the next step is due
            const float untilNextStep = (1.0F - _timestep.alpha()) * _timestep.stepSeconds();
            std::this_thread::sleep_for(std::chrono::duration<float>(untilNextStep));
        }
    }
}

void GameLoop::runFrame(float frameSeconds)
{
    _frameElapsed += static_cast<double>(frameSeconds);
    _frameTime = Time{frameSeconds, static_cast<float>(_frameElapsed)};
    _frameStart.update(_world, _frameTime);

    for (int steps = _timestep.advance(frameSeconds); steps > 0; --steps) {
        ++_steps;
        const double elapsed = static_cast<double>(_steps) * static_cast<double>(_timestep.stepSeconds());
        _simulationTime = Time{_timestep.stepSeconds(), static_cast<float>(elapsed)};
        _world.update(_simulationTime);
    }

    _frameEnd.update(_world, _frameTime);
}

void GameLoop::stop() noexcept
{
    _stopRequested.store(true);
}

bool GameLoop::stopRequested() const noexcept
{
    return _stopRequested.load();
}

const Time& GameLoop::simulationTime() const noexcept
{
    return _simulationTime;
}

float GameLoop::alpha() const noexcept
{
    return _timestep.alpha();
}

} // namespace ecs
