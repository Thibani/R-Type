#pragma once

#include "ecs/System/System.hpp"

#include <cstdint>

namespace server {

/// Turns INPUT messages into each player's `PlayerInput`. Runs in
/// `frameStart()`, after `SessionReceiveSystem`.
///
/// This is where authority starts: a client never sends where its ship is,
/// only which keys are pressed; the server's systems decide the rest.
///
/// Each INPUT repeats the client's last masks (§4.2). Only ticks newer than
/// the last one applied for that player count, and ticks more than 120 ahead
/// of it are ignored (§7.1), so a client cannot jump its input clock.
class InputSystem final : public ecs::System {
public:
    static constexpr std::uint32_t MaxTicksAhead = 120;

    void update(ecs::World& world, const ecs::Time& time) override;
};

} // namespace server
