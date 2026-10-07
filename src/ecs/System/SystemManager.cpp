#include "ecs/System/SystemManager.hpp"

#include <algorithm>

namespace ecs {

void SystemManager::update(World& world, const Time& time)
{
    for (const Entry& entry : _entries) {
        entry.system->update(world, time);
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
