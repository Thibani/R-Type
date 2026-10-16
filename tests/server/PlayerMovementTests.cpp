// Ship movement, through the same simulation systems the server runs, in the
// same order: PlayerControl → Movement → PlayerBounds, at the 60 Hz step.

#include "TestPlayers.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/Motion.hpp"
#include "game/Ship.hpp"
#include "game/Systems/MovementSystem.hpp"
#include "network/Protocol/Protocol.hpp"
#include "server/Components/Player.hpp"
#include "server/Systems/PlayerBoundsSystem.hpp"
#include "server/Systems/PlayerControlSystem.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>

using namespace network::protocol;
using game::Position;
using game::Velocity;
using testing_players::addPlayer;

namespace {

constexpr float Step = 1.0F / 60.0F;

/// Several keys held together, combined without signed integer promotion.
std::uint8_t keys(std::initializer_list<std::uint8_t> held)
{
    unsigned mask = 0;
    for (const std::uint8_t key : held) {
        mask |= key;
    }
    return static_cast<std::uint8_t>(mask);
}

/// One player and their ship, at a chosen position, run through the systems.
class MovementTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        _player = addPlayer(_world, 0);
        _ship = addShip(0, game::arena::Width / 2.0F, game::arena::Height / 2.0F);
        _world.systems().add<server::PlayerControlSystem>();
        _world.systems().add<game::MovementSystem>();
        _world.systems().add<server::PlayerBoundsSystem>();
    }

    ecs::Entity addShip(std::uint8_t playerId, float x, float y)
    {
        const ecs::Entity entity = _world.createEntity();
        _world.addComponent(entity, server::PlayerShip{.playerId = playerId});
        _world.addComponent(entity, Position{.x = x, .y = y});
        _world.addComponent(entity, Velocity{});
        return entity;
    }

    void press(ecs::Entity who, std::uint8_t mask)
    {
        _world.addComponent(
            who, server::PlayerInput{.mask = mask, .lastTick = 0, .received = true});
    }

    void runSteps(int steps)
    {
        for (int i = 0; i < steps; ++i) {
            _world.update(ecs::Time{Step, 0.0F});
        }
    }

    [[nodiscard]]
    const Position& positionOf(ecs::Entity entity)
    {
        return _world.getComponent<Position>(entity);
    }

    [[nodiscard]]
    const Velocity& velocityOf(ecs::Entity entity)
    {
        return _world.getComponent<Velocity>(entity);
    }

    ecs::World _world;
    ecs::Entity _player;
    ecs::Entity _ship;
};

constexpr float CentreX = game::arena::Width / 2.0F;
constexpr float CentreY = game::arena::Height / 2.0F;

} // namespace

TEST_F(MovementTest, RightForOneSecondMovesByTheShipSpeed)
{
    press(_player, input::Right);

    runSteps(60);

    EXPECT_NEAR(positionOf(_ship).x, CentreX + game::ship::Speed, 0.01F);
    EXPECT_FLOAT_EQ(positionOf(_ship).y, CentreY);
}

TEST_F(MovementTest, UpMovesTowardTheTopOfTheArena)
{
    press(_player, input::Up);

    runSteps(30);

    EXPECT_LT(positionOf(_ship).y, CentreY); // y grows downwards
    EXPECT_FLOAT_EQ(positionOf(_ship).x, CentreX);
}

TEST_F(MovementTest, DiagonalIsNotFaster)
{
    press(_player, keys({input::Down, input::Left}));

    runSteps(1);

    const Velocity& velocity = velocityOf(_ship);
    EXPECT_NEAR(std::hypot(velocity.dx, velocity.dy), game::ship::Speed, 0.01F);
    EXPECT_LT(velocity.dx, 0.0F);
    EXPECT_GT(velocity.dy, 0.0F);
}

TEST_F(MovementTest, OppositeKeysCancelOut)
{
    press(_player, keys({input::Left, input::Right, input::Up, input::Down}));

    runSteps(10);

    EXPECT_FLOAT_EQ(positionOf(_ship).x, CentreX);
    EXPECT_FLOAT_EQ(positionOf(_ship).y, CentreY);
}

TEST_F(MovementTest, FireAloneDoesNotMove)
{
    press(_player, input::Fire);

    runSteps(10);

    EXPECT_FLOAT_EQ(positionOf(_ship).x, CentreX);
}

TEST_F(MovementTest, ReleasingTheKeysStopsTheShip)
{
    press(_player, input::Right);
    runSteps(10);
    const float reached = positionOf(_ship).x;

    press(_player, 0);
    runSteps(10);

    EXPECT_FLOAT_EQ(positionOf(_ship).x, reached);
    EXPECT_FLOAT_EQ(velocityOf(_ship).dx, 0.0F);
}

TEST_F(MovementTest, NoInputYetMeansNoMovement)
{
    runSteps(10); // The _player never sent an INPUT

    EXPECT_FLOAT_EQ(positionOf(_ship).x, CentreX);
}

TEST_F(MovementTest, ShipStopsAtEveryEdge)
{
    // 5 s of holding a direction is more than enough to cross the arena
    struct Edge {
        std::uint8_t mask;
        float x;
        float y;
    };
    const std::array edges{
        Edge{.mask = input::Right, .x = game::arena::Width - game::ship::HalfWidth, .y = CentreY},
        Edge{.mask = input::Left, .x = game::ship::HalfWidth, .y = CentreY},
        Edge{.mask = input::Up, .x = CentreX, .y = game::ship::HalfHeight},
        Edge{.mask = input::Down, .x = CentreX, .y = game::arena::Height - game::ship::HalfHeight},
    };
    for (const auto& edge : edges) {
        SCOPED_TRACE(static_cast<int>(edge.mask));
        _world.getComponent<Position>(_ship) = Position{.x = CentreX, .y = CentreY};
        press(_player, edge.mask);

        runSteps(300);

        EXPECT_FLOAT_EQ(positionOf(_ship).x, edge.x);
        EXPECT_FLOAT_EQ(positionOf(_ship).y, edge.y);
    }
}

TEST_F(MovementTest, VelocityIntoAnEdgeIsCancelled)
{
    // Clients extrapolate from velocity: a ship pinned to the edge must not
    // look like it keeps going
    press(_player, keys({input::Right, input::Up}));

    runSteps(300);

    EXPECT_FLOAT_EQ(velocityOf(_ship).dx, 0.0F);
    EXPECT_FLOAT_EQ(velocityOf(_ship).dy, 0.0F);
}

TEST_F(MovementTest, EachShipFollowsItsOwnPlayer)
{
    const ecs::Entity otherPlayer = addPlayer(_world, 1);
    const ecs::Entity otherShip = addShip(1, CentreX, CentreY);
    press(_player, input::Right);
    press(otherPlayer, input::Left);

    runSteps(30);

    EXPECT_GT(positionOf(_ship).x, CentreX);
    EXPECT_LT(positionOf(otherShip).x, CentreX);
}
