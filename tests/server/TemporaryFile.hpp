#pragma once

#include "network/Crypto/Crypto.hpp"

#include <filesystem>
#include <random>
#include <string>
#include <system_error>

namespace testing_players {

/// The cheapest hash Monocypher allows: a test checks the logic, not the cost.
inline constexpr network::crypto::PasswordCost FastCost{.kibibytes = 8, .passes = 1};

/// A unique path in the temporary folder, removed with what was made from it.
/// The file itself does not exist at first.
class TemporaryFile {
public:
    TemporaryFile()
        : _path(std::filesystem::temp_directory_path()
              / ("rtype-accounts-" + std::to_string(std::random_device{}())))
    {
    }

    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;

    ~TemporaryFile()
    {
        std::error_code ignored;
        for (const char* const suffix : {"", ".tmp", ".damaged"}) {
            std::filesystem::path file = _path;
            file += suffix;
            std::filesystem::remove(file, ignored);
        }
    }

    [[nodiscard]]
    const std::filesystem::path& path() const noexcept
    {
        return _path;
    }

private:
    std::filesystem::path _path;
};

} // namespace testing_players
