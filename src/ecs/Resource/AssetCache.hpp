#pragma once

#include "ecs/Resource/IAssetCache.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace ecs {

/// Builds an asset of type `T` from a file path. Returns nullptr when
/// loading fails.
template <typename T>
using AssetLoader = std::function<std::unique_ptr<T>(const std::string& path)>;

namespace detail {

/// The loader and the loaded assets of one type, kept by `AssetManager`.
template <typename T>
class AssetCache final : public IAssetCache {
public:
    [[nodiscard]]
    std::size_t size() const noexcept override
    {
        return assets.size();
    }

    void clear() noexcept override
    {
        assets.clear();
    }

    AssetLoader<T> loader;
    std::unordered_map<std::string, std::shared_ptr<T>> assets;
};

} // namespace detail

} // namespace ecs
