// What a line typed in the chat means, and how a message is cut to fit its box.

#include "client/Chat.hpp"
#include "client/Components/Session.hpp"
#include "client/Replication.hpp"
#include "client/Singleton.hpp"
#include "ecs/World/World.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <variant>
#include <vector>

using client::ChatLine;
using client::ChatOrder;
using client::FriendRow;
using client::readChat;
using client::wrapChatText;
using namespace network::protocol;

namespace {

const std::vector<FriendRow> Friends{{.name = "Bernard", .state = FriendState::Online},
    {.name = "Ann", .state = FriendState::Offline},
    {.name = "Ann Lee", .state = FriendState::Offline},
    {.name = "Asks", .state = FriendState::RequestReceived},
    {.name = "Sent", .state = FriendState::RequestSent}};

ChatOrder read(std::string_view input, bool inGame = true)
{
    return readChat(input, std::nullopt, Friends, inGame);
}

void expectToFriend(const ChatOrder& order, std::string_view name, std::string_view text)
{
    const auto* letter = std::get_if<ChatOrder::ToFriend>(&order.action);
    ASSERT_NE(letter, nullptr);
    EXPECT_EQ(letter->name, name);
    EXPECT_EQ(letter->text, text);
}

void expectProblem(const ChatOrder& order, std::string_view text)
{
    const auto* problem = std::get_if<ChatOrder::Problem>(&order.action);
    ASSERT_NE(problem, nullptr);
    EXPECT_EQ(problem->text, text);
}

/// Every character is 1 wide, whatever its bytes: a line holds `width` of them.
auto widthOf(std::size_t width)
{
    return [width](std::string_view text) {
        std::size_t characters = 0;
        for (const char c : text) {
            characters += (static_cast<unsigned char>(c) & 0xC0U) != 0x80U ? 1 : 0;
        }
        return characters <= width;
    };
}

} // namespace

TEST(ChatLine, PlainTextGoesToTheGame)
{
    const ChatOrder order = read("  hello there ");
    const auto* game = std::get_if<ChatOrder::ToGame>(&order.action);

    ASSERT_NE(game, nullptr);
    EXPECT_EQ(game->text, "hello there");
}

TEST(ChatLine, NothingTypedDoesNothing)
{
    EXPECT_TRUE(std::holds_alternative<ChatOrder::Nothing>(read("").action));
    EXPECT_TRUE(std::holds_alternative<ChatOrder::Nothing>(read("    ").action));
}

TEST(ChatLine, OutsideAGameThereIsNobodyToTalkTo)
{
    expectProblem(read("hello", false), "Not in a group yet.");
}

TEST(ChatLine, MsgFindsTheFriendAndWorksOutsideAGame)
{
    expectToFriend(read("/msg Bernard Salut"), "Bernard", "Salut");
    expectToFriend(read("/msg Bernard Salut", false), "Bernard", "Salut");
}

TEST(ChatLine, MsgIgnoresUpperAndLowerCaseButSendsTheExactName)
{
    expectToFriend(read("/msg bernard Salut"), "Bernard", "Salut");
    expectToFriend(read("/MSG BERNARD Salut"), "Bernard", "Salut");
}

TEST(ChatLine, MsgTakesTheLongestName)
{
    expectToFriend(read("/msg Ann Lee Salut"), "Ann Lee", "Salut");
    expectToFriend(read("/msg Ann Salut"), "Ann", "Salut");
}

TEST(ChatLine, MsgWithoutAFriendIsRefused)
{
    expectProblem(read("/msg Paul Salut"), "Not in your friends.");
    expectProblem(read("/msg Asks Salut"), "Not in your friends."); // A request is no friend
    expectProblem(read("/msg Sent Salut"), "Not in your friends.");
    expectProblem(read("/msg Bernardo Salut"), "Not in your friends.");
}

TEST(ChatLine, MsgWithoutAMessageShowsTheUsage)
{
    expectProblem(read("/msg Bernard"), "Usage: /msg name message");
    expectProblem(read("/msg Bernard    "), "Usage: /msg name message");
}

TEST(ChatLine, AWordStartingLikeTheCommandIsNotTheCommand)
{
    EXPECT_TRUE(std::holds_alternative<ChatOrder::ToGame>(read("/msgs are fun").action));
}

TEST(ChatLine, TheMenuChoosesTheFriend)
{
    expectToFriend(
        readChat("/msg is text here", "Ann", Friends, false), "Ann", "/msg is text here");
}

TEST(ChatLine, TooLongIsSaidNotCut)
{
    expectProblem(
        read(std::string(MaxChatSize + 1, 'x')), "Message too long: 200 characters at most.");
    expectProblem(read("/msg Bernard " + std::string(MaxChatSize + 1, 'x')),
        "Message too long: 200 characters at most.");
    EXPECT_TRUE(
        std::holds_alternative<ChatOrder::ToGame>(read(std::string(MaxChatSize, 'x')).action));
}

