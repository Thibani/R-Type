#pragma once

#include "ecs/System/System.hpp"

#include <concepts>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

namespace ecs {

class World;
class Time;

/// Owns the systems and runs them, in the order they were added.
///
///     systems.add<InputSystem>();
///     systems.add<MovementSystem>();
///     systems.add<CollisionSystem>();
///
///     systems.update(world, time); // Input, then Movement, then Collision
///
/// Each system type can be added only once, which allows looking a system up
/// by its type. The execution order is the insertion order; finer control
/// (stages, dependencies) is the job of the Scheduler.
class SystemManager {
public:
    /// Creates a system of type `T` from `arguments` and appends it to the
    /// execution order. Returns a reference to the new system.
    /// Throws `std::logic_error` if a `T` was already added.
    template <std::derived_from<System> T, typename... Arguments>
        requires std::constructible_from<T, Arguments...>
    T& add(Arguments&&... arguments)
    {
        if (has<T>()) {
            throw std::logic_error("SystemManager::add: this system type was already added");
        }

        auto system = std::make_unique<T>(std::forward<Arguments>(arguments)...);
        T& reference = *system;
        _entries.push_back(Entry{.type = std::type_index(typeid(T)), .system = std::move(system)});
        return reference;
    }

    /// Removes the system of type `T`. Returns false if there was none.
    template <std::derived_from<System> T>
    bool remove()
    {
        const auto found = findEntry(std::type_index(typeid(T)));
        if (found == _entries.end()) {
            return false;
        }
        _entries.erase(found);
        return true;
    }

    template <std::derived_from<System> T>
    [[nodiscard]]
    bool has() const noexcept
    {
        return find<T>() != nullptr;
    }

    /// Returns the system of type `T`, or nullptr if it was not added.
    template <std::derived_from<System> T>
    [[nodiscard]]
    T* find() noexcept
    {
        const auto found = findEntry(std::type_index(typeid(T)));
        return found != _entries.end() ? static_cast<T*>(found->system.get()) : nullptr;
    }

    template <std::derived_from<System> T>
    [[nodiscard]]
    const T* find() const noexcept
    {
        const auto found = findEntry(std::type_index(typeid(T)));
        return found != _entries.end() ? static_cast<const T*>(found->system.get()) : nullptr;
    }

    /// Runs every system once, in insertion order.
    /// Do not add or remove systems from inside a system's update.
    void update(World& world, const Time& time);

    /// Number of systems.
    [[nodiscard]]
    std::size_t size() const noexcept;

    /// Removes every system.
    void clear() noexcept;

private:
    struct Entry {
        std::type_index type;
        std::unique_ptr<System> system;
    };

    using Entries = std::vector<Entry>;

    [[nodiscard]]
    Entries::iterator findEntry(std::type_index type) noexcept;

    [[nodiscard]]
    Entries::const_iterator findEntry(std::type_index type) const noexcept;

    Entries _entries; // In execution order
};

} // namespace ecs
