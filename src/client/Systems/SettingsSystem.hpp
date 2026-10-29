#pragma once

#include "ecs/System/System.hpp"

#include <filesystem>
#include <optional>

namespace client {

/// Saves the `Settings` to their file when they changed (`Settings::dirty`).
/// Runs in `frameEnd()`: the input code only changes values, and the file is
/// written at most once per frame however many keys were pressed.
///
/// Without a known path (`settingsPath()`), settings work but are not saved.
class SettingsSystem final : public ecs::System {
public:
    explicit SettingsSystem(std::optional<std::filesystem::path> path);

    void update(ecs::World& world, const ecs::Time& time) override;

private:
    std::optional<std::filesystem::path> _path;
};

} // namespace client
