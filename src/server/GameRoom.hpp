#pragma once

#include "ecs/Entity/Entity.hpp"
#include "ecs/Loop/GameLoop.hpp"
#include "ecs/World/World.hpp"
#include "network/Protocol/Protocol.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace server {

/// One game: its own World (ships, Bydos, missiles, score) and its own loop. Two games
/// cannot touch each other, since no query of one can see the entities of the other.
///
/// The sessions stay in the server's main World, for the whole connection. A player in
/// the game also has an entity here, whose `InGame` says which session it belongs to.
class GameRoom {
public:
    /// `sessions` must outlive the room. `firstTick`: see `startGame`.
    GameRoom(ecs::World& sessions, std::uint32_t firstTick);

    ecs::World world;
    ecs::GameLoop loop{world};
    std::vector<std::string> names; // The accounts that started it, for the leaderboard
    bool scored{false}; // Its score entered the leaderboard: at GAME_OVER, or when it ended
};

/// Every game running, on a singleton of the main World. A room goes with its last player:
/// that is when its score enters the leaderboard (`RoomSystem`).
struct GameRooms {
    std::map<std::uint32_t, std::unique_ptr<GameRoom>> rooms;
    std::uint32_t nextId{0};
};

/// A new game for these sessions, in this order: the first one is player 0. `now`: the
/// server's elapsed seconds.
///
/// Snapshot ticks start from that time, so they keep growing for a client that
/// goes from one game to the next: nothing late from the old game counts in the new one.
void startGame(ecs::World& sessions, std::span<const ecs::Entity> players, float now);

/// Takes a session out of its game, if it is in one: it keeps nothing of it. The others
/// get PLAYER_LEFT, and the ship goes at the next frame of the room.
void leaveRoom(ecs::World& sessions, ecs::Entity session, network::protocol::LeaveReason reason);

} // namespace server
