#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace server {

/// Command line of `r-type_server`.
struct ServerOptions {
    std::uint16_t port{4242};
    bool help{false}; // -h / --help: print the usage and exit
};

/// Parses the arguments that follow the program name.
/// Returns a message explaining the problem if they are invalid.
[[nodiscard]]
std::expected<ServerOptions, std::string> parseArguments(
    std::span<const std::string_view> arguments);

/// Text printed by `-h` and after an invalid command line.
[[nodiscard]]
std::string_view usage() noexcept;

} // namespace server
