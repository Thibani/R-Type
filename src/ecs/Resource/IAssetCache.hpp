#pragma once

#include <cstddef>

/// Implementation details of the ECS: not meant to be used directly.
namespace ecs::detail {

/// Type-erased view of an `AssetCache<T>`, so `AssetManager` can keep the
/// caches of every asset type in a single container, count and clear them
/// without knowing their types.
class IAssetCache {
public:
    virtual ~IAssetCache() = default;

    IAssetCache(const IAssetCache&) = delete;
    IAssetCache& operator=(const IAssetCache&) = delete;
    IAssetCache(IAssetCache&&) = delete;
    IAssetCache& operator=(IAssetCache&&) = delete;

    /// Number of loaded assets of this type.
    [[nodiscard]]
    virtual std::size_t size() const noexcept
        = 0;

    /// Unloads every asset of this type. The loader is kept.
    virtual void clear() noexcept = 0;

protected:
    IAssetCache() = default;
};

} // namespace ecs::detail
