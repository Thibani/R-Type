#pragma once

#include "ecs/Resource/IResourceHolder.hpp"

#include <concepts>
#include <utility>

namespace ecs::detail {

/// Owns the one resource of type `T` stored in `Resources`. It lives on the
/// heap, so the resource keeps its address while other resources come and go.
template <std::movable T>
class ResourceHolder final : public IResourceHolder {
public:
    explicit ResourceHolder(T resource)
        : value(std::move(resource))
    {
    }

    T value;
};

} // namespace ecs::detail
