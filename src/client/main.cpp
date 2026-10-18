// r-type_client: shows the game and sends the player's keys to the server.

#include "client/Assets.hpp"
#include "client/ClientOptions.hpp"
#include "client/ClientSystems.hpp"
#include "client/Components/Session.hpp"
#include "client/Network/ServerConnection.hpp"
#include "client/Singleton.hpp"
#include "client/Systems/StarfieldSystem.hpp"
#include "ecs/Loop/GameLoop.hpp"
#include "ecs/World/World.hpp"

#include <SFML/Graphics/RenderWindow.hpp>

#include <exception>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int ExitFailure = 84;

int run(const char* programPath, std::span<const std::string_view> arguments)
{
    const auto options = client::parseArguments(arguments);
    if (!options) {
        std::cerr << "r-type_client: " << options.error() << "\n\n" << client::usage();
        return ExitFailure;
    }
    if (options->help) {
        std::cout << client::usage();
        return 0;
    }

    client::Assets assets;
    if (const auto loaded = client::Assets::load(assets, client::findAssetsFolder(programPath));
        !loaded) {
        std::cerr << "r-type_client: " << loaded.error() << '\n';
        return ExitFailure;
    }
    auto connection = client::ServerConnection::open();
    if (!connection) {
        std::cerr << "r-type_client: cannot open a UDP socket: " << connection.error().message()
                  << '\n';
        return ExitFailure;
    }

    sf::RenderWindow window(sf::VideoMode({1280, 720}), "R-Type");
    window.setFramerateLimit(60); // Display rate = simulation rate: no CPU wasted

    ecs::World world;
    ecs::GameLoop loop(world); // Client settings: 60 Hz steps, no sleeping
    client::Session& session = client::singleton<client::Session>(world);
    session.name = options->name;
    session.address = options->host;
    session.port = std::to_string(options->port);
    client::spawnStarfield(world);
    client::installSystems(world, loop, window, *connection, assets);

    loop.run(); // Until the window is closed or Quit is chosen

    connection->disconnect(client::ServerConnection::Clock::now());
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::span<char*> raw(argv, static_cast<std::size_t>(argc));
        const std::vector<std::string_view> arguments(raw.begin() + 1, raw.end());
        return run(raw.front(), arguments);
    } catch (const std::exception& error) {
        std::cerr << "r-type_client: fatal error: " << error.what() << '\n';
        return ExitFailure;
    }
}
