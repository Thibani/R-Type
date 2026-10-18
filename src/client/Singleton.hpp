#pragma once

#include "ecs/Component/Component.hpp"
#include "ecs/World/World.hpp"

namespace client {

/// The one `T` of the world, created on first use: state shared by several
/// systems (the session, the input history...) lives on its own entity, like
/// any other data, instead of in a system or a global. Same rule as the server.
template <ecs::Component T>
T& singleton(ecs::World& world)
{
    T* found = nullptr;
    world.query<T>([&](ecs::Entity /*entity*/, T& component) { found = &component; });
    if (found != nullptr) {
        return *found;
    }
    return world.addComponent(world.createEntity(), T{});
}

} // namespace client
