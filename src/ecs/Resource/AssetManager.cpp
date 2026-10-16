#include "ecs/Resource/AssetManager.hpp"

namespace ecs {

std::size_t AssetManager::size() const noexcept
{
    std::size_t total = 0;
    for (const auto& [type, typedCache] : _caches) {
        total += typedCache->size();
    }
    return total;
}

void AssetManager::clear() noexcept
{
    for (auto& [type, typedCache] : _caches) {
        typedCache->clear();
    }
}

} // namespace ecs
