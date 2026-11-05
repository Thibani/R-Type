#pragma once

#include "network/Crypto/Crypto.hpp"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace server {

/// What is kept of an account. The password itself is never stored.
struct Account {
    network::crypto::PasswordSalt salt{};
    network::crypto::PasswordCost cost{};
    network::crypto::PasswordHash hash{};
};

/// The players' accounts, in memory and in a text file with one account per
/// line (docs/technical-study/network.md §10):
///
///     <kibibytes> <passes> <salt in hex> <hash in hex> <name>
///
/// It knows nothing about the ECS or the network. Only the game thread uses it.
class AccountStore {
public:
    /// Reads `file`, or creates it if it does not exist yet. Fails if it cannot
    /// be read or written: the server must not start and lose accounts later.
    /// A damaged line is skipped, and the file is first copied to
    /// `<file>.damaged`, since the next save would remove that line for good.
    [[nodiscard]]
    static std::expected<AccountStore, std::string> load(const std::filesystem::path& file,
        network::crypto::PasswordCost cost = network::crypto::DefaultPasswordCost);

    /// True if an account can have this name: not empty, valid for the protocol,
    /// no control character (one line per account, and names reach the logs).
    [[nodiscard]]
    static bool isValidName(std::string_view name) noexcept;

    /// The account called `name`, or nullptr. The pointer stays valid.
    [[nodiscard]]
    const Account* find(const std::string& name) const;

    /// Adds an account and saves the file. False if the name is invalid or taken,
    /// or if the file cannot be written: the account is then not created.
    [[nodiscard]]
    bool add(const std::string& name, const Account& account);

    /// Cost of the accounts created from now on.
    [[nodiscard]]
    network::crypto::PasswordCost cost() const noexcept;

    [[nodiscard]]
    std::size_t size() const noexcept;

    /// Lines of the file that could not be read when it was loaded.
    [[nodiscard]]
    std::size_t damagedLines() const noexcept;

private:
    AccountStore(std::filesystem::path file, network::crypto::PasswordCost cost);

    /// Writes a temporary file, then renames it over the real one: a crash
    /// during the write leaves the previous file intact.
    [[nodiscard]]
    bool save() const;

    std::filesystem::path _file;
    network::crypto::PasswordCost _cost;
    std::map<std::string, Account, std::less<>> _accounts;
    std::size_t _damagedLines{0};
};

} // namespace server
