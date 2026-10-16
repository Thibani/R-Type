#pragma once

#include <cstdint>

namespace ecs {
class World;
} // namespace ecs

namespace server {

/// A netId for a new networked entity: never `NoEntity` (0xFFFF), never one
/// in use, and taken in a cycle so a freed id comes back only after the other
/// 65 534 (§5.1: not reused within 5 seconds).
///
/// shortcut: the 5 s rule holds as long as fewer than ~13 000 entities are
/// created per second; track destruction times if that ever changes.
[[nodiscard]]
std::uint16_t allocateNetId(ecs::World& world);

} // namespace server
