// The friends file, without any network: the rules, what survives a restart,
// and what happens when the file is damaged.

#include "TemporaryFile.hpp"
#include "TestPlayers.hpp"
#include "ecs/Time/Time.hpp"
#include "ecs/World/World.hpp"
#include "server/AccountStore.hpp"
#include "server/Components/NetClient.hpp"
#include "server/FriendStore.hpp"
#include "server/Systems/FriendSystem.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <variant>

using server::FriendStore;
using testing_players::TemporaryFile;
using AcceptResult = FriendStore::AcceptResult;
using RequestResult = FriendStore::RequestResult;

namespace {

FriendStore loadOrFail(const std::filesystem::path& file)
{
    auto store = FriendStore::load(file);
    EXPECT_TRUE(store.has_value()) << (store ? "" : store.error());
    return std::move(*store);
}

std::string contentOf(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream(path, std::ios::binary) << content;
}

std::string nameOf(std::size_t number)
{
    return "P" + std::to_string(number);
}

} // namespace

TEST(FriendStore, ARequestIsSeenFromBothSides)
{
    const TemporaryFile file;
    FriendStore store = loadOrFail(file.path());

    EXPECT_EQ(store.request("Ann", "Ben"), RequestResult::Sent);

    EXPECT_TRUE(store.requestsFrom("Ann").contains("Ben"));
    EXPECT_TRUE(store.requestsTo("Ben").contains("Ann"));
    EXPECT_TRUE(store.friendsOf("Ann").empty());
}

TEST(FriendStore, AcceptingMakesThemFriendsBothWays)
{
    const TemporaryFile file;
    FriendStore store = loadOrFail(file.path());
    (void)store.request("Ann", "Ben");

    EXPECT_EQ(store.accept("Ann", "Ben"), AcceptResult::Accepted);

    EXPECT_TRUE(store.friendsOf("Ann").contains("Ben"));
    EXPECT_TRUE(store.friendsOf("Ben").contains("Ann"));
    EXPECT_TRUE(store.requestsTo("Ben").empty());
    EXPECT_TRUE(store.requestsFrom("Ann").empty());
}

TEST(FriendStore, OnlyTheOneAskedCanAccept)
{
    const TemporaryFile file;
    FriendStore store = loadOrFail(file.path());
    (void)store.request("Ann", "Ben");

    EXPECT_EQ(store.accept("Ben", "Ann"), AcceptResult::NoRequest); // Ann accepting for Ben

    EXPECT_TRUE(store.friendsOf("Ann").empty());
}

TEST(FriendStore, AskingBackMakesThemFriends)
{
    const TemporaryFile file;
    FriendStore store = loadOrFail(file.path());
    (void)store.request("Ann", "Ben");

    EXPECT_EQ(store.request("Ben", "Ann"), RequestResult::BecameFriends);

    EXPECT_TRUE(store.friendsOf("Ann").contains("Ben"));
    EXPECT_TRUE(store.requestsTo("Ben").empty());
}

TEST(FriendStore, RefusedRulesOfARequest)
{
    const TemporaryFile file;
    FriendStore store = loadOrFail(file.path());

    EXPECT_EQ(store.request("Ann", "Ann"), RequestResult::Yourself);
    (void)store.request("Ann", "Ben");
    EXPECT_EQ(store.request("Ann", "Ben"), RequestResult::AlreadyLinked);
    (void)store.accept("Ann", "Ben");
    EXPECT_EQ(store.request("Ann", "Ben"), RequestResult::AlreadyLinked);
    EXPECT_EQ(store.request("Ben", "Ann"), RequestResult::AlreadyLinked);
}

