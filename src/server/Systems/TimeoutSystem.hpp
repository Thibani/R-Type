#pragma once

#include "ecs/System/System.hpp"

#include <chrono>

namespace server {

/// Closes the sessions of clients that went silent: crashed, unplugged, or
/// frozen. Runs in `frameStart()`, after `SessionReceiveSystem`.
///
/// A session times out when nothing was received for 5 seconds (§8.2), or
/// when a reliable message stayed unacknowledged for 5 seconds (§6.2).
class TimeoutSystem final : public ecs::System {
public:
    static constexpr std::chrono::seconds SessionTimeout{5};

    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