TEST(ChatWrap, ShortTextStaysOnOneLine)
{
    EXPECT_EQ(wrapChatText("a short one", widthOf(40)), (std::vector<std::string>{"a short one"}));
}

TEST(ChatWrap, CutsBetweenWords)
{
    EXPECT_EQ(
        wrapChatText("one two three", widthOf(7)), (std::vector<std::string>{"one two", "three"}));
}

TEST(ChatWrap, CutsAWordTooLongForALine)
{
    EXPECT_EQ(
        wrapChatText("abcdefghij", widthOf(4)), (std::vector<std::string>{"abcd", "efgh", "ij"}));
}

TEST(ChatWrap, NeverCutsAUtf8Character)
{
    EXPECT_EQ(wrapChatText("ééééé", widthOf(2)), (std::vector<std::string>{"éé", "éé", "é"}));
}

TEST(ChatWrap, NeverCutsAUtf8CharacterEvenIfItsFirstByteWouldFit)
{
    const auto fiveBytes = [](std::string_view text) { return text.size() <= 5; };

    EXPECT_EQ(wrapChatText("ééééé", fiveBytes), (std::vector<std::string>{"éé", "éé", "é"}));
}

TEST(ChatWrap, ALineThatFitsNothingStillMovesForward)
{
    EXPECT_EQ(wrapChatText("ab", widthOf(0)), (std::vector<std::string>{"a", "b"}));
}

TEST(ChatHistory, KeepsTheNewestLines)
{
    client::Session session;
    for (std::size_t i = 0; i < client::MaxChatLines + 5; ++i) {
        client::addChatLine(session,
            ChatLine{.kind = ChatLine::Kind::Notice, .name = {}, .text = std::to_string(i)});
    }

    EXPECT_EQ(session.chat.size(), client::MaxChatLines);
    EXPECT_EQ(session.chat.front().text, "5");
    EXPECT_EQ(session.chat.back().text, std::to_string(client::MaxChatLines + 4));
}

TEST(ChatHistory, ReadingOlderLinesStaysInPlaceWhenOneArrives)
{
    client::Session session;
    client::addChatLine(session, ChatLine{.kind = ChatLine::Kind::Notice, .name = {}, .text = "a"});
    client::addChatLine(session, ChatLine{.kind = ChatLine::Kind::Notice, .name = {}, .text = "b"});
    session.chatScroll = 1;

    client::addChatLine(session, ChatLine{.kind = ChatLine::Kind::Notice, .name = {}, .text = "c"});

    EXPECT_EQ(session.chatScroll, 2U);
}

TEST(ChatHistory, MessagesCountOnTheMainPage)
{
    ecs::World world;
    client::startSession(world);

    client::applyPayload(
        world, ChatMessage{.kind = ChatKind::Received, .name = "Ben", .text = "Salut"}, 0.0F);

    const auto& session = client::singleton<client::Session>(world);
    ASSERT_EQ(session.chat.size(), 1U);
    EXPECT_EQ(session.chat.at(0).kind, ChatLine::Kind::Received);
    EXPECT_EQ(session.chat.at(0).name, "Ben");
    EXPECT_EQ(session.chat.at(0).text, "Salut");
}

TEST(ChatHistory, AMessageDoesNotEndTheCatchingUp)
{
    ecs::World world;
    auto& session = client::singleton<client::Session>(world);
    session.name = "B";
    client::enterGame(world, 1);

    client::applyPayload(
        world, ChatMessage{.kind = ChatKind::Game, .name = "A", .text = "hi"}, 0.1F);
    client::applyPayload(world, PlayerJoined{.playerId = 0, .name = "A"}, 0.1F);

    EXPECT_TRUE(session.notice.empty());
}

TEST(ChatHistory, ARefusalIsSaidInTheChat)
{
    ecs::World world;
    client::startSession(world);

    client::applyPayload(world, ChatRefused{.reason = ChatRefusal::NotFriend}, 0.0F);

    const auto& session = client::singleton<client::Session>(world);
    ASSERT_EQ(session.chat.size(), 1U);
    EXPECT_EQ(session.chat.at(0).kind, ChatLine::Kind::Notice);
    EXPECT_EQ(session.chat.at(0).text, "Not in your friends.");
}

TEST(ChatHistory, HistoryLivesThroughAGameButNotALogOut)
{
    ecs::World world;
    auto& session = client::singleton<client::Session>(world);
    client::enterGame(world, 0);
    client::applyPayload(
        world, ChatMessage{.kind = ChatKind::Game, .name = "A", .text = "hi"}, 0.0F);
    session.chatOpen = true;
    session.chatInput = "typing";

    client::leaveGame(world);
    EXPECT_EQ(session.chat.size(), 1U);
    EXPECT_FALSE(session.chatOpen); // But the typing line does not come back in the next game
    EXPECT_TRUE(session.chatInput.empty());

    client::clearSession(world);
    EXPECT_TRUE(session.chat.empty());
}
