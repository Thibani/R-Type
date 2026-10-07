#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

namespace ecs {

/// A lightweight handle that identifies an entity.
///
/// An entity is nothing more than an identifier: it owns no data and no
/// behavior. Data lives in components, behavior lives in systems.
///
/// The handle is made of two parts:
///  - `id`: an index that may be reused after the entity is destroyed;
///  - `generation`: a counter bumped every time that index is reused.
///
/// Comparing both parts lets the ECS detect "stale" handles, i.e. handles
/// that still point to an index whose original entity has been destroyed.
///
/// Note: `valid()` only says whether the handle is non-null. Whether the
/// entity is still alive must be asked to the `EntityManager`.
class Entity {
public:
    using Id = std::uint32_t;
    using Generation = std::uint32_t;

    /// Id carried by the null handle. Never assigned to a real entity.
    static constexpr Id NullId = std::numeric_limits<Id>::max();

    /// Creates the null handle.
    constexpr Entity() noexcept = default;

    constexpr Entity(Id id, Generation generation) noexcept
        : _id(id)
        , _generation(generation)
    {
    }

    [[nodiscard]]
    constexpr Id id() const noexcept
    {
        return _id;
    }

    [[nodiscard]]
    constexpr Generation generation() const noexcept
    {
        return _generation;
    }

    /// Returns false for the null handle, true otherwise.
    [[nodiscard]]
    constexpr bool valid() const noexcept
    {
        return _id != NullId;
    }

    friend constexpr bool operator==(const Entity&, const Entity&) noexcept = default;

private:
    Id _id{NullId};
    Generation _generation{};
};

/// The null entity handle, convenient for "no entity" defaults.
inline constexpr Entity NullEntity{};

} // namespace ecs

/// Allows `Entity` to be used as a key in unordered containers.
template <>
struct std::hash<ecs::Entity> {
    [[nodiscard]]
    std::size_t operator()(const ecs::Entity& entity) const noexcept
    {
        const auto packed = (static_cast<std::uint64_t>(entity.generation()) << 32U) | entity.id();
        return std::hash<std::uint64_t>{}(packed);
    }
};
