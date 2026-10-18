#include "server/ServerOptions.hpp"

#include <charconv>
#include <memory>
#include <system_error>
#include <utility>

namespace server {

namespace {

std::expected<std::uint16_t, std::string> parsePort(std::string_view text)
{
    const char* const first = std::to_address(text.begin());
    const char* const last = std::to_address(text.end());
    int port{};
    const auto [end, error] = std::from_chars(first, last, port);
    if (error != std::errc{} || end != last || port == 0 || !std::in_range<std::uint16_t>(port)) {
        return std::unexpected("invalid port '" + std::string(text) + "' (expected 1-65535)");
    }
    return static_cast<std::uint16_t>(port);
}

} // namespace

std::expected<ServerOptions, std::string> parseArguments(
    std::span<const std::string_view> arguments)
{
    ServerOptions options;

    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string_view argument = arguments[i];

        if (argument == "-h" || argument == "--help") {
            options.help = true;
        } else if (argument == "-p" || argument == "--port") {
            if (i + 1 == arguments.size()) {
                return std::unexpected(std::string(argument) + " needs a port number");
            }
            const auto port = parsePort(arguments[++i]);
            if (!port) {
                return std::unexpected(port.error());
            }
            options.port = *port;
        } else {
            return std::unexpected("unknown argument '" + std::string(argument) + "'");
        }
    }
    return options;
}

std::string_view usage() noexcept
{
    return "USAGE: r-type_server [-p port]\n"
           "  -p, --port PORT  UDP port to listen on (default 4242)\n"
           "  -h, --help       print this help\n";
}

} // namespace server
