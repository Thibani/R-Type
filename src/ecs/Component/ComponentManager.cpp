#include "ecs/Component/ComponentManager.hpp"

namespace ecs {

void ComponentManager::removeAll(Entity entity)
{
    for (auto& [typeId, storage] : _storages) {
        storage->remove(entity);
    }
}

void ComponentManager::clear() noexcept
{
    for (auto& [typeId, storage] : _storages) {
        storage->clear();
    }
}

} // namespace ecs
