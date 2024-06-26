#pragma once
#include <cocos2d.h>

namespace util::cocos {
    // Loads the given images in separate threads, in parallel. Blocks the thread until all images have been loaded.
    // This will ONLY load .png images.
    void loadAssetsParallel(const std::vector<std::string>& images);

    enum class AssetPreloadStage {
        DeathEffect,
        Cube,
        Ship,
        Ball,
        Ufo,
        Wave,
        Other,
        All, // all at once
        AllWithoutDeathEffects,
    };

    void preloadAssets(AssetPreloadStage stage);

    bool forcedSkipPreload();
    bool shouldTryToPreload(bool onLoading);

    enum class TextureQuality {
        Low, Medium, High
    };

    TextureQuality getTextureQuality();

    // State that persists between multiple calls to `loadAssetsParallel`, but will be reset upon game reloads (i.e. changing graphics settings or texture packs)
    struct PersistentPreloadState;
    PersistentPreloadState& getPreloadState();
    void resetPreloadState();
    void cleanupThreadPool();

    gd::string fullPathForFilename(const std::string_view filename);

    // Like cocos' func, returns empty string if file doesn't exist.
    gd::string getPathForFilename(const gd::string& filename, const gd::string& searchPath);

    std::string spr(const std::string_view s);

    // creates a new, independent texture
    cocos2d::CCTexture2D* textureFromSpriteName(std::string_view name);

    template <typename To, typename From>
    To convert(const From& value);
}