#include "server/ScoreStore.hpp"

#include "server/AccountStore.hpp"
#include "server/TextFile.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace server {

namespace {

constexpr std::string_view Header
    = "# r-type scores: S score name, M score name name... (separated by tabs)";

/// Solo for one name, Multi for 2 to 4; none for another number.
std::optional<network::protocol::Board> boardFor(std::size_t names) noexcept
{
    if (names == 1) {
        return network::protocol::Board::Solo;
    }
    if (names >= 2 && names <= network::protocol::MaxPlayers) {
        return network::protocol::Board::Multi;
    }
    return std::nullopt;
}

} // namespace

ScoreStore::ScoreStore(std::filesystem::path file)
    : _file(std::move(file))
{
}

std::expected<ScoreStore, std::string> ScoreStore::load(const std::filesystem::path& file)
{
    ScoreStore store(file);

    std::error_code error;
    if (!std::filesystem::exists(file, error)) {
        if (error || !store.save()) { // Tested now, not at the end of the first game
            return std::unexpected("cannot create the scores file " + file.string());
        }
        return store;
    }

    std::ifstream input(file, std::ios::binary);
    if (!input) {
        return std::unexpected("cannot read the scores file " + file.string());
    }
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && !line.starts_with('#') && !store.readLine(line)) {
            ++store._damagedLines;
        }
    }
    store._changed = false; // Read, not changed
    if (store._damagedLines > 0 && !keepDamagedCopy(file)) {
        return std::unexpected("cannot save a copy of the damaged file " + file.string());
    }
    return store;
}

bool ScoreStore::readLine(std::string_view line)
{
    if (line.size() < 2 || (line[0] != 'S' && line[0] != 'M') || line[1] != '\t') {
        return false;
    }
    const Board board = line[0] == 'S' ? Board::Solo : Board::Multi;
    line.remove_prefix(2);

    std::vector<std::string_view> fields;
    for (std::size_t tab = line.find('\t'); tab != std::string_view::npos; tab = line.find('\t')) {
        fields.push_back(line.substr(0, tab));
        line.remove_prefix(tab + 1);
    }
    fields.push_back(line);

    std::uint32_t score = 0;
    const std::string_view number = fields.front();
    const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), score);
    if (error != std::errc{} || end != number.data() + number.size()) {
        return false;
    }
    std::vector<std::string> names(fields.begin() + 1, fields.end());
    return boardFor(names.size()) == board && add(score, names);
}

bool ScoreStore::add(std::uint32_t score, const std::vector<std::string>& names)
{
    const auto board = boardFor(names.size());
    if (!board || !std::ranges::all_of(names, AccountStore::isValidName)) {
        return false;
    }
    Ranking& games = ranking(*board);
    const std::size_t index = games.games.size();
    games.games.push_back(Game{.score = score, .names = names});
    // After every game with the same score or more: on a tie, the older game ranks first
    const auto place = std::ranges::partition_point(
        games.sorted, [&](std::size_t other) { return games.games[other].score >= score; });
    games.sorted.insert(place, index);
    for (const std::string& name : names) {
        games.last.insert_or_assign(name, index);
    }
    _changed = true;
    return true;
}

std::vector<ScoreStore::Game> ScoreStore::best(Board board, std::size_t count) const
{
    const Ranking& games = ranking(board);
    std::vector<Game> best;
    for (std::size_t i = 0; i < games.sorted.size() && i < count; ++i) {
        best.push_back(games.games[games.sorted[i]]);
    }
    return best;
}

std::optional<ScoreStore::Ranked> ScoreStore::lastGameOf(Board board, std::string_view name) const
{
    const Ranking& games = ranking(board);
    const auto found = games.last.find(name);
    if (found == games.last.end()) {
        return std::nullopt;
    }
    const std::size_t index = found->second;
    const std::uint32_t score = games.games[index].score;
    // Games before it: a better score, or the same score played earlier
    const auto place = std::ranges::partition_point(games.sorted, [&](std::size_t other) {
        const std::uint32_t otherScore = games.games[other].score;
        return otherScore > score || (otherScore == score && other < index);
    });
    const auto before = static_cast<std::uint32_t>(place - games.sorted.begin());
    return Ranked{.rank = before + 1, .score = score};
}

const ScoreStore::Ranking& ScoreStore::ranking(Board board) const noexcept
{
    return _rankings[board == Board::Solo ? 0 : 1];
}

ScoreStore::Ranking& ScoreStore::ranking(Board board) noexcept
{
    return _rankings[board == Board::Solo ? 0 : 1];
}

bool ScoreStore::hasChanges() const noexcept
{
    return _changed;
}

bool ScoreStore::save()
{
    std::ostringstream content;
    content << Header << '\n';
    for (const Board board : {Board::Solo, Board::Multi}) {
        for (const Game& game : ranking(board).games) {
            content << (board == Board::Solo ? 'S' : 'M') << '\t' << game.score;
            for (const std::string& name : game.names) {
                content << '\t' << name;
            }
            content << '\n';
        }
    }
    if (!replaceFile(_file, content.str())) {
        return false;
    }
    _changed = false;
    return true;
}

std::size_t ScoreStore::damagedLines() const noexcept
{
    return _damagedLines;
}

} // namespace server
