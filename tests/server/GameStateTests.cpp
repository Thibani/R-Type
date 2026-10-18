// The authoritative game state, system by system, on a plain World: no
// sockets, the tests create the components the network systems would.

#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "game/Arena.hpp"
#include "game/Components/Motion.hpp"
#include "network/Protocol/Packets.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Components/NetEntity.hpp"
#include "server/Components/Player.hpp"
#include "server/NetIds.hpp"
#include "server/Singleton.hpp"
#include "server/Systems/InputSystem.hpp"
#include "server/Systems/PlayerShipSystem.hpp"
#include "server/Systems/TickSystem.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

using namespace network::protocol;
using server::InputSystem;
using server::NetEntity;
using server::PlayerInput;
using server::PlayerShip;
using server::PlayerShipSystem;

namespace {

ecs::Entity addPlayer(ecs::World& world, std::uint8_t playerId)
{
    const ecs::Entity player = world.createEntity();
    world.addComponent(player,
        server::NetClient{
            .endpoint = {},
            .token = playerId + 1U,
            .playerId = playerId,
            .name = "Player" + std::to_string(playerId),
            .channel = network::ReliableChannel(playerId + 1U),
            .lastReceived = {},
            .lastSent = {},
        });
    return player;
}

void receiveInput(ecs::World& world, ecs::Entity player, Input input)
{
    world.addComponent(
        world.createEntity(), server::ClientMessage{.client = player, .payload = input});
}

Input inputAt(std::uint32_t firstTick, std::vector<std::uint8_t> masks)
{
    Input input{.firstTick = firstTick, .count = static_cast<std::uint8_t>(masks.size())};
    for (std::size_t i = 0; i < masks.size(); ++i) {
        input.masks.at(i) = masks[i];
    }
    return input;
}

std::vector<ecs::Entity> ships(ecs::World& world)
{
    std::vector<ecs::Entity> found;
    world.query<PlayerShip>(
        [&](ecs::Entity entity, PlayerShip& /*ship*/) { found.push_back(entity); });
    return found;
}

/// A fresh ship: in its player's lane, still, owned and networked as a ship.
void expectSpawnedShip(ecs::World& world, ecs::Entity ship)
{
    const auto playerId = world.getComponent<PlayerShip>(ship).playerId;
    const auto spawn = game::arena::playerSpawn(playerId);
    const auto& position = world.getComponent<game::Position>(ship);
    const auto& net = world.getComponent<NetEntity>(ship);

    EXPECT_FLOAT_EQ(position.x, spawn.x);
    EXPECT_FLOAT_EQ(position.y, spawn.y);
    EXPECT_TRUE(world.hasComponent<game::Velocity>(ship));
    EXPECT_EQ(net.kind, EntityKind::PlayerShip);
    EXPECT_EQ(net.owner, playerId);
}

} // namespace

// --- Simulation tick ----------------------------------------------------------

TEST(GameState, TickCountsSimulationSteps)
{
    ecs::World world;
    server::TickSystem tick;

    for (int step = 0; step < 3; ++step) {
        tick.update(world, ecs::Time{});
    }

    EXPECT_EQ(server::singleton<server::SimulationTick>(world).value, 3U);
}

TEST(GameState, SingletonIsCreatedOnceAndShared)
{
    ecs::World world;

    server::singleton<server::SimulationTick>(world).value = 7;

    EXPECT_EQ(server::singleton<server::SimulationTick>(world).value, 7U);
    EXPECT_EQ(world.entityCount(), 1U);
}

// --- Inputs (§4.2, §7.1) ------------------------------------------------------

TEST(GameState, InputBecomesThePlayersInput)
{
    ecs::World world;
    const ecs::Entity player = addPlayer(world, 0);
    InputSystem system;

    receiveInput(world, player, inputAt(10, {input::Up, input::Up | input::Fire}));
    system.update(world, ecs::Time{});

    const auto& state = world.getComponent<PlayerInput>(player);
    EXPECT_TRUE(state.received);
    EXPECT_EQ(state.mask, input::Up | input::Fire); // The newest of the two ticks
    EXPECT_EQ(state.lastTick, 11U);
    EXPECT_EQ(world.entityCount(), 1U); // The message was consumed
}

TEST(GameState, RepeatedTicksAreIgnored)
{
    // Each INPUT repeats the last masks: only new ticks count
    ecs::World world;
    const ecs::Entity player = addPlayer(world, 0);
    InputSystem system;
    receiveInput(world, player, inputAt(10, {input::Left, input::Right}));
    system.update(world, ecs::Time{});

    receiveInput(world, player, inputAt(9, {input::Down, input::Down, input::Down}));
    system.update(world, ecs::Time{});

    const auto& state = world.getComponent<PlayerInput>(player);
    EXPECT_EQ(state.lastTick, 11U); // Ticks 9..11: only 11 would be new, and it is not
    EXPECT_EQ(state.mask, input::Right);
}

