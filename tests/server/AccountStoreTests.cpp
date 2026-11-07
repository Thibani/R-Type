// The accounts file, without any network: what is written, what survives a
// restart, and what happens when the file is damaged.

#include "TemporaryFile.hpp"
#include "server/AccountStore.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

using network::crypto::generatePasswordSalt;
using network::crypto::hashPassword;
using server::Account;
using server::AccountStore;
using testing_players::FastCost;
using testing_players::TemporaryFile;

namespace {

Account accountWith(const std::string& password)
{
    const auto salt = generatePasswordSalt();
    return {.salt = salt, .cost = FastCost, .hash = hashPassword(password, salt, FastCost)};
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

} // namespace

TEST(AccountStore, MissingFileIsCreatedEmpty)
{
    const TemporaryFile file;

    const auto store = AccountStore::load(file.path(), FastCost);

    ASSERT_TRUE(store.has_value());
    EXPECT_EQ(store->size(), 0U);
    EXPECT_TRUE(std::filesystem::exists(file.path()));
}

TEST(AccountStore, AccountsSurviveARestart)
{
    const TemporaryFile file;
    const Account dave = accountWith("first password");
    const Account zoe = accountWith("second password");
    {
        auto store = AccountStore::load(file.path(), FastCost);
        ASSERT_TRUE(store.has_value());
        ASSERT_TRUE(store->add("Dave", dave));
        ASSERT_TRUE(store->add("Zoë the 2nd", zoe)); // Not ASCII, and with spaces
    }

    const auto reloaded = AccountStore::load(file.path(), FastCost);

    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(reloaded->size(), 2U);
    EXPECT_EQ(reloaded->damagedLines(), 0U);
    const Account* found = reloaded->find("Zoë the 2nd");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->salt, zoe.salt);
    EXPECT_EQ(found->cost, zoe.cost);
    EXPECT_EQ(found->hash, zoe.hash);
    EXPECT_NE(reloaded->find("Dave"), nullptr);
    EXPECT_EQ(reloaded->find("Nobody"), nullptr);
}

TEST(AccountStore, NameIsTakenOnlyOnce)
{
    const TemporaryFile file;
    auto store = AccountStore::load(file.path(), FastCost);
    ASSERT_TRUE(store.has_value());
    const Account first = accountWith("first password");

    ASSERT_TRUE(store->add("Dave", first));
    EXPECT_FALSE(store->add("Dave", accountWith("second password")));

    EXPECT_EQ(store->find("Dave")->hash, first.hash);
}

TEST(AccountStore, NameThatWouldBreakTheFileIsRefused)
{
    const TemporaryFile file;
    auto store = AccountStore::load(file.path(), FastCost);
    ASSERT_TRUE(store.has_value());
    const Account account = accountWith("some password");

    EXPECT_FALSE(store->add("", account));
    EXPECT_FALSE(store->add("Da\nve", account));
    EXPECT_FALSE(store->add("Da\tve", account));
    EXPECT_FALSE(store->add("a name of more than 16 bytes", account));
    EXPECT_EQ(store->size(), 0U);
}

TEST(AccountStore, PasswordIsNotInTheFile)
{
    const TemporaryFile file;
    auto store = AccountStore::load(file.path(), FastCost);
    ASSERT_TRUE(store.has_value());

    ASSERT_TRUE(store->add("Dave", accountWith("hunter2hunter2")));

    const std::string content = contentOf(file.path());
    EXPECT_NE(content.find("Dave"), std::string::npos);
    EXPECT_EQ(content.find("hunter2"), std::string::npos);
}

TEST(AccountStore, SameNameAndPasswordGiveAnotherHash)
{
    EXPECT_NE(accountWith("same password").hash, accountWith("same password").hash); // Salted
}

TEST(AccountStore, DamagedLinesAreSkippedAndCopied)
{
    const TemporaryFile file;
    {
        auto store = AccountStore::load(file.path(), FastCost);
        ASSERT_TRUE(store.has_value());
        ASSERT_TRUE(store->add("Dave", accountWith("some password")));
    }
    const std::string good = contentOf(file.path());
    const std::string damaged = good
        + "not an account\n"
          "8 1 zz 00 Bad hex\n"
          "99999999 1 00000000000000000000000000000000 "
          "0000000000000000000000000000000000000000000000000000000000000000 Huge\n"
          "\xFF\xFE\n";
    write(file.path(), damaged);

    const auto store = AccountStore::load(file.path(), FastCost);

    ASSERT_TRUE(store.has_value());
    EXPECT_EQ(store->size(), 1U);
    EXPECT_EQ(store->damagedLines(), 4U);
    EXPECT_NE(store->find("Dave"), nullptr);
    std::filesystem::path copy = file.path();
    copy += ".damaged";
    EXPECT_EQ(contentOf(copy), damaged);

    // Still damaged at the next start: the server must start again, with a new copy
    const std::string worse = damaged + "another bad line\n";
    write(file.path(), worse);
    EXPECT_TRUE(AccountStore::load(file.path(), FastCost).has_value());
    EXPECT_EQ(contentOf(copy), worse);
}

TEST(AccountStore, FileTruncatedAnywhereDoesNotCrash)
{
    const TemporaryFile file;
    {
        auto store = AccountStore::load(file.path(), FastCost);
        ASSERT_TRUE(store.has_value());
        ASSERT_TRUE(store->add("Dave", accountWith("some password")));
    }
    const std::string content = contentOf(file.path());

    for (std::size_t size = 0; size < content.size(); ++size) {
        write(file.path(), content.substr(0, size));
        EXPECT_TRUE(AccountStore::load(file.path(), FastCost).has_value()) << size;
    }
}

TEST(AccountStore, UnusablePathFailsAtLoading)
{
    const TemporaryFile folder;
    std::filesystem::create_directory(folder.path());
    const std::filesystem::path inMissingFolder = folder.path() / "missing" / "accounts.txt";

    EXPECT_FALSE(AccountStore::load(inMissingFolder, FastCost).has_value());
    EXPECT_FALSE(std::filesystem::exists(inMissingFolder));
}

TEST(AccountStore, FailedSaveDoesNotCreateTheAccount)
{
    const TemporaryFile folder;
    std::filesystem::create_directory(folder.path());
    const std::filesystem::path path = folder.path() / "accounts.txt";
    auto store = AccountStore::load(path, FastCost);
    ASSERT_TRUE(store.has_value());
    // The temporary file cannot be created where a folder already has its name
    std::filesystem::path blocker = path;
    blocker += ".tmp";
    std::filesystem::create_directory(blocker);

    EXPECT_FALSE(store->add("Dave", accountWith("some password")));

    EXPECT_EQ(store->find("Dave"), nullptr);
    std::filesystem::remove(blocker);
    std::filesystem::remove(path);
}
