#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ecs {

/// The ordered list of stages a frame goes through.
///
/// A stage is a named step of the frame ("Input", "Movement", "Collision",
/// "Render"...). Every system belongs to one stage. Each frame, the stages
/// run in order and, inside a stage, systems run in the order they were added.
///
/// The ECS defines only one stage, `DefaultStage` ("Update"). The game
/// defines its own pipeline around it:
///
///     auto& scheduler = world.systems().scheduler();
///     scheduler.addStageBefore("Input", ecs::Scheduler::DefaultStage);
///     scheduler.addStageAfter("Collision", ecs::Scheduler::DefaultStage);
///     // Input -> Update -> Collision
///
/// The order is configured in one place instead of being hidden in the order
/// of `add()` calls, and a system can be added at any time to its stage.
class Scheduler {
public:
    /// The stage every system goes to unless told otherwise. Always present.
    static constexpr std::string_view DefaultStage = "Update";

    /// Creates a scheduler with the default stage only.
    Scheduler();

    /// Appends a stage at the end of the frame.
    /// Throws `std::invalid_argument` if the name is empty or already used.
    void addStage(std::string name);

    /// Inserts a stage just before `anchor`.
    /// Throws `std::invalid_argument` if the name is empty or already used,
    /// or if `anchor` does not exist.
    void addStageBefore(std::string name, std::string_view anchor);

    /// Inserts a stage just after `anchor`. Same errors as `addStageBefore`.
    void addStageAfter(std::string name, std::string_view anchor);

    [[nodiscard]]
    bool hasStage(std::string_view name) const noexcept;

    /// The stages, in execution order.
    [[nodiscard]]
    std::span<const std::string> stages() const noexcept;

private:
    using Stages = std::vector<std::string>;

    void insertStage(Stages::const_iterator position, std::string name);

    [[nodiscard]]
    Stages::const_iterator findStage(std::string_view name) const noexcept;

    [[nodiscard]]
    Stages::const_iterator requireStage(std::string_view anchor) const;

    Stages _stages;
};

} // namespace ecs
