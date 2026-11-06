// The private messages waiting for a player, without any network.

#include "TemporaryFile.hpp"
#include "server/MessageStore.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

using server::MessageStore;
using server::StoredMessage;
using testing_players::TemporaryFile;

namespace {

MessageStore loadOrFail(const std::filesystem::path& file)
{
    auto store = MessageStore::load(file);
    EXPECT_TRUE(store.has_value()) << (store ? "" : store.error());
    return std::move(*store);
}

std::string contentOf(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

StoredMessage from(const std::string& name, const std::string& text)
{
    return {.from = name, .text = text};
}

} // namespace

TEST(MessageStore, MessagesComeOutOldestFirstAndOnlyOnce)
{
    const TemporaryFile file;
    MessageStore store = loadOrFail(file.path());
    ASSERT_TRUE(store.add("Ben", from("Ann", "first")));
    ASSERT_TRUE(store.add("Ben", from("Cid", "second")));

    const auto messages = store.take("Ben");

    ASSERT_EQ(messages.size(), 2U);
    EXPECT_EQ(messages.at(0).from, "Ann");
    EXPECT_EQ(messages.at(0).text, "first");
    EXPECT_EQ(messages.at(1).text, "second");
    EXPECT_TRUE(store.take("Ben").empty());
}

TEST(MessageStore, EachPlayerHasTheirOwn)
{
    const TemporaryFile file;
    MessageStore store = loadOrFail(file.path());
    ASSERT_TRUE(store.add("Ben", from("Ann", "for Ben")));
    ASSERT_TRUE(store.add("Cid", from("Ann", "for Cid")));

    const auto messages = store.take("Ben");

    ASSERT_EQ(messages.size(), 1U);
    EXPECT_EQ(messages.at(0).text, "for Ben");
    EXPECT_EQ(store.take("Cid").size(), 1U);
}

TEST(MessageStore, NoMoreThan100PerPlayer)
{
    const TemporaryFile file;
    MessageStore store = loadOrFail(file.path());
    for (std::size_t i = 0; i < MessageStore::MaxMessages; ++i) {
        ASSERT_TRUE(store.add("Ben", from("Ann", std::to_string(i))));
    }

    EXPECT_FALSE(store.add("Ben", from("Ann", "one too many")));
    EXPECT_TRUE(store.add("Cid", from("Ann", "someone else")));
    EXPECT_EQ(store.take("Ben").size(), MessageStore::MaxMessages);
    EXPECT_TRUE(store.add("Ben", from("Ann", "room again")));
}

TEST(MessageStore, MessagesSurviveARestartWithSpacesAndAccents)
{
    const TemporaryFile file;
    {
        MessageStore store = loadOrFail(file.path());
        ASSERT_TRUE(store.add("Ben Lee", from("Ann Lee", "ça va, à plus tard ?")));
        ASSERT_TRUE(store.save());
    }

    MessageStore store = loadOrFail(file.path());

    EXPECT_EQ(store.damagedLines(), 0U);
    EXPECT_FALSE(store.hasChanges());
    const auto messages = store.take("Ben Lee");
    ASSERT_EQ(messages.size(), 1U);
    EXPECT_EQ(messages.at(0).from, "Ann Lee");
    EXPECT_EQ(messages.at(0).text, "ça va, à plus tard ?");
}

TEST(MessageStore, AReadMessageLeavesTheFile)
{
    const TemporaryFile file;
    MessageStore store = loadOrFail(file.path());
    ASSERT_TRUE(store.add("Ben", from("Ann", "secret plans")));
    ASSERT_TRUE(store.save());
    ASSERT_NE(contentOf(file.path()).find("secret plans"), std::string::npos);

    (void)store.take("Ben");
    EXPECT_TRUE(store.hasChanges());
    ASSERT_TRUE(store.save());

    EXPECT_EQ(contentOf(file.path()).find("secret plans"), std::string::npos);
}

TEST(MessageStore, ADamagedLineIsSkippedAndKept)
{
    const TemporaryFile file;
    const std::string content
        = "Ben\tAnn\thello\nno tabs here\nBen\tAnn\n\tAnn\ttext\nBen\tAnn\t\nBen\tAnn\tbye\n";
    std::ofstream(file.path(), std::ios::binary) << content;

    MessageStore store = loadOrFail(file.path());

    EXPECT_EQ(store.damagedLines(), 4U);
    EXPECT_EQ(store.take("Ben").size(), 2U);
    std::filesystem::path copy = file.path();
    copy += ".damaged";
    EXPECT_EQ(contentOf(copy), content);
}

TEST(MessageStore, AFileCannotHoldMoreThanTheLimit)
{
    const TemporaryFile file;
    std::string content;
    for (std::size_t i = 0; i <= MessageStore::MaxMessages; ++i) {
        content += "Ben\tAnn\t" + std::to_string(i) + "\n";
    }
    std::ofstream(file.path(), std::ios::binary) << content;

    MessageStore store = loadOrFail(file.path());

    EXPECT_EQ(store.damagedLines(), 1U);
    EXPECT_EQ(store.take("Ben").size(), MessageStore::MaxMessages);
}

TEST(MessageStore, AFolderThatCannotBeWrittenFailsAtLoad)
{
    EXPECT_FALSE(MessageStore::load("/nonexistent-folder/messages.txt").has_value());
}
