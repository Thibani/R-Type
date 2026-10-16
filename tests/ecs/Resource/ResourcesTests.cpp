#include "ecs/Resource/Resources.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

using ecs::Resources;

namespace {

struct Score {
    std::uint32_t points{};
};

struct GameConfig {
    std::string name;
    int maxPlayers{};
};

/// A resource that can only be moved.
struct Connection {
    std::unique_ptr<int> handle;
};

} // namespace

TEST(Resources, InsertedResourceCanBeRetrieved)
{
    Resources resources;

    resources.insert(Score{.points = 10});

    EXPECT_TRUE(resources.contains<Score>());
    EXPECT_EQ(resources.get<Score>().points, 10U);
    EXPECT_EQ(resources.size(), 1U);
}

TEST(Resources, GetReturnsAModifiableReference)
{
    Resources resources;
    resources.insert(Score{});

    resources.get<Score>().points += 100;
    resources.get<Score>().points += 50;

    EXPECT_EQ(resources.get<Score>().points, 150U);
}

TEST(Resources, EmplaceBuildsTheResourceInPlace)
{
    Resources resources;

    const GameConfig& config = resources.emplace<GameConfig>(std::string("R-Type"), 4);

    EXPECT_EQ(config.name, "R-Type");
    EXPECT_EQ(resources.get<GameConfig>().maxPlayers, 4);
}

TEST(Resources, InsertingAgainReplacesTheValueAtTheSameAddress)
{
    Resources resources;
    const Score& reference = resources.insert(Score{.points = 1});

    resources.insert(Score{.points = 2});

    EXPECT_EQ(resources.size(), 1U);
    EXPECT_EQ(reference.points, 2U); // The old reference sees the new value
    EXPECT_EQ(&resources.get<Score>(), &reference);
}

TEST(Resources, ReferencesStayValidWhenOtherResourcesAreAdded)
{
    Resources resources;
    const Score& score = resources.insert(Score{.points = 7});

    resources.emplace<GameConfig>(std::string("a"), 1);
    resources.insert(Connection{});
    resources.insert(42);
    resources.insert(3.5);

    EXPECT_EQ(&resources.get<Score>(), &score);
    EXPECT_EQ(score.points, 7U);
}

TEST(Resources, MissingResourceIsReported)
{
    Resources resources;

    EXPECT_FALSE(resources.contains<Score>());
    EXPECT_EQ(resources.tryGet<Score>(), nullptr);
    EXPECT_THROW((void)resources.get<Score>(), std::out_of_range);
}

TEST(Resources, TypesAreIndependent)
{
    Resources resources;
    resources.insert(Score{.points = 3});

    EXPECT_FALSE(resources.contains<GameConfig>());
    EXPECT_EQ(resources.tryGet<GameConfig>(), nullptr);
}

TEST(Resources, RemoveDestroysTheResource)
{
    Resources resources;
    resources.insert(Score{});

    EXPECT_TRUE(resources.remove<Score>());
    EXPECT_FALSE(resources.contains<Score>());
    EXPECT_FALSE(resources.remove<Score>());
}

TEST(Resources, ConstAccess)
{
    Resources resources;
    resources.insert(Score{.points = 5});

    const Resources& constResources = resources;

    EXPECT_EQ(constResources.get<Score>().points, 5U);
    EXPECT_NE(constResources.tryGet<Score>(), nullptr);
    EXPECT_THROW((void)constResources.get<GameConfig>(), std::out_of_range);
}

TEST(Resources, AcceptsMoveOnlyResources)
{
    Resources resources;

    resources.insert(Connection{.handle = std::make_unique<int>(9)});
    resources.insert(Connection{.handle = std::make_unique<int>(10)}); // Replace

    EXPECT_EQ(*resources.get<Connection>().handle, 10);
}

TEST(Resources, ClearDestroysEverything)
{
    Resources resources;
    resources.insert(Score{});
    resources.emplace<GameConfig>(std::string("a"), 1);

    resources.clear();

    EXPECT_EQ(resources.size(), 0U);
    EXPECT_FALSE(resources.contains<Score>());
}
