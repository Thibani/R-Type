// The finished games and the two rankings, without any network.

#include "TemporaryFile.hpp"
#include "server/ScoreStore.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

using network::protocol::Board;
using server::ScoreStore;
using testing_players::TemporaryFile;

namespace {

ScoreStore loadOrFail(const std::filesystem::path& file)
{
    auto store = ScoreStore::load(file);
    EXPECT_TRUE(store.has_value()) << (store ? "" : store.error());
    return std::move(*store);
}

std::string contentOf(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

std::vector<std::uint32_t> scoresOf(const std::vector<ScoreStore::Game>& games)
{
    std::vector<std::uint32_t> scores;
    for (const ScoreStore::Game& game : games) {
        scores.push_back(game.score);
    }
    return scores;
}

} // namespace

TEST(ScoreStore, OneNameIsSoloAndSeveralAreMulti)
{
    const TemporaryFile file;
    ScoreStore store = loadOrFail(file.path());

    ASSERT_TRUE(store.add(300, {"Ann"}));
    ASSERT_TRUE(store.add(900, {"Ann", "Ben", "Cid"}));

    const auto solo = store.best(Board::Solo, 10);
    const auto multi = store.best(Board::Multi, 10);
    ASSERT_EQ(solo.size(), 1U);
    EXPECT_EQ(solo[0].score, 300U);
    ASSERT_EQ(multi.size(), 1U);
    EXPECT_EQ(multi[0].names, (std::vector<std::string>{"Ann", "Ben", "Cid"}));
}

TEST(ScoreStore, NoGameWithoutPlayersOrWithTooMany)
{
    const TemporaryFile file;
    ScoreStore store = loadOrFail(file.path());

    EXPECT_FALSE(store.add(100, {}));
    EXPECT_FALSE(store.add(100, {"A", "B", "C", "D", "E"}));
    EXPECT_FALSE(store.add(100, {""}));
    EXPECT_TRUE(store.best(Board::Solo, 10).empty());
    EXPECT_TRUE(store.best(Board::Multi, 10).empty());
    EXPECT_FALSE(store.hasChanges());
}

TEST(ScoreStore, TheBestComeFirstAndOnlyTenAreShown)
{
    const TemporaryFile file;
    ScoreStore store = loadOrFail(file.path());
    for (std::uint32_t score = 100; score <= 1200; score += 100) { // 12 games
        ASSERT_TRUE(store.add(score, {"Ann"}));
    }

    const auto best = store.best(Board::Solo, 10);

    EXPECT_EQ(scoresOf(best),
        (std::vector<std::uint32_t>{1200, 1100, 1000, 900, 800, 700, 600, 500, 400, 300}));
}

TEST(ScoreStore, AGameIsALineSoAPlayerCanBeThereTwice)
{
    const TemporaryFile file;
    ScoreStore store = loadOrFail(file.path());
    ASSERT_TRUE(store.add(500, {"Ann", "Ben"}));
    ASSERT_TRUE(store.add(700, {"Ann", "Ben"}));

    EXPECT_EQ(scoresOf(store.best(Board::Multi, 10)), (std::vector<std::uint32_t>{700, 500}));
}

TEST(ScoreStore, OnATieTheOlderGameRanksFirst)
{
    const TemporaryFile file;
    ScoreStore store = loadOrFail(file.path());
    ASSERT_TRUE(store.add(400, {"Ann"}));
    ASSERT_TRUE(store.add(400, {"Ben"}));
    ASSERT_TRUE(store.add(400, {"Cid"}));

    const auto best = store.best(Board::Solo, 10);

    ASSERT_EQ(best.size(), 3U);
    EXPECT_EQ(best[0].names.front(), "Ann");
    EXPECT_EQ(best[1].names.front(), "Ben");
    EXPECT_EQ(best[2].names.front(), "Cid");
    EXPECT_EQ(store.lastGameOf(Board::Solo, "Ben")->rank, 2U);
}

TEST(ScoreStore, TheLastGameIsRankedEvenFarBelowTheTen)
{
    const TemporaryFile file;
    ScoreStore store = loadOrFail(file.path());
    for (std::uint32_t i = 0; i < 33; ++i) {
        ASSERT_TRUE(store.add(1000 + i, {"Other"}));
    }
    ASSERT_TRUE(store.add(5, {"Ann"}));

    const auto last = store.lastGameOf(Board::Solo, "Ann");

    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->rank, 34U);
    EXPECT_EQ(last->score, 5U);
}

