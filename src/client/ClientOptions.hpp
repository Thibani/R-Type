#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace client {

/// Command line of `r-type_client`. The values fill the connection form,
/// where the player can still change them before connecting.
struct ClientOptions {
    std::string host{"127.0.0.1"};
    std::uint16_t port{4242};
    std::string name{"Player"};
    bool help{false}; // -h / --help: print the usage and exit
};

/// Parses the arguments that follow the program name.
/// Returns a message explaining the problem if they are invalid.
[[nodiscard]]
std::expected<ClientOptions, std::string> parseArguments(
    std::span<const std::string_view> arguments);

/// Parses a port number, 1-65535.
[[nodiscard]]
std::expected<std::uint16_t, std::string> parsePort(std::string_view text);

/// Text printed by `-h` and after an invalid command line.
[[nodiscard]]
std::string_view usage() noexcept;

} // namespace client
