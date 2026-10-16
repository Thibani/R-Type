#pragma once

#include "ecs/Scheduler/Scheduler.hpp"
#include "ecs/System/System.hpp"

#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

namespace ecs {

class World;
class Time;

/// Owns the systems and runs them, stage by stage.
///
/// Every system belongs to a stage of the `Scheduler`. Stages run in the
/// scheduler's order; inside a stage, systems run in the order they were added:
///
///     systems.scheduler().addStageBefore("Input", ecs::Scheduler::DefaultStage);
///
///     systems.add<MovementSystem>();               // stage "Update"
///     systems.addToStage<InputSystem>("Input");    // stage "Input"
///
///     systems.update(world, time); // Input, then Movement
///
/// Each system type can be added only once, which allows looking a system up
/// by its type.
class SystemManager {
public:
    /// Creates a system of type `T` from `arguments` in the default stage.
    /// Returns a reference to the new system.
    /// Throws `std::logic_error` if a `T` was already added.
    template <std::derived_from<System> T, typename... Arguments>
        requires std::constructible_from<T, Arguments...>
    T& add(Arguments&&... arguments)
    {
        return addToStage<T>(Scheduler::DefaultStage, std::forward<Arguments>(arguments)...);
    }

    /// Creates a system of type `T` from `arguments` in the given stage.
    /// Throws `std::logic_error` if a `T` was already added, and
    /// `std::invalid_argument` if the stage does not exist.
    template <std::derived_from<System> T, typename... Arguments>
        requires std::constructible_from<T, Arguments...>
    T& addToStage(std::string_view stage, Arguments&&... arguments)
    {
        if (has<T>()) {
            throw std::logic_error("SystemManager::add: this system type was already added");
        }
        if (!_scheduler.hasStage(stage)) {
            throw std::invalid_argument(
                "SystemManager::add: unknown stage '" + std::string(stage) + "'");
        }

        auto system = std::make_unique<T>(std::forward<Arguments>(arguments)...);
        T& reference = *system;
        _entries.push_back(Entry{
            .type = std::type_index(typeid(T)),
            .stage = std::string(stage),
            .system = std::move(system),
        });
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

    /// Returns the stage of the system of type `T`, or nothing if it was not added.
    template <std::derived_from<System> T>
    [[nodiscard]]
    std::optional<std::string_view> stageOf() const noexcept
    {
        const auto found = findEntry(std::type_index(typeid(T)));
        if (found == _entries.end()) {
            return std::nullopt;
        }
        return found->stage;
    }

    /// The stages and their order.
    [[nodiscard]]
    Scheduler& scheduler() noexcept;

    [[nodiscard]]
    const Scheduler& scheduler() const noexcept;

    /// Runs every system once: stage after stage, in insertion order inside
    /// a stage. Do not add or remove systems from inside a system's update.
    void update(World& world, const Time& time);

    /// Runs the systems of one stage, in insertion order.
    void updateStage(World& world, const Time& time, std::string_view stage);

    /// Number of systems.
    [[nodiscard]]
    std::size_t size() const noexcept;

    /// Removes every system. The stages are kept.
    void clear() noexcept;

private:
    struct Entry {
        std::type_index type;
        std::string stage;
        std::unique_ptr<System> system;
    };

    using Entries = std::vector<Entry>;

    [[nodiscard]]
    Entries::iterator findEntry(std::type_index type) noexcept;

    [[nodiscard]]
    Entries::const_iterator findEntry(std::type_index type) const noexcept;

    Scheduler _scheduler;
    Entries _entries; // In insertion order
};

} // namespace ecs