TEST(ScoreStore, OnlyTheLastGameCountsNotTheBest)
{
    const TemporaryFile file;
    ScoreStore store = loadOrFail(file.path());
    ASSERT_TRUE(store.add(900, {"Ann"}));
    ASSERT_TRUE(store.add(100, {"Ann"}));

    const auto last = store.lastGameOf(Board::Solo, "Ann");

    ASSERT_TRUE(last.has_value());
    EXPECT_EQ(last->score, 100U);
    EXPECT_EQ(last->rank, 2U);
}

TEST(ScoreStore, TheRankOfTheLastGameMovesWhenOthersPlay)
{
    const TemporaryFile file;
    ScoreStore store = loadOrFail(file.path());
    ASSERT_TRUE(store.add(500, {"Ann"}));
    ASSERT_EQ(store.lastGameOf(Board::Solo, "Ann")->rank, 1U);

    ASSERT_TRUE(store.add(800, {"Ben"}));

    EXPECT_EQ(store.lastGameOf(Board::Solo, "Ann")->rank, 2U);
}

TEST(ScoreStore, EachRankingHasItsOwnLastGame)
{
    const TemporaryFile file;
    ScoreStore store = loadOrFail(file.path());
    ASSERT_TRUE(store.add(300, {"Ann"}));
    ASSERT_TRUE(store.add(600, {"Ben", "Ann"}));

    EXPECT_EQ(store.lastGameOf(Board::Solo, "Ann")->score, 300U);
    EXPECT_EQ(store.lastGameOf(Board::Multi, "Ann")->score, 600U);
    EXPECT_FALSE(store.lastGameOf(Board::Solo, "Ben").has_value());
    EXPECT_FALSE(store.lastGameOf(Board::Multi, "Nobody").has_value());
}

TEST(ScoreStore, EverythingSurvivesARestartEvenNamesWithSpaces)
{
    const TemporaryFile file;
    {
        ScoreStore store = loadOrFail(file.path());
        ASSERT_TRUE(store.add(250, {"Ann Lee"}));
        ASSERT_TRUE(store.add(250, {"Ben"}));
        ASSERT_TRUE(store.add(800, {"Ann Lee", "Ben", "Cid", "Dom"}));
        ASSERT_TRUE(store.save());
    }

    const ScoreStore store = loadOrFail(file.path());

    const auto solo = store.best(Board::Solo, 10);
    ASSERT_EQ(solo.size(), 2U);
    EXPECT_EQ(solo[0].names.front(), "Ann Lee"); // Still first on the tie: the order is kept
    EXPECT_EQ(store.best(Board::Multi, 10).at(0).names.size(), 4U);
    EXPECT_EQ(store.lastGameOf(Board::Multi, "Dom")->rank, 1U);
    EXPECT_FALSE(store.hasChanges());
}

TEST(ScoreStore, ChangesWaitForSave)
{
    const TemporaryFile file;
    ScoreStore store = loadOrFail(file.path());
    const std::string empty = contentOf(file.path());

    ASSERT_TRUE(store.add(100, {"Ann"}));

    EXPECT_TRUE(store.hasChanges());
    EXPECT_EQ(contentOf(file.path()), empty);
    ASSERT_TRUE(store.save());
    EXPECT_FALSE(store.hasChanges());
    EXPECT_NE(contentOf(file.path()), empty);
}

TEST(ScoreStore, ADamagedLineIsSkippedAndKept)
{
    const TemporaryFile file;
    const std::string content = "S\t100\tAnn\n"
                                "S\tlots\tAnn\n" // Not a number
                                "S\t100\tAnn\tBen\n" // Solo with two names
                                "M\t100\tAnn\n" // Multi with one name
                                "X\t100\tAnn\n" // Unknown kind
                                "S\t-5\tAnn\n" // Negative
                                "S\t99999999999\tAnn\n" // Too big for 32 bits
                                "M\t200\tAnn\tBen\n";
    std::ofstream(file.path(), std::ios::binary) << content;

    const ScoreStore store = loadOrFail(file.path());

    EXPECT_EQ(store.damagedLines(), 6U);
    EXPECT_EQ(store.best(Board::Solo, 10).size(), 1U);
    EXPECT_EQ(store.best(Board::Multi, 10).size(), 1U);
    std::filesystem::path copy = file.path();
    copy += ".damaged";
    EXPECT_EQ(contentOf(copy), content);
}

TEST(ScoreStore, AFolderThatCannotBeWrittenFailsAtLoad)
{
    EXPECT_FALSE(ScoreStore::load("/nonexistent-folder/scores.txt").has_value());
}
