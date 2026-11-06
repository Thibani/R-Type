#include "client/Systems/StarfieldSystem.hpp"

#include "client/Components/Appearance.hpp"
#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/Motion.hpp"

#include <array>
#include <random>

namespace client {

namespace {

struct Layer {
    int count;
    float speed; // Units per second
    float size; // Units
};

// One full crossing of the far layer takes about a minute: a slow scroll
constexpr std::array<Layer, 3> Layers{{{90, 30.0F, 2.0F}, {50, 60.0F, 3.0F}, {20, 120.0F, 4.0F}}};

// The streaks climb to the upper right, along their own line, a few at a time
constexpr int StreakCount = 7;
constexpr float SqrtHalf = 0.70710678F;

} // namespace

void spawnStarfield(ecs::World& world)
{
    std::mt19937 random(42); // Same sky every run
    std::uniform_real_distribution<float> x(0.0F, game::arena::Width);
    std::uniform_real_distribution<float> y(0.0F, game::arena::Height);
    for (const Layer& layer : Layers) {
        for (int i = 0; i < layer.count; ++i) {
            const ecs::Entity star = world.createEntity();
            world.addComponent(star, game::Position{.x = x(random), .y = y(random)});
            world.addComponent(star, game::Velocity{.dx = -layer.speed, .dy = 0.0F});
            world.addComponent(star, Star{.size = layer.size});
        }
    }

    std::uniform_real_distribution<float> speed(70.0F, 150.0F);
    std::uniform_real_distribution<float> length(260.0F, 620.0F);
    for (int i = 0; i < StreakCount; ++i) {
        const float along = speed(random) * SqrtHalf;
        const ecs::Entity streak = world.createEntity();
        world.addComponent(streak, game::Position{.x = x(random), .y = y(random)});
        world.addComponent(streak, game::Velocity{.dx = along, .dy = -along});
        world.addComponent(streak, Streak{.length = length(random)});
    }
}

void StarfieldSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    world.query<Star, game::Position>(
        [](ecs::Entity /*entity*/, Star& star, game::Position& position) {
            if (position.x < -star.size) {
                position.x += game::arena::Width + star.size;
            }
        });
    world.query<Streak, game::Position>(
        [](ecs::Entity /*entity*/, Streak& streak, game::Position& position) {
            // Its tail is its position: once out at the top or right, it goes back
            // down its own diagonal, far enough to enter from below or the left
            if (position.y + streak.length < 0.0F || position.x > game::arena::Width) {
                position.x -= game::arena::Width;
                position.y += game::arena::Width;
            }
        });
}

} // namespace client