TEST(FriendStore, DroppingARequestForgetsItOnBothSides)
{
    const TemporaryFile file;
    FriendStore store = loadOrFail(file.path());
    (void)store.request("Ann", "Ben");

    EXPECT_TRUE(store.dropRequest("Ann", "Ben"));
    EXPECT_FALSE(store.dropRequest("Ann", "Ben"));

    EXPECT_TRUE(store.requestsFrom("Ann").empty());
    EXPECT_TRUE(store.requestsTo("Ben").empty());
    EXPECT_EQ(store.request("Ann", "Ben"), RequestResult::Sent); // Asking again is allowed
}

TEST(FriendStore, RemovingWorksFromEitherSide)
{
    const TemporaryFile file;
    FriendStore store = loadOrFail(file.path());
    (void)store.request("Ann", "Ben");
    (void)store.accept("Ann", "Ben");

    EXPECT_TRUE(store.removeFriend("Ben", "Ann"));
    EXPECT_FALSE(store.removeFriend("Ben", "Ann"));

    EXPECT_TRUE(store.friendsOf("Ann").empty());
    EXPECT_TRUE(store.friendsOf("Ben").empty());
}

TEST(FriendStore, NoMoreThan50Friends)
{
    const TemporaryFile file;
    FriendStore store = loadOrFail(file.path());
    for (std::size_t i = 0; i < FriendStore::MaxFriends; ++i) {
        ASSERT_EQ(store.request("Ann", nameOf(i)), RequestResult::Sent);
        ASSERT_EQ(store.accept("Ann", nameOf(i)), AcceptResult::Accepted);
    }

    EXPECT_EQ(store.request("Ann", "Late"), RequestResult::LimitReached);
    EXPECT_EQ(store.request("Late", "Ann"), RequestResult::LimitReached);
    EXPECT_EQ(store.friendsOf("Ann").size(), FriendStore::MaxFriends);
}

TEST(FriendStore, AcceptingIsRefusedOnceFull)
{
    const TemporaryFile file;
    FriendStore store = loadOrFail(file.path());
    ASSERT_EQ(store.request("Late", "Ann"), RequestResult::Sent);
    for (std::size_t i = 0; i < FriendStore::MaxFriends; ++i) {
        (void)store.request("Ann", nameOf(i));
        (void)store.accept("Ann", nameOf(i));
    }

    EXPECT_EQ(store.accept("Late", "Ann"), AcceptResult::LimitReached);

    EXPECT_TRUE(store.requestsTo("Ann").contains("Late")); // Still there: a friend may leave
}

TEST(FriendStore, NoMoreThan50RequestsSentOrReceived)
{
    const TemporaryFile file;
    FriendStore store = loadOrFail(file.path());
    for (std::size_t i = 0; i < FriendStore::MaxRequests; ++i) {
        ASSERT_EQ(store.request("Ann", nameOf(i)), RequestResult::Sent);
        ASSERT_EQ(store.request(nameOf(i), "Ben"), RequestResult::Sent);
    }

    EXPECT_EQ(store.request("Ann", "Late"), RequestResult::LimitReached);
    EXPECT_EQ(store.request("Late", "Ben"), RequestResult::LimitReached);
}

TEST(FriendStore, EverythingSurvivesARestartEvenNamesWithSpaces)
{
    const TemporaryFile file;
    {
        FriendStore store = loadOrFail(file.path());
        (void)store.request("Ann Lee", "Ben");
        (void)store.accept("Ann Lee", "Ben");
        (void)store.request("Cid", "Ann Lee");
        ASSERT_TRUE(store.save());
    }

    const FriendStore store = loadOrFail(file.path());

    EXPECT_EQ(store.damagedLines(), 0U);
    EXPECT_TRUE(store.friendsOf("Ben").contains("Ann Lee"));
    EXPECT_TRUE(store.friendsOf("Ann Lee").contains("Ben"));
    EXPECT_TRUE(store.requestsTo("Ann Lee").contains("Cid"));
    EXPECT_TRUE(store.requestsFrom("Cid").contains("Ann Lee"));
}

