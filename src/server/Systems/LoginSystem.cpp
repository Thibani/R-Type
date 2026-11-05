#include "server/Systems/LoginSystem.hpp"

#include "ecs/World/World.hpp"
#include "network/Protocol/Packets.hpp"
#include "server/Components/ClientMessage.hpp"
#include "server/Components/NetClient.hpp"
#include "server/Network.hpp"

#include <exception>
#include <iostream>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace server {

using namespace network::protocol;
using namespace network::crypto;

namespace {

/// The session this attempt came from, if it is still open.
NetClient* sessionOf(ecs::World& world, ecs::Entity player, std::uint32_t token)
{
    auto* client = world.tryGetComponent<NetClient>(player);
    if (client == nullptr || client->token != token || world.hasComponent<Disconnecting>(player)) {
        return nullptr;
    }
    return client;
}

bool isLoggedIn(ecs::World& world, const std::string& name)
{
    bool found = false;
    world.query<NetClient, LoggedIn>([&](ecs::Entity /*entity*/, NetClient& client, LoggedIn&) {
        found = found || client.name == name;
    });
    return found;
}

/// Lets the player in: AUTH_RESULT comes first, then everyone learns about
/// everyone. From the next system on, this player is part of the game.
void admit(ecs::World& world, ecs::Entity player)
{
    NetClient& client = world.getComponent<NetClient>(player);
    client.channel.queueReliable(AuthResult{.code = AuthCode::Ok});

    const PlayerJoined newcomer{.playerId = client.playerId, .name = client.name};
    world.query<NetClient, LoggedIn>([&](ecs::Entity /*entity*/, NetClient& other, LoggedIn&) {
        other.channel.queueReliable(newcomer);
        client.channel.queueReliable(PlayerJoined{.playerId = other.playerId, .name = other.name});
    });
    world.addComponent(player, LoggedIn{});

    std::cout << "r-type_server: player " << static_cast<int>(client.playerId) << " '"
              << client.name << "' joined from " << client.endpoint << '\n';
}

} // namespace

LoginSystem::LoginSystem(AccountStore& accounts) noexcept
    : _accounts(accounts)
{
}

void LoginSystem::update(ecs::World& world, const ecs::Time& time)
{
    takeRequests(world);
    finishRunningAttempt(world);
    startNextAttempt(world);
    closeLateSessions(world, time);
}

void LoginSystem::takeRequests(ecs::World& world)
{
    std::vector<ecs::Entity> messages;
    world.query<ClientMessage>([&](ecs::Entity entity, ClientMessage& message) {
        if (std::holds_alternative<AuthRequest>(message.payload)) {
            messages.push_back(entity);
        }
    });

    for (const ecs::Entity entity : messages) {
        const ClientMessage message = world.getComponent<ClientMessage>(entity);
        world.destroyEntity(entity);

        auto* client = world.tryGetComponent<NetClient>(message.client);
        if (client == nullptr || client->loginRequested) {
            continue; // A repeated request: its answer is on its way, reliably
        }
        client->loginRequested = true;

        const auto& request = std::get<AuthRequest>(message.payload);
        const auto refuse
            = [&](AuthCode code) { client->channel.queueReliable(AuthResult{.code = code}); };
        if (request.mode == AuthMode::Register) {
            if (!AccountStore::isValidName(client->name)) {
                refuse(AuthCode::InvalidName);
                continue;
            }
            if (request.password.size() < MinPasswordSize) {
                refuse(AuthCode::InvalidPassword);
                continue;
            }
        }
        _waiting.push_back(Attempt{
            .player = message.client,
            .token = client->token,
            .mode = request.mode,
            .name = client->name,
            .password = request.password,
            .account = std::nullopt,
        });
    }
}

void LoginSystem::finishRunningAttempt(ecs::World& world)
{
    if (!_running || _hash.wait_for(std::chrono::seconds::zero()) != std::future_status::ready) {
        return;
    }
    const Attempt attempt = *std::exchange(_running, std::nullopt);

    std::optional<PasswordHash> hash;
    try {
        hash = _hash.get();
    } catch (const std::exception& error) { // Not enough memory for this hash
        std::cerr << "r-type_server: cannot hash a password: " << error.what() << '\n';
    }

    NetClient* client = sessionOf(world, attempt.player, attempt.token);
    if (client == nullptr) {
        return; // The player left in the meantime
    }

    if (!hash) {
        client->channel.queueReliable(AuthResult{.code = AuthCode::ServerError});
    } else if (attempt.mode == AuthMode::Register) {
        // The name was free when hashing started, and attempts run one at a time
        if (_accounts.add(attempt.name, Account{.salt = _salt, .cost = _cost, .hash = *hash})) {
            admit(world, attempt.player);
        } else {
            client->channel.queueReliable(AuthResult{.code = AuthCode::ServerError});
        }
    } else if (!attempt.account || !samePasswordHash(*hash, attempt.account->hash)) {
        client->channel.queueReliable(AuthResult{.code = AuthCode::WrongCredentials});
    } else if (isLoggedIn(world, attempt.name)) {
        // Checked last: only someone who knows the password learns this
        client->channel.queueReliable(AuthResult{.code = AuthCode::AlreadyConnected});
    } else {
        admit(world, attempt.player);
    }
}

void LoginSystem::startNextAttempt(ecs::World& world)
{
    while (!_running && !_waiting.empty()) {
        Attempt attempt = std::move(_waiting.front());
        _waiting.pop_front();

        NetClient* client = sessionOf(world, attempt.player, attempt.token);
        if (client == nullptr) {
            continue; // Gone while waiting: no need to hash
        }

        const Account* account = _accounts.find(attempt.name);
        if (attempt.mode == AuthMode::Register && account != nullptr) {
            client->channel.queueReliable(AuthResult{.code = AuthCode::NameTaken});
            continue;
        }
        if (attempt.mode == AuthMode::Login && account != nullptr) {
            attempt.account = *account;
            _salt = account->salt;
            _cost = account->cost;
        } else {
            // An unknown name is hashed too: answering sooner would reveal
            // which names have an account
            _salt = generatePasswordSalt();
            _cost = _accounts.cost();
        }

        // Copies only: the thread touches nothing the game thread uses
        try {
            _hash = std::async(std::launch::async,
                [password = std::exchange(attempt.password, {}), salt = _salt, cost = _cost] {
                    return hashPassword(password, salt, cost);
                });
            _running = std::move(attempt);
        } catch (const std::system_error& error) { // The OS refused a new thread
            std::cerr << "r-type_server: cannot start hashing: " << error.what() << '\n';
            client->channel.queueReliable(AuthResult{.code = AuthCode::ServerError});
        }
    }
}

void LoginSystem::closeLateSessions(ecs::World& world, const ecs::Time& time)
{
    const TimePoint now = toTimePoint(time);
    std::vector<ecs::Entity> late;
    world.query<NetClient>([&](ecs::Entity entity, NetClient& client) {
        if (!world.hasComponent<LoggedIn>(entity) && now - client.connectedAt >= LoginTimeout) {
            late.push_back(entity);
        }
    });
    for (const ecs::Entity entity : late) {
        if (!world.hasComponent<Disconnecting>(entity)) {
            world.addComponent(entity,
                Disconnecting{.reason = LeaveReason::Timeout, .notify = DisconnectReason::Timeout});
        }
    }
}

} // namespace server
