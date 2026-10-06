#pragma once

#include <concepts>
#include <typeindex>
#include <typeinfo>

namespace ecs {

/// Requirements for a type to be used as a component.
///
/// A component is a plain piece of data attached to an entity, for example:
///
///     struct Health {
///         int current{};
///         int maximum{};
///     };
///
/// Any movable object type works: no base class, no registration.
/// References, `const` types and C arrays are rejected at compile time.
///
/// Tags are simply empty components (`struct PlayerTag {};`).
template <typename T>
concept Component = std::movable<T>;

/// Unique identifier of a component type, used to find its storage.
using ComponentTypeId = std::type_index;

/// Returns the identifier of the component type `T`.
/// The id is derived from the type itself: nothing needs to be registered.
template <Component T>
[[nodiscard]]
ComponentTypeId componentTypeId() noexcept
{
    return std::type_index(typeid(T));
}

} // namespace ecs
