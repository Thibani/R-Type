// r-type_server: the authoritative game server.

#include "ecs/Loop/GameLoop.hpp"
#include "ecs/World/World.hpp"
#include "network/Transport/NetworkThread.hpp"
#include "server/AccountStore.hpp"
#include "server/FriendStore.hpp"
#include "server/MessageStore.hpp"
#include "server/Network.hpp"
#include "server/ServerOptions.hpp"
#include "server/ServerSystems.hpp"

#include <csignal>
#include <exception>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

constexpr int ExitFailure = 84;

// The signal handler can only reach the loop through a global
ecs::GameLoop* runningLoop = nullptr; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

extern "C" void onStopSignal(int /*signal*/)
{
    if (runningLoop != nullptr) {
        runningLoop->stop(); // A lock-free atomic store: allowed in a signal handler
    }
}

int run(std::span<const std::string_view> arguments)
{
    const auto options = server::parseArguments(arguments);
    if (!options) {
        std::cerr << "r-type_server: " << options.error() << "\n\n" << server::usage();
        return ExitFailure;
    }
    if (options->help) {
        std::cout << server::usage();
        return 0;
    }

    auto accounts = server::AccountStore::load(options->accountsFile);
    if (!accounts) {
        std::cerr << "r-type_server: " << accounts.error() << '\n';
        return ExitFailure;
    }
    if (accounts->damagedLines() > 0) {
        std::cerr << "r-type_server: warning: " << accounts->damagedLines()
                  << " damaged line(s) in the accounts file, copied to "
                  << options->accountsFile.string() << ".damaged\n";
    }

    auto friends = server::FriendStore::load(options->friendsFile);
    if (!friends) {
        std::cerr << "r-type_server: " << friends.error() << '\n';
        return ExitFailure;
    }
    if (friends->damagedLines() > 0) {
        std::cerr << "r-type_server: warning: " << friends->damagedLines()
                  << " damaged line(s) in the friends file, copied to "
                  << options->friendsFile.string() << ".damaged\n";
    }

    auto messages = server::MessageStore::load(options->messagesFile);
    if (!messages) {
        std::cerr << "r-type_server: " << messages.error() << '\n';
        return ExitFailure;
    }
    if (messages->damagedLines() > 0) {
        std::cerr << "r-type_server: warning: " << messages->damagedLines()
                  << " damaged line(s) in the messages file, copied to "
                  << options->messagesFile.string() << ".damaged\n";
    }

    // The network thread starts here; the game runs on this one
    auto socket = network::NetworkThread::open(options->port);
    if (!socket) {
        std::cerr << "r-type_server: cannot listen on UDP port " << options->port << ": "
                  << socket.error().message() << '\n';
        return ExitFailure;
    }

    ecs::World world;
    ecs::GameLoop loop(world, {.sleepUntilNextStep = true});
    server::installSystems(world, loop, **socket, *accounts, *friends, *messages);

    runningLoop = &loop;
    std::signal(SIGINT, onStopSignal);
    std::signal(SIGTERM, onStopSignal);

    std::cout << "r-type_server: listening on UDP port " << options->port << " (Ctrl+C to stop)\n"
              << std::flush; // Visible at once even when piped
    loop.run();

    runningLoop = nullptr;
    if (!friends->save()) { // The last second of changes (FriendSystem)
        std::cerr << "r-type_server: cannot save the friends file\n";
    }
    if (!messages->save()) { // Same for the messages (ChatSystem)
        std::cerr << "r-type_server: cannot save the messages file\n";
    }
    // The channels only use time for round-trip statistics: any recent value works
    server::notifyShutdown(world, **socket, server::toTimePoint(loop.simulationTime()));
    socket->reset(); // Sends the DISCONNECTs still queued, then joins the network thread
    std::cout << "r-type_server: stopped" << '\n';
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    try {
        const std::span<char*> raw(argv, static_cast<std::size_t>(argc));
        const std::vector<std::string_view> arguments(raw.begin() + 1, raw.end());
        return run(arguments);
    } catch (const std::exception& error) {
        std::cerr << "r-type_server: fatal error: " << error.what() << '\n';
        return ExitFailure;
    }
}
