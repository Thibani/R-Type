#pragma once

#include "ecs/Entity/Entity.hpp"
#include "ecs/System/System.hpp"
#include "network/Crypto/Crypto.hpp"
#include "network/Protocol/Protocol.hpp"
#include "network/Reliability/ReliableChannel.hpp"
#include "server/AccountStore.hpp"

#include <chrono>
#include <cstdint>
#include <deque>
#include <future>
#include <optional>
#include <string>

namespace server {

/// Lets a connected client into the game once it has logged in to its account
/// (docs/protocol.md §8.1). Runs in `frameStart()`, after `SessionReceiveSystem`.
///
/// Hashing a password is deliberately slow, so it runs on another thread, one
/// at a time: the game never waits for it. The thread only receives copies and
/// returns a hash; the accounts and the sessions stay on the game thread.
///
/// One attempt per connection: after a failure, the client must connect again.
/// A session still not logged in after `LoginTimeout` is closed, so that a
/// stranger cannot keep a player slot.
class LoginSystem final : public ecs::System {
public:
    static constexpr std::chrono::seconds LoginTimeout{10};

    explicit LoginSystem(AccountStore& accounts) noexcept;

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    /// An AUTH_REQUEST waiting for its hash.
    struct Attempt {
        ecs::Entity player;
        std::uint32_t token{}; // Tells whether the session is still the same one
        network::protocol::AuthMode mode{network::protocol::AuthMode::Login};
        std::string name;
        std::string password; // Cleared as soon as hashing starts
        std::optional<Account> account; // To compare with. None: unknown name
    };

    void takeRequests(ecs::World& world);
    void finishRunningAttempt(ecs::World& world);
    void startNextAttempt(ecs::World& world);
    void closeLateSessions(ecs::World& world, const ecs::Time& time);

    AccountStore& _accounts;
    std::deque<Attempt> _waiting; // At most one per session
    std::optional<Attempt> _running;
    std::future<network::crypto::PasswordHash> _hash; // Result of `_running`
    network::crypto::PasswordSalt _salt{}; // Salt and cost used for `_running`
    network::crypto::PasswordCost _cost{};
};

} // namespace server
