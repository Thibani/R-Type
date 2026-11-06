#include "server/AccountStore.hpp"

#include "network/Protocol/PacketCodec.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <fstream>
#include <functional>
#include <optional>
#include <span>
#include <system_error>
#include <utility>

namespace server {

namespace {

constexpr std::string_view Header = "# r-type accounts: kibibytes passes salt hash name";

// A damaged file must not make the server allocate all its memory on a login
constexpr std::uint32_t MinKibibytes = 8;
constexpr std::uint32_t MaxKibibytes = 1024U * 1024U; // 1 GiB
constexpr std::uint32_t MaxPasses = 16;

std::string toHex(std::span<const std::byte> bytes)
{
    static constexpr std::string_view Digits = "0123456789abcdef";
    std::string text;
    text.reserve(bytes.size() * 2);
    for (const std::byte byte : bytes) {
        text += Digits[std::to_integer<std::size_t>(byte >> 4U)];
        text += Digits[std::to_integer<std::size_t>(byte & std::byte{0x0F})];
    }
    return text;
}

std::optional<std::uint8_t> hexDigit(char digit) noexcept
{
    if (digit >= '0' && digit <= '9') {
        return static_cast<std::uint8_t>(digit - '0');
    }
    if (digit >= 'a' && digit <= 'f') {
        return static_cast<std::uint8_t>(digit - 'a' + 10);
    }
    return std::nullopt;
}

/// False unless `text` is exactly the hexadecimal form of `output`.
bool fromHex(std::string_view text, std::span<std::byte> output) noexcept
{
    if (text.size() != output.size() * 2) {
        return false;
    }
    for (std::size_t i = 0; i < output.size(); ++i) {
        const auto high = hexDigit(text[2 * i]);
        const auto low = hexDigit(text[(2 * i) + 1]);
        if (!high || !low) {
            return false;
        }
        output[i] = static_cast<std::byte>((*high << 4U) | *low);
    }
    return true;
}

std::optional<std::uint32_t> parseNumber(std::string_view text) noexcept
{
    std::uint32_t value{};
    const char* const last = text.data() + text.size();
    const auto [end, error] = std::from_chars(text.data(), last, value);
    if (error != std::errc{} || end != last) {
        return std::nullopt;
    }
    return value;
}

/// Removes and returns what comes before the next space. Nullopt if there is none.
std::optional<std::string_view> nextWord(std::string_view& line) noexcept
{
    const std::size_t space = line.find(' ');
    if (space == std::string_view::npos) {
        return std::nullopt;
    }
    const std::string_view word = line.substr(0, space);
    line.remove_prefix(space + 1);
    return word;
}

/// Reads one account. The name comes last: it may contain spaces.
std::optional<std::pair<std::string, Account>> parseLine(std::string_view line)
{
    const auto kibibytesText = nextWord(line);
    const auto passesText = nextWord(line);
    const auto saltText = nextWord(line);
    const auto hashText = nextWord(line);
    if (!hashText) { // Words are read in order: the last one found means all were
        return std::nullopt;
    }
    const auto kibibytes = parseNumber(*kibibytesText);
    const auto passes = parseNumber(*passesText);
    if (!kibibytes || *kibibytes < MinKibibytes || *kibibytes > MaxKibibytes || !passes
        || *passes < 1 || *passes > MaxPasses) {
        return std::nullopt;
    }

    Account account{.salt = {}, .cost = {.kibibytes = *kibibytes, .passes = *passes}, .hash = {}};
    if (!fromHex(*saltText, account.salt) || !fromHex(*hashText, account.hash)
        || !AccountStore::isValidName(line)) {
        return std::nullopt;
    }
    return std::pair{std::string(line), account};
}

} // namespace

AccountStore::AccountStore(std::filesystem::path file, network::crypto::PasswordCost cost)
    : _file(std::move(file))
    , _cost(cost)
{
}

std::expected<AccountStore, std::string> AccountStore::load(
    const std::filesystem::path& file, network::crypto::PasswordCost cost)
{
    AccountStore store(file, cost);

    std::error_code error;
    if (!std::filesystem::exists(file, error)) {
        if (error || !store.save()) { // Tested now, not at the first player's sign-up
            return std::unexpected("cannot create the accounts file " + file.string());
        }
        return store;
    }

    std::ifstream input(file, std::ios::binary);
    if (!input) {
        return std::unexpected("cannot read the accounts file " + file.string());
    }
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line.starts_with('#')) {
            continue;
        }
        auto account = parseLine(line);
        if (!account || !store._accounts.insert(std::move(*account)).second) {
            ++store._damagedLines;
        }
    }

    if (store._damagedLines > 0) {
        // Streams, not copy_file(): with MinGW it refuses to replace an older copy
        input.clear();
        input.seekg(0);
        std::filesystem::path copy = file;
        copy += ".damaged";
        std::ofstream output(copy, std::ios::binary | std::ios::trunc);
        output << input.rdbuf();
        output.close();
        if (!output) {
            return std::unexpected("cannot save a copy of the damaged file " + file.string());
        }
    }
    return store;
}

bool AccountStore::isValidName(std::string_view name) noexcept
{
    const auto isControl = [](char c) {
        const auto byte = static_cast<unsigned char>(c);
        return byte < 0x20U || byte == 0x7FU;
    };
    return !name.empty() && network::protocol::isValidName(name)
        && std::ranges::none_of(name, isControl);
}

const Account* AccountStore::find(const std::string& name) const
{
    const auto found = _accounts.find(name);
    return found != _accounts.end() ? &found->second : nullptr;
}

bool AccountStore::add(const std::string& name, const Account& account)
{
    if (!isValidName(name) || !_accounts.emplace(name, account).second) {
        return false;
    }
    if (!save()) {
        _accounts.erase(name); // Memory and file stay the same
        return false;
    }
    return true;
}

network::crypto::PasswordCost AccountStore::cost() const noexcept
{
    return _cost;
}

std::size_t AccountStore::size() const noexcept
{
    return _accounts.size();
}

std::size_t AccountStore::damagedLines() const noexcept
{
    return _damagedLines;
}

bool AccountStore::save() const
{
    std::filesystem::path temporary = _file;
    temporary += ".tmp";

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << Header << '\n';
        for (const auto& [name, account] : _accounts) {
            output << account.cost.kibibytes << ' ' << account.cost.passes << ' '
                   << toHex(account.salt) << ' ' << toHex(account.hash) << ' ' << name << '\n';
        }
        output.close();
        if (!output) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }

    // Only the server's user may read it (no effect on Windows). Not fatal: the
    // file holds no password, this only makes offline guessing harder to start
    std::error_code error;
    std::filesystem::permissions(
        temporary, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, error);

    std::filesystem::rename(temporary, _file, error);
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

} // namespace server
