#include "ecs/Resource/AssetManager.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using ecs::AssetManager;

namespace {

/// Stand-in for a real texture (SFML is not allowed in the ECS).
struct Texture {
    std::string path;
};

struct Sound {
    std::string path;
};

/// A loader that records every file it reads and fails on "missing.png".
class FakeDisk {
public:
    std::unique_ptr<Texture> loadTexture(const std::string& path)
    {
        reads.push_back(path);
        if (path == "missing.png") {
            return nullptr;
        }
        return std::make_unique<Texture>(Texture{.path = path});
    }

    std::vector<std::string> reads;
};

void registerTextureLoader(AssetManager& assets, FakeDisk& disk)
{
    assets.setLoader<Texture>([&disk](const std::string& path) { return disk.loadTexture(path); });
}

} // namespace

TEST(AssetManager, LoadReadsTheFileOnceThenUsesTheCache)
{
    FakeDisk disk;
    AssetManager assets;
    registerTextureLoader(assets, disk);

    const auto first = assets.load<Texture>("player.png");
    const auto second = assets.load<Texture>("player.png");

    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->path, "player.png");
    EXPECT_EQ(first, second); // Same object
    EXPECT_EQ(disk.reads, std::vector<std::string>{"player.png"});
}

TEST(AssetManager, GetNeverLoads)
{
    FakeDisk disk;
    AssetManager assets;
    registerTextureLoader(assets, disk);

    EXPECT_EQ(assets.get<Texture>("player.png"), nullptr);
    EXPECT_TRUE(disk.reads.empty());

    const auto loaded = assets.load<Texture>("player.png");
    EXPECT_EQ(assets.get<Texture>("player.png"), loaded);
    EXPECT_TRUE(assets.isLoaded<Texture>("player.png"));
}

TEST(AssetManager, FailedLoadReturnsNullAndIsRetriedLater)
{
    FakeDisk disk;
    AssetManager assets;
    registerTextureLoader(assets, disk);

    EXPECT_EQ(assets.load<Texture>("missing.png"), nullptr);
    EXPECT_FALSE(assets.isLoaded<Texture>("missing.png"));
    EXPECT_EQ(assets.load<Texture>("missing.png"), nullptr);

    EXPECT_EQ(disk.reads.size(), 2U); // Failures are not cached
}

TEST(AssetManager, LoadingWithoutLoaderThrows)
{
    AssetManager assets;

    EXPECT_FALSE(assets.hasLoader<Texture>());
    EXPECT_THROW((void)assets.load<Texture>("player.png"), std::logic_error);
}

TEST(AssetManager, TypesHaveIndependentCaches)
{
    FakeDisk disk;
    AssetManager assets;
    registerTextureLoader(assets, disk);
    assets.setLoader<Sound>(
        [](const std::string& path) { return std::make_unique<Sound>(Sound{.path = path}); });

    (void)assets.load<Texture>("shared-name");
    (void)assets.load<Sound>("shared-name");

    EXPECT_TRUE(assets.isLoaded<Texture>("shared-name"));
    EXPECT_TRUE(assets.isLoaded<Sound>("shared-name"));
    EXPECT_EQ(assets.size(), 2U);
}

TEST(AssetManager, UnloadKeepsAssetsAliveForTheirUsers)
{
    FakeDisk disk;
    AssetManager assets;
    registerTextureLoader(assets, disk);
    const auto inUse = assets.load<Texture>("player.png");

    EXPECT_TRUE(assets.unload<Texture>("player.png"));
    EXPECT_FALSE(assets.unload<Texture>("player.png"));

    EXPECT_FALSE(assets.isLoaded<Texture>("player.png"));
    EXPECT_EQ(inUse->path, "player.png"); // Still valid for whoever holds it

    (void)assets.load<Texture>("player.png"); // Loaded again from disk
    EXPECT_EQ(disk.reads.size(), 2U);
}

TEST(AssetManager, ClearUnloadsEverythingButKeepsTheLoaders)
{
    FakeDisk disk;
    AssetManager assets;
    registerTextureLoader(assets, disk);
    (void)assets.load<Texture>("a.png");
    (void)assets.load<Texture>("b.png");

    assets.clear();

    EXPECT_EQ(assets.size(), 0U);
    EXPECT_TRUE(assets.hasLoader<Texture>());
    EXPECT_NE(assets.load<Texture>("a.png"), nullptr);
}

TEST(AssetManager, UnknownTypesAreHandledSafely)
{
    const AssetManager assets;

    EXPECT_EQ(assets.get<Sound>("x"), nullptr);
    EXPECT_FALSE(assets.isLoaded<Sound>("x"));
    EXPECT_EQ(assets.size(), 0U);
}
