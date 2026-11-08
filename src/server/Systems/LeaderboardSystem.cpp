#include "server/Systems/LeaderboardSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/NetClient.hpp"

#include <iostream>
#include <utility>
#include <variant>
#include <vector>

namespace server {

using namespace network::protocol;

LeaderboardSystem::LeaderboardSystem(ScoreStore& scores) noexcept
    : _scores(scores)
{
}

void LeaderboardSystem::update(ecs::World& world, const ecs::Time& time)
{
    std::vector<ecs::Entity> requests;
    world.query<ClientMessage>([&](ecs::Entity entity, ClientMessage& message) {
        if (std::holds_alternative<LeaderboardRequest>(message.payload)) {
            requests.push_back(entity);
        }
    });

    for (const ecs::Entity entity : requests) {
        const ClientMessage message = world.getComponent<ClientMessage>(entity);
        world.destroyEntity(entity);
        auto* client = world.tryGetComponent<NetClient>(message.client);
        if (client == nullptr || !world.hasComponent<LoggedIn>(message.client)
            || world.hasComponent<Disconnecting>(message.client)) {
            continue; // Not logged in yet, or leaving during this frame
        }
        const Board board = std::get<LeaderboardRequest>(message.payload).board;
        Leaderboard answer;
        answer.board = board;
        for (ScoreStore::Game& game : _scores.best(board, LeaderboardSize)) {
            answer.best.push_back(
                LeaderboardEntry{.score = game.score, .names = std::move(game.names)});
        }
        if (const auto last = _scores.lastGameOf(board, client->name)) {
            answer.lastRank = last->rank;
            answer.lastScore = last->score;
        }
        client->channel.queueReliable(std::move(answer));
    }
    saveWhenDue(time);
}

void LeaderboardSystem::saveWhenDue(const ecs::Time& time)
{
    if (!_scores.hasChanges() || time.elapsedSeconds() - _lastSave < SecondsBetweenSaves) {
        return;
    }
    _lastSave = time.elapsedSeconds();
    if (!_scores.save()) { // Tried again in a second: the games stay in memory
        std::cerr << "r-type_server: cannot save the scores file\n";
    }
}

} // namespace server
