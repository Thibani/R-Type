#include "ecs/Component/Component.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

struct Position {
    float x{};
    float y{};
};

struct PlayerTag { };

} // namespace

// The Component concept is checked at compile time.
static_assert(ecs::Component<Position>);
static_assert(ecs::Component<PlayerTag>);
static_assert(ecs::Component<int>);
static_assert(ecs::Component<std::string>);
static_assert(!ecs::Component<const Position>);
static_assert(!ecs::Component<Position&>);
static_assert(!ecs::Component<Position[4]>); // NOLINT(*-avoid-c-arrays): rejection is tested

TEST(Component, TypeIdIsStablePerType)
{
    EXPECT_EQ(ecs::componentTypeId<Position>(), ecs::componentTypeId<Position>());
}

TEST(Component, TypeIdDiffersBetweenTypes)
{
    EXPECT_NE(ecs::componentTypeId<Position>(), ecs::componentTypeId<PlayerTag>());
}
