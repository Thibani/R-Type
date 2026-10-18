#include "client/ClientOptions.hpp"

#include "network/Protocol/PacketCodec.hpp"

#include <charconv>
#include <memory>
#include <system_error>
#include <utility>

namespace client {

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

std::expected<ClientOptions, std::string> parseArguments(
    std::span<const std::string_view> arguments)
{
    ClientOptions options;
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string_view argument = arguments[i];
        if (argument == "-h" || argument == "--help") {
            options.help = true;
            continue;
        }
        if (argument != "-s" && argument != "--server" && argument != "-p" && argument != "--port"
            && argument != "-n" && argument != "--name") {
            return std::unexpected("unknown argument '" + std::string(argument) + "'");
        }
        if (i + 1 == arguments.size()) {
            return std::unexpected(std::string(argument) + " needs a value");
        }
        const std::string_view value = arguments[++i];
        if (argument == "-s" || argument == "--server") {
            options.host = value;
        } else if (argument == "-p" || argument == "--port") {
            const auto port = parsePort(value);
            if (!port) {
                return std::unexpected(port.error());
            }
            options.port = *port;
        } else {
            if (value.empty() || !network::protocol::isValidName(value)) {
                return std::unexpected(
                    "invalid name '" + std::string(value) + "' (1 to 16 bytes of UTF-8)");
            }
            options.name = value;
        }
    }
    return options;
}

std::string_view usage() noexcept
{
    return "USAGE: r-type_client [-s address] [-p port] [-n name]\n"
           "  -s, --server ADDRESS  server IPv4 address or host name (default 127.0.0.1)\n"
           "  -p, --port PORT       server UDP port (default 4242)\n"
           "  -n, --name NAME       player name, 1 to 16 bytes (default Player)\n"
           "  -h, --help            print this help\n";
}

} // namespace client
