#pragma once

#include "ecs/Resource/IResourceHolder.hpp"
#include "ecs/Resource/ResourceHolder.hpp"

#include <concepts>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace ecs {

/// Requirements for a type to be stored as a resource: any movable object.
template <typename T>
concept Resource = std::movable<T>;

/// Data that exists once per World and is shared by systems.
///
/// Components belong to one entity. Some data belongs to nobody in
/// particular: the game configuration, the shared score, the inputs received
/// this frame. Those are resources, at most one value per type:
///
///     struct Score {
///         std::uint32_t points{};
///     };
///
///     world.resources().insert(Score{});
///     world.resources().get<Score>().points += 100; // from any system
///
/// A resource stays at the same address for as long as it is stored, even
/// when it is replaced: references to it remain valid until `remove()`.
class Resources {
public:
    /// Stores `resource`, replacing any existing `T`, and returns it.
    template <Resource T>
    T& insert(T resource)
    {
        if (T* existing = tryGet<T>()) {
            *existing = std::move(resource); // Replaced in place: same address
            return *existing;
        }
        auto holder = std::make_unique<detail::ResourceHolder<T>>(std::move(resource));
        T& reference = holder->value;
        _resources.emplace(std::type_index(typeid(T)), std::move(holder));
        return reference;
    }

    /// Builds a `T` from `arguments`, replacing any existing `T`, and returns it.
    template <Resource T, typename... Arguments>
        requires std::constructible_from<T, Arguments...>
    T& emplace(Arguments&&... arguments)
    {
        return insert(T(std::forward<Arguments>(arguments)...));
    }

    /// Returns the `T`. Throws `std::out_of_range` if there is none.
    template <Resource T>
    [[nodiscard]]
    T& get()
    {
        T* resource = tryGet<T>();
        if (resource == nullptr) {
            throw std::out_of_range("Resources::get: no resource of this type");
        }
        return *resource;
    }

    template <Resource T>
    [[nodiscard]]
    const T& get() const
    {
        const T* resource = tryGet<T>();
        if (resource == nullptr) {
            throw std::out_of_range("Resources::get: no resource of this type");
        }
        return *resource;
    }

    /// Returns the `T`, or nullptr if there is none.
    template <Resource T>
    [[nodiscard]]
    T* tryGet() noexcept
    {
        const auto found = _resources.find(std::type_index(typeid(T)));
        if (found == _resources.end()) {
            return nullptr;
        }
        // Safe: the holder registered under T's type is always a detail::ResourceHolder<T>.
        return &static_cast<detail::ResourceHolder<T>&>(*found->second).value;
    }

    template <Resource T>
    [[nodiscard]]
    const T* tryGet() const noexcept
    {
        const auto found = _resources.find(std::type_index(typeid(T)));
        if (found == _resources.end()) {
            return nullptr;
        }
        return &static_cast<const detail::ResourceHolder<T>&>(*found->second).value;
    }

    template <Resource T>
    [[nodiscard]]
    bool contains() const noexcept
    {
        return _resources.contains(std::type_index(typeid(T)));
    }

    /// Destroys the `T`. Returns false if there was none.
    template <Resource T>
    bool remove()
    {
        return _resources.erase(std::type_index(typeid(T))) > 0;
    }

    /// Number of resources stored.
    [[nodiscard]]
    std::size_t size() const noexcept;

    /// Destroys every resource.
    void clear() noexcept;

private:
    // unique_ptr: each resource lives on the heap, so its address never
    // changes when other resources are added (the map may rehash).
    std::unordered_map<std::type_index, std::unique_ptr<detail::IResourceHolder>> _resources;
};

} // namespace ecs