TEST(FriendStore, ChangesWaitForSave)
{
    const TemporaryFile file;
    FriendStore store = loadOrFail(file.path());
    EXPECT_FALSE(store.hasChanges());
    const std::string before = contentOf(file.path());

    (void)store.request("Ann", "Ben");

    EXPECT_TRUE(store.hasChanges());
    EXPECT_EQ(contentOf(file.path()), before);

    ASSERT_TRUE(store.save());
    EXPECT_FALSE(store.hasChanges());
    EXPECT_NE(contentOf(file.path()), before);
}

TEST(FriendStore, ARefusedRequestChangesNothing)
{
    const TemporaryFile file;
    FriendStore store = loadOrFail(file.path());

    (void)store.request("Ann", "Ann");
    (void)store.accept("Ann", "Ben");
    (void)store.dropRequest("Ann", "Ben");
    (void)store.removeFriend("Ann", "Ben");

    EXPECT_FALSE(store.hasChanges());
}

TEST(FriendStore, ADamagedLineIsSkippedAndKept)
{
    const TemporaryFile file;
    const std::string content
        = "F\tAnn\tBen\nnot a line\nF\tAnn\nX\tAnn\tCid\nF\tAnn\tAnn\nR\tCid\tAnn\n";
    write(file.path(), content);

    const FriendStore store = loadOrFail(file.path());

    EXPECT_EQ(store.damagedLines(), 4U);
    EXPECT_TRUE(store.friendsOf("Ann").contains("Ben"));
    EXPECT_TRUE(store.requestsTo("Ann").contains("Cid"));
    std::filesystem::path copy = file.path();
    copy += ".damaged";
    EXPECT_EQ(contentOf(copy), content);
}

TEST(FriendStore, AFileCannotHoldMoreThanTheLimits)
{
    const TemporaryFile file;
    std::string content;
    for (std::size_t i = 0; i <= FriendStore::MaxFriends; ++i) {
        content += "F\tAnn\t" + nameOf(i) + "\n";
    }
    write(file.path(), content);

    const FriendStore store = loadOrFail(file.path());

    EXPECT_EQ(store.friendsOf("Ann").size(), FriendStore::MaxFriends);
    EXPECT_EQ(store.damagedLines(), 1U);
}

TEST(FriendStore, AFolderThatCannotBeWrittenFailsAtLoad)
{
    EXPECT_FALSE(FriendStore::load("/nonexistent-folder/friends.txt").has_value());
}

TEST(FriendSystem, ALeavingPlayerIsOfflineFromTheFrameTheyLeave)
{
    // Not one frame later: a new login of the same account can come in the next frame, and
    // must count as an arrival, or it would never receive its list
    using namespace network::protocol;
    const TemporaryFile accountsFile;
    const TemporaryFile friendsFile;
    auto accounts = server::AccountStore::load(accountsFile.path(), testing_players::FastCost);
    ASSERT_TRUE(accounts.has_value());
    FriendStore friends = loadOrFail(friendsFile.path());
    (void)friends.request("Player0", "Player1");
    (void)friends.accept("Player0", "Player1");

    ecs::World world;
    const ecs::Entity leaving = testing_players::addPlayer(world, 0);
    const ecs::Entity staying = testing_players::addPlayer(world, 1);
    server::FriendSystem system(*accounts, friends);
    system.update(world, ecs::Time{});
    auto& channel = world.getComponent<server::NetClient>(staying).channel;
    const auto now = network::ReliableChannel::Clock::time_point{};
    (void)channel.takeDue(now); // The list, and "Player0 is online"

    world.addComponent(leaving, server::Disconnecting{});
    system.update(world, ecs::Time{});

    const auto due = channel.takeDue(now + network::ReliableChannel::MinResendDelay);
    const bool told = std::ranges::any_of(due, [](const Packet& packet) {
        const auto* entry = std::get_if<FriendEntry>(&packet.payload);
        return entry != nullptr && entry->state == FriendState::Offline;
    });
    EXPECT_TRUE(told);
}
