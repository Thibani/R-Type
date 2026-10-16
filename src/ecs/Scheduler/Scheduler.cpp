#include "ecs/Scheduler/Scheduler.hpp"

#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace ecs {

Scheduler::Scheduler()
    : _stages{std::string(DefaultStage)}
{
}

void Scheduler::addStage(std::string name)
{
    insertStage(_stages.cend(), std::move(name));
}

void Scheduler::addStageBefore(std::string name, std::string_view anchor)
{
    const auto position = requireStage(anchor);
    insertStage(position, std::move(name));
}

void Scheduler::addStageAfter(std::string name, std::string_view anchor)
{
    const auto position = requireStage(anchor);
    insertStage(std::next(position), std::move(name));
}

bool Scheduler::hasStage(std::string_view name) const noexcept
{
    return findStage(name) != _stages.cend();
}

std::span<const std::string> Scheduler::stages() const noexcept
{
    return _stages;
}

void Scheduler::insertStage(Stages::const_iterator position, std::string name)
{
    if (name.empty()) {
        throw std::invalid_argument("Scheduler: a stage name cannot be empty");
    }
    if (hasStage(name)) {
        throw std::invalid_argument("Scheduler: stage '" + name + "' already exists");
    }
    _stages.insert(position, std::move(name));
}

Scheduler::Stages::const_iterator Scheduler::findStage(std::string_view name) const noexcept
{
    return std::ranges::find(_stages, name);
}

Scheduler::Stages::const_iterator Scheduler::requireStage(std::string_view anchor) const
{
    const auto found = findStage(anchor);
    if (found == _stages.cend()) {
        throw std::invalid_argument("Scheduler: unknown stage '" + std::string(anchor) + "'");
    }
    return found;
}

} // namespace ecs