TEST(GameState, LostInputIsRecoveredFromTheNextPacket)
{
    ecs::World world;
    const ecs::Entity player = addPlayer(world, 0);
    InputSystem system;
    receiveInput(world, player, inputAt(10, {input::Up}));
    system.update(world, ecs::Time{});

    // Ticks 11 and 12 were lost; packet 13 repeats them
    receiveInput(world, player, inputAt(11, {input::Up, input::Down, input::Left}));
    system.update(world, ecs::Time{});

    EXPECT_EQ(world.getComponent<PlayerInput>(player).lastTick, 13U);
    EXPECT_EQ(world.getComponent<PlayerInput>(player).mask, input::Left);
}

TEST(GameState, InputTooFarAheadIsIgnored)
{
    ecs::World world;
    const ecs::Entity player = addPlayer(world, 0);
    InputSystem system;
    receiveInput(world, player, inputAt(100, {input::Up}));
    system.update(world, ecs::Time{});

    receiveInput(world, player, inputAt(100 + InputSystem::MaxTicksAhead + 1, {input::Fire}));
    system.update(world, ecs::Time{});
    EXPECT_EQ(world.getComponent<PlayerInput>(player).lastTick, 100U);

    receiveInput(world, player, inputAt(100 + InputSystem::MaxTicksAhead, {input::Fire}));
    system.update(world, ecs::Time{});
    EXPECT_EQ(world.getComponent<PlayerInput>(player).lastTick, 100 + InputSystem::MaxTicksAhead);
}

TEST(GameState, InputOfAPlayerWhoLeftIsDropped)
{
    ecs::World world;
    const ecs::Entity player = addPlayer(world, 0);
    receiveInput(world, player, inputAt(1, {input::Up}));
    world.destroyEntity(player);
    InputSystem system;

    system.update(world, ecs::Time{});

    EXPECT_EQ(world.entityCount(), 0U); // Message consumed, nothing created
}

// --- Ships --------------------------------------------------------------------

TEST(GameState, EveryPlayerGetsOneShipInItsLane)
{
    ecs::World world;
    (void)addPlayer(world, 0);
    (void)addPlayer(world, 2);
    PlayerShipSystem system;

    system.update(world, ecs::Time{});
    system.update(world, ecs::Time{}); // Not twice

    const auto created = ships(world);
    ASSERT_EQ(created.size(), 2U);
    std::set<std::uint16_t> netIds;
    for (const ecs::Entity ship : created) {
        expectSpawnedShip(world, ship);
        netIds.insert(world.getComponent<NetEntity>(ship).netId);
    }
    EXPECT_EQ(netIds.size(), 2U); // Distinct network ids
}

TEST(GameState, ShipIsDestroyedWhenItsPlayerLeaves)
{
    ecs::World world;
    const ecs::Entity leaving = addPlayer(world, 0);
    (void)addPlayer(world, 1);
    PlayerShipSystem system;
    system.update(world, ecs::Time{});

    world.destroyEntity(leaving);
    system.update(world, ecs::Time{});

    const auto remaining = ships(world);
    ASSERT_EQ(remaining.size(), 1U);
    EXPECT_EQ(world.getComponent<PlayerShip>(remaining[0]).playerId, 1);
}

TEST(GameState, PlayerSpawnPointsStayInTheArena)
{
    for (std::uint8_t id = 0; id < MaxPlayers; ++id) {
        const auto spawn = game::arena::playerSpawn(id);
        EXPECT_GT(spawn.y, 0.0F);
        EXPECT_LT(spawn.y, game::arena::Height);
        EXPECT_GT(spawn.x, 0.0F);
        EXPECT_LT(spawn.x, game::arena::Width);
    }
}

// --- Network ids (§5.1) -------------------------------------------------------

TEST(GameState, NetIdsSkipThoseInUse)
{
    ecs::World world;
    world.addComponent(world.createEntity(), NetEntity{.netId = 0});
    world.addComponent(world.createEntity(), NetEntity{.netId = 1});

    EXPECT_EQ(server::allocateNetId(world), 2);
}

TEST(GameState, FreedNetIdIsNotReusedAtOnce)
{
    ecs::World world;
    const std::uint16_t first = server::allocateNetId(world);
    // Nothing holds `first` any more, yet the next id is a new one
    EXPECT_NE(server::allocateNetId(world), first);
}

TEST(GameState, NetIdsCycleAndNeverUseNoEntity)
{
    ecs::World world;
    server::singleton<server::NetIdCounter>(world).next = NoEntity - 1;

    EXPECT_EQ(server::allocateNetId(world), NoEntity - 1);
    EXPECT_EQ(server::allocateNetId(world), 0); // 0xFFFF skipped: it means "none"
}
