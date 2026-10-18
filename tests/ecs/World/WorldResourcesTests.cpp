#include "ecs/Resource/AssetManager.hpp"
#include "ecs/System/System.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

using ecs::Entity;
using ecs::Time;
using ecs::World;

namespace {

struct Score {
    std::uint32_t points{};
};

struct Enemy { };

/// Adds 10 points per enemy each frame, through a shared resource.
class ScoringSystem final : public ecs::System {
public:
    void update(World& world, const Time& /*time*/) override
    {
        auto& score = world.resources().get<Score>();
        world.query<Enemy>([&](Entity, Enemy&) { score.points += 10; });
    }
};

/// Reads the score written by another system, without knowing it.
class ScoreDisplaySystem final : public ecs::System {
public:
    void update(World& world, const Time& /*time*/) override
    {
        lastSeen = world.resources().get<Score>().points;
    }

    std::uint32_t lastSeen{0};
};

struct Texture {
    std::string path;
};

} // namespace

TEST(WorldResources, SystemsShareDataThroughResources)
{
    World world;
    world.resources().insert(Score{});
    world.systems().add<ScoringSystem>();
    auto& display = world.systems().add<ScoreDisplaySystem>();
    for (int index = 0; index < 3; ++index) {
        world.addComponent(world.createEntity(), Enemy{});
    }

    world.update(Time{});
    world.update(Time{});

    EXPECT_EQ(world.resources().get<Score>().points, 60U);
    EXPECT_EQ(display.lastSeen, 60U);
}

TEST(WorldResources, ClearKeepsTheResources)
{
    World world;
    world.resources().insert(Score{.points = 5});

    world.clear();

    EXPECT_EQ(world.resources().get<Score>().points, 5U);
}

TEST(WorldResources, AssetManagerCanBeSharedAsAResource)
{
    World world;
    auto& assets = world.resources().emplace<ecs::AssetManager>();
    assets.setLoader<Texture>(
        [](const std::string& path) { return std::make_unique<Texture>(Texture{.path = path}); });

    const auto texture = world.resources().get<ecs::AssetManager>().load<Texture>("ship.png");

    ASSERT_NE(texture, nullptr);
    EXPECT_EQ(texture->path, "ship.png");
}
