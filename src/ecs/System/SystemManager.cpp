#include "ecs/System/SystemManager.hpp"

#include <algorithm>

namespace ecs {

Scheduler& SystemManager::scheduler() noexcept
{
    return _scheduler;
}

const Scheduler& SystemManager::scheduler() const noexcept
{
    return _scheduler;
}

void SystemManager::update(World& world, const Time& time)
{
    for (const std::string& stage : _scheduler.stages()) {
        updateStage(world, time, stage);
    }
}

void SystemManager::updateStage(World& world, const Time& time, std::string_view stage)
{
    for (const Entry& entry : _entries) {
        if (entry.stage == stage) {
            entry.system->update(world, time);
        }
    }
}

std::size_t SystemManager::size() const noexcept
{
    return _entries.size();
}

void SystemManager::clear() noexcept
{
    _entries.clear();
}

SystemManager::Entries::iterator SystemManager::findEntry(std::type_index type) noexcept
{
    return std::ranges::find(_entries, type, &Entry::type);
}

SystemManager::Entries::const_iterator SystemManager::findEntry(std::type_index type) const noexcept
{
    return std::ranges::find(_entries, type, &Entry::type);
}

} // namespace ecs
