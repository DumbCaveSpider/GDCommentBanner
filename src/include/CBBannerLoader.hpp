#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/LazySprite.hpp>
#include <asp/fs/fs.hpp>
#include <functional>

using namespace geode::prelude;

namespace comment::loader {
    void queue(LazySprite* sprite, std::string source, bool priority = false);

    asp::fs::Path getCacheDir();
    void clearDiskCache();
}

namespace comment {
    enum class BannerVariant {
        Compact,
        NonCompact,
    };

    enum class BannerSource {
        Cached,
        Server,
    };

    struct BannerInfo {
        bool equipped = false;
        std::string imageUrl = "";
        std::string nonCompactImageUrl = "";

        std::string urlFor(BannerVariant variant) const {
            if (variant == BannerVariant::NonCompact && !nonCompactImageUrl.empty()) return nonCompactImageUrl;
            return imageUrl;
        }
    };

    void requestBannerInfo(int accountId, BannerSource source, std::function<void(BannerInfo const&)> callback);
    LazySprite* createBannerSprite(int accountId, BannerVariant variant, BannerSource source, cocos2d::CCSize const& size, bool priority = false);
    LazySprite* createBannerSpriteFromSource(std::string const& source, cocos2d::CCSize const& size, bool priority = false);
}
