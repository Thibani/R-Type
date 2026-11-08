#pragma once

#include "network/Protocol/Protocol.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace server {

/// Every finished game, in memory and in a text file with one line per game, the older
/// first within each ranking:
///
///     S <tab> <score> <tab> <name>                  a game started alone
///     M <tab> <score> <tab> <name> <tab> <name>...  a game started by 2 to 4 players
///
/// Two rankings (`Board`), each sorted by score, the older game first on a tie.
///
/// It knows nothing about the ECS or the network. Only the game thread uses it.
/// Changes only stay in memory until `save()`: `LeaderboardSystem` saves at most once
/// per second, so that a game ending does not make the server wait for the disk.
class ScoreStore {
public:
    using Board = network::protocol::Board;

    struct Game {
        std::uint32_t score{0};
        std::vector<std::string> names; // The accounts that started it

        friend bool operator==(const Game&, const Game&) = default;
    };

    /// A game and its place in its ranking (1 = the best).
    struct Ranked {
        std::uint32_t rank{0};
        std::uint32_t score{0};
    };

    /// Reads `file`, or creates it if it does not exist yet. Fails if it cannot
    /// be read or written: the server must not start and lose scores later.
    /// A damaged line is skipped, and the file is first copied to `<file>.damaged`.
    [[nodiscard]]
    static std::expected<ScoreStore, std::string> load(const std::filesystem::path& file);

    /// Adds a finished game: `Solo` for one name, `Multi` for 2 to 4. False (and nothing
    /// added) for another number of names, or an invalid name.
    bool add(std::uint32_t score, const std::vector<std::string>& names);

    /// The `count` best games of `board`, best first.
    [[nodiscard]]
    std::vector<Game> best(Board board, std::size_t count) const;

    /// The last game `name` played in `board`, and its rank there today. None if they
    /// have not played one.
    [[nodiscard]]
    std::optional<Ranked> lastGameOf(Board board, std::string_view name) const;

    /// True if something changed since the last successful `save()`.
    [[nodiscard]]
    bool hasChanges() const noexcept;

    /// Writes the file, through a temporary one. False if it failed.
    [[nodiscard]]
    bool save();

    /// Lines of the file that could not be read when it was loaded.
    [[nodiscard]]
    std::size_t damagedLines() const noexcept;

private:
    explicit ScoreStore(std::filesystem::path file);

    /// One ranking: its games in the order they were played, and their indices sorted
    /// from the best. `last` gives, for each name, the index of their last game.
    struct Ranking {
        std::vector<Game> games;
        std::vector<std::size_t> sorted;
        std::map<std::string, std::size_t, std::less<>> last;
    };

    /// Adds one line of the file. False if it breaks a rule of the store.
    bool readLine(std::string_view line);
    [[nodiscard]]
    const Ranking& ranking(Board board) const noexcept;
    [[nodiscard]]
    Ranking& ranking(Board board) noexcept;

    std::filesystem::path _file;
    std::array<Ranking, 2> _rankings; // Solo, Multi
    bool _changed{false};
    std::size_t _damagedLines{0};
};

} // namespace server
