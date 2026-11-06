#pragma once

#include <filesystem>
#include <string_view>

/// The two things the server's text files (accounts, friends) must do safely.
namespace server {

/// Writes `content` to a temporary file, then renames it over `file`: a crash
/// during the write leaves the previous file intact. Only the server's user may
/// read it (no effect on Windows). False if it failed: `file` has not changed.
[[nodiscard]]
bool replaceFile(const std::filesystem::path& file, std::string_view content);

/// Copies `file` to `<file>.damaged`, before a save removes its damaged lines for good.
[[nodiscard]]
bool keepDamagedCopy(const std::filesystem::path& file);

} // namespace server
