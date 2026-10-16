#include "ecs/Resource/Resources.hpp"

namespace ecs {

std::size_t Resources::size() const noexcept
{
    return _resources.size();
}

void Resources::clear() noexcept
{
    _resources.clear();
}

} // namespace ecs
