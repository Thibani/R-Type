#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace ecs {

/// Loads files (textures, sounds, fonts, maps...) once and shares them.
///
/// The ECS knows nothing about SFML or file formats: the code that owns a
/// backend registers a loader for each asset type, then everyone loads by path:
///
///     assets.setLoader<Texture>([](const std::string& path) {
///         auto texture = std::make_unique<Texture>();
///         if (!texture->loadFromFile(path)) {
///             return std::unique_ptr<Texture>{}; // failure
///         }
///         return texture;
///     });
///
///     auto texture = assets.load<Texture>("assets/textures/player.png");
///     auto same = assets.get<Texture>("assets/textures/player.png"); // no reload
///
/// Assets are cached by (type, path): the file is read only once. They are
/// handed out as `std::shared_ptr`, so unloading an asset from the cache never
/// invalidates an asset someone is still using.
///
/// To share it between systems, store it as a resource:
/// `world.resources().emplace<ecs::AssetManager>()`.
class AssetManager {
public:
    /// Builds an asset from a file path. Returns nullptr when loading fails.
    template <typename T>
    using Loader = std::function<std::unique_ptr<T>(const std::string& path)>;

    AssetManager() = default;
    ~AssetManager() = default;

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;
    AssetManager(AssetManager&&) noexcept = default;
    AssetManager& operator=(AssetManager&&) noexcept = default;

    /// Registers (or replaces) the loader used for assets of type `T`.
    template <typename T>
    void setLoader(Loader<T> loader)
    {
        cache<T>().loader = std::move(loader);
    }

    template <typename T>
    [[nodiscard]]
    bool hasLoader() const noexcept
    {
        const Cache<T>* typedCache = findCache<T>();
        return typedCache != nullptr && static_cast<bool>(typedCache->loader);
    }

    /// Returns the asset at `path`, loading it the first time.
    /// Returns nullptr if the loader fails (missing or invalid file): the
    /// failure is not cached, so a later call tries again.
    /// Throws `std::logic_error` if no loader is registered for `T`.
    template <typename T>
    std::shared_ptr<T> load(const std::string& path)
    {
        Cache<T>& typedCache = cache<T>();
        if (const auto found = typedCache.assets.find(path); found != typedCache.assets.end()) {
            return found->second;
        }
        if (!typedCache.loader) {
            throw std::logic_error("AssetManager::load: no loader registered for this type");
        }

        std::shared_ptr<T> asset = typedCache.loader(path);
        if (asset != nullptr) {
            typedCache.assets.emplace(path, asset);
        }
        return asset;
    }

    /// Returns the asset at `path` if it is already loaded, nullptr otherwise.
    /// Never loads anything.
    template <typename T>
    [[nodiscard]]
    std::shared_ptr<T> get(const std::string& path) const
    {
        const Cache<T>* typedCache = findCache<T>();
        if (typedCache == nullptr) {
            return nullptr;
        }
        const auto found = typedCache->assets.find(path);
        return found != typedCache->assets.end() ? found->second : nullptr;
    }

    template <typename T>
    [[nodiscard]]
    bool isLoaded(const std::string& path) const
    {
        return get<T>(path) != nullptr;
    }

    /// Removes the asset from the cache. Users still holding it keep it alive.
    /// Returns false if it was not loaded.
    template <typename T>
    bool unload(const std::string& path)
    {
        Cache<T>* typedCache = findCache<T>();
        return typedCache != nullptr && typedCache->assets.erase(path) > 0;
    }

    /// Number of loaded assets, all types together.
    [[nodiscard]]
    std::size_t size() const noexcept;

    /// Unloads every asset. Loaders are kept.
    void clear() noexcept;

private:
    /// Type-erased view of a `Cache<T>`.
    class ICache {
    public:
        virtual ~ICache() = default;

        ICache(const ICache&) = delete;
        ICache& operator=(const ICache&) = delete;
        ICache(ICache&&) = delete;
        ICache& operator=(ICache&&) = delete;

        [[nodiscard]]
        virtual std::size_t size() const noexcept
            = 0;

        virtual void clear() noexcept = 0;

    protected:
        ICache() = default;
    };

    /// The loader and the loaded assets of one type.
    template <typename T>
    class Cache final : public ICache {
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

        Loader<T> loader;
        std::unordered_map<std::string, std::shared_ptr<T>> assets;
    };

    template <typename T>
    [[nodiscard]]
    Cache<T>& cache()
    {
        if (Cache<T>* existing = findCache<T>()) {
            return *existing;
        }
        auto created = std::make_unique<Cache<T>>();
        Cache<T>& reference = *created;
        _caches.emplace(std::type_index(typeid(T)), std::move(created));
        return reference;
    }

    template <typename T>
    [[nodiscard]]
    Cache<T>* findCache() noexcept
    {
        const auto found = _caches.find(std::type_index(typeid(T)));
        // Safe: the cache registered under T's type is always a Cache<T>.
        return found != _caches.end() ? static_cast<Cache<T>*>(found->second.get()) : nullptr;
    }

    template <typename T>
    [[nodiscard]]
    const Cache<T>* findCache() const noexcept
    {
        const auto found = _caches.find(std::type_index(typeid(T)));
        return found != _caches.end() ? static_cast<const Cache<T>*>(found->second.get()) : nullptr;
    }

    std::unordered_map<std::type_index, std::unique_ptr<ICache>> _caches;
};

} // namespace ecs
