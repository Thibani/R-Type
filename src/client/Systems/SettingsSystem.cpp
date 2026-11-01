#include "client/Systems/SettingsSystem.hpp"

#include "client/Components/Settings.hpp"
#include "client/Settings.hpp"
#include "client/Singleton.hpp"
#include "ecs/World/World.hpp"

#include <utility>

namespace client {

SettingsSystem::SettingsSystem(std::optional<std::filesystem::path> path)
    : _path(std::move(path))
{
}

void SettingsSystem::update(ecs::World& world, const ecs::Time& /*time*/)
{
    auto& settings = singleton<Settings>(world);
    if (!settings.dirty) {
        return;
    }
    settings.dirty = false;
    if (!_path) {
        settings.saveError = "No settings folder: changes last until you quit.";
        return;
    }
    const auto saved = saveSettings(*_path, settings);
    settings.saveError = saved ? "" : "Not saved: " + saved.error();
}

} // namespace client
