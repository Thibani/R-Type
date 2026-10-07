#include "ecs/System/SystemManager.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using ecs::SystemManager;
using ecs::Time;
using ecs::World;

namespace {

/// Records its name in a shared log every time it runs.
class LoggingSystem : public ecs::System {
public:
    LoggingSystem(std::vector<std::string>& log, std::string name)
        : _log(log)
        , _name(std::move(name))
    {
    }

    void update(World& /*world*/, const Time& /*time*/) override
    {
        _log.get().push_back(_name);
    }

private:
    std::reference_wrapper<std::vector<std::string>> _log;
    std::string _name;
};

// Distinct types, since each system type can only be added once.
class FirstSystem final : public LoggingSystem {
public:
    explicit FirstSystem(std::vector<std::string>& log)
        : LoggingSystem(log, "first")
    {
    }
};

class SecondSystem final : public LoggingSystem {
public:
    explicit SecondSystem(std::vector<std::string>& log)
        : LoggingSystem(log, "second")
    {
    }
};

/// Remembers what it received during its last update.
class SpySystem final : public ecs::System {
public:
    void update(World& world, const Time& time) override
    {
        lastWorld = &world;
        lastDelta = time.deltaSeconds();
        ++updateCount;
    }

    World* lastWorld{nullptr};
    float lastDelta{0.0F};
    int updateCount{0};
};

} // namespace

TEST(SystemManager, RunsSystemsInInsertionOrder)
{
    std::vector<std::string> log;
    World world;
    SystemManager systems;
    systems.add<SecondSystem>(log);
    systems.add<FirstSystem>(log);

    systems.update(world, Time{});

    EXPECT_EQ(log, (std::vector<std::string>{"second", "first"}));
}

TEST(SystemManager, PassesTheWorldAndTheTime)
{
    World world;
    SystemManager systems;
    const auto& spy = systems.add<SpySystem>();

    systems.update(world, Time{0.5F, 0.5F});

    EXPECT_EQ(spy.lastWorld, &world);
    EXPECT_FLOAT_EQ(spy.lastDelta, 0.5F);
    EXPECT_EQ(spy.updateCount, 1);
}

TEST(SystemManager, AddingTheSameTypeTwiceThrows)
{
    SystemManager systems;
    systems.add<SpySystem>();

    EXPECT_THROW(systems.add<SpySystem>(), std::logic_error);
    EXPECT_EQ(systems.size(), 1U);
}

TEST(SystemManager, FindsSystemsByType)
{
    SystemManager systems;
    const auto& spy = systems.add<SpySystem>();

    EXPECT_TRUE(systems.has<SpySystem>());
    EXPECT_EQ(systems.find<SpySystem>(), &spy);
    EXPECT_FALSE(systems.has<FirstSystem>());
    EXPECT_EQ(systems.find<FirstSystem>(), nullptr);

    const SystemManager& constSystems = systems;
    EXPECT_EQ(constSystems.find<SpySystem>(), &spy);
}

TEST(SystemManager, RemovedSystemNoLongerRuns)
{
    std::vector<std::string> log;
    World world;
    SystemManager systems;
    systems.add<FirstSystem>(log);
    systems.add<SecondSystem>(log);

    EXPECT_TRUE(systems.remove<FirstSystem>());
    EXPECT_FALSE(systems.remove<FirstSystem>());

    systems.update(world, Time{});

    EXPECT_EQ(log, std::vector<std::string>{"second"});
    EXPECT_EQ(systems.size(), 1U);
}

TEST(SystemManager, ClearRemovesEverySystem)
{
    SystemManager systems;
    systems.add<SpySystem>();

    systems.clear();

    EXPECT_EQ(systems.size(), 0U);
    EXPECT_FALSE(systems.has<SpySystem>());
}
