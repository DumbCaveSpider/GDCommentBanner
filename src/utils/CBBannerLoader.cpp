#include "../include/CBBannerLoader.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/utils/file.hpp>
#include <unordered_map>
#include <vector>

using namespace geode::prelude;

// how many banners can be downloading/decoding at the same time
static constexpr size_t kMaxActiveLoads = 3;

struct PendingBanner {
    WeakRef<LazySprite> sprite;
    std::string source;
};

static LazySprite::Format guessFormat(std::string_view urlOrPath) {
    if (urlOrPath.ends_with(".png")) return CCImage::kFmtPng;
    if (urlOrPath.ends_with(".jpg") || urlOrPath.ends_with(".jpeg")) return CCImage::kFmtJpg;
    return CCImage::kFmtWebp;
}

static bool isRemote(std::string_view source) {
    return source.starts_with("http://") || source.starts_with("https://");
}

// stable across runs, unlike std::hash
static uint64_t fnv1a(std::string_view str) {
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : str) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return hash;
}

static std::filesystem::path getCachePath(std::string_view url) {
    return comment::loader::getCacheDir() / fmt::format("{:016x}.img", fnv1a(url));
}

static bool isOnScreen(CCNode* node) {
    if (!node->isRunning()) return false;
    for (auto n = node; n; n = n->getParent()) {
        if (!n->isVisible()) return false;
    }

    auto a = node->convertToWorldSpace(CCPointZero);
    auto b = node->convertToWorldSpace(node->getContentSize());
    auto minX = std::min(a.x, b.x), maxX = std::max(a.x, b.x);
    auto minY = std::min(a.y, b.y), maxY = std::max(a.y, b.y);

    // small margin so banners just outside the view start loading before scrolled in
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    float margin = winSize.height * 0.25f;
    return maxX >= -margin && minX <= winSize.width + margin &&
           maxY >= -margin && minY <= winSize.height + margin;
}

class BannerLoader : public CCObject {
    std::vector<PendingBanner> m_pending;
    std::vector<WeakRef<LazySprite>> m_decoding;
    std::unordered_map<std::string, std::vector<WeakRef<LazySprite>>> m_downloads;
    bool m_scheduled = false;

    size_t activeCount() const {
        return m_decoding.size() + m_downloads.size();
    }

    void ensureScheduled() {
        if (m_scheduled) return;
        m_scheduled = true;
        CCScheduler::get()->scheduleUpdateForTarget(this, 0, false);
    }

    void startDecode(LazySprite* sprite, std::filesystem::path const& path, LazySprite::Format format) {
        // ignoreCache = true, otherwise every texture stays in CCTextureCache forever
        sprite->loadFromFile(path, format, true);
        m_decoding.push_back(WeakRef(sprite));
    }

    void start(LazySprite* sprite, std::string const& source) {
        if (!isRemote(source)) {
            startDecode(sprite, source, CCImage::kFmtUnKnown);
            return;
        }

        auto cachePath = getCachePath(source);
        std::error_code ec;
        if (std::filesystem::exists(cachePath, ec)) {
            startDecode(sprite, cachePath, guessFormat(source));
            return;
        }

        if (auto it = m_downloads.find(source); it != m_downloads.end()) {
            it->second.push_back(WeakRef(sprite));
            return;
        }
        m_downloads[source].push_back(WeakRef(sprite));

        arc::spawn([url = source, cachePath = std::move(cachePath)]() -> arc::Future<> {
            auto response = co_await web::WebRequest().get(url);
            if (!response.ok()) {
                log::warn("Failed to fetch banner '{}': {}", url, response.errorMessage());
                geode::queueInMainThread([url] {
                    BannerLoader::get()->onDownloaded(url, {});
                });
                co_return;
            }

            // copy instead of std::move(response).data(): that buffer is allocated by
            // Geode.dll, and freeing it from this dll trips the STL heap checks
            auto const& body = response.data();
            std::vector<uint8_t> data(body.begin(), body.end());

            // write to a temp file first so a half-written file is never read back
            auto tmpPath = cachePath;
            tmpPath += ".tmp";
            if (auto res = utils::file::writeBinary(tmpPath, data); res) {
                std::error_code ec;
                std::filesystem::rename(tmpPath, cachePath, ec);
                if (ec) log::warn("Failed to save banner cache '{}': {}", url, ec.message());
            } else {
                log::warn("Failed to save banner cache '{}': {}", url, res.unwrapErr());
            }

            geode::queueInMainThread([url, data = std::move(data)]() mutable {
                BannerLoader::get()->onDownloaded(url, std::move(data));
            });
            co_return;
        });
    }

    void onDownloaded(std::string const& url, std::vector<uint8_t> data) {
        auto it = m_downloads.find(url);
        if (it == m_downloads.end()) return;
        auto waiters = std::move(it->second);
        m_downloads.erase(it);

        auto format = guessFormat(url);
        for (auto& weak : waiters) {
            auto sprite = weak.lock();
            if (!sprite) continue;
            if (data.empty()) {
                sprite->cancelLoad();  // removes the loading circle
                continue;
            }
            // pointer overload so Geode makes its own copy, a std::vector would be
            // allocated here and freed inside Geode.dll
            sprite->loadFromData(data.data(), data.size(), format);
            m_decoding.push_back(WeakRef(sprite.data()));
        }
        this->ensureScheduled();
    }

public:
    static BannerLoader* get() {
        static auto instance = [] {
            auto loader = new BannerLoader();
            loader->retain();
            return loader;
        }();
        return instance;
    }

    void queue(LazySprite* sprite, std::string source) {
        m_pending.push_back({WeakRef(sprite), std::move(source)});
        this->ensureScheduled();
    }

    void update(float) override {
        std::erase_if(m_decoding, [](WeakRef<LazySprite>& weak) {
            auto sprite = weak.lock();
            return !sprite || !sprite->isLoading();
        });

        for (auto it = m_pending.begin(); it != m_pending.end() && activeCount() < kMaxActiveLoads;) {
            auto sprite = it->sprite.lock();
            if (!sprite) {
                it = m_pending.erase(it);
                continue;
            }
            if (!isOnScreen(sprite)) {
                ++it;
                continue;
            }
            auto source = std::move(it->source);
            it = m_pending.erase(it);
            this->start(sprite, source);
        }

        // drop banners that were destroyed before they got a chance to load
        std::erase_if(m_pending, [](PendingBanner& p) { return !p.sprite.lock(); });

        if (m_pending.empty() && m_decoding.empty() && m_downloads.empty()) {
            CCScheduler::get()->unscheduleUpdateForTarget(this);
            m_scheduled = false;
        }
    }
};

namespace comment::loader {
    void queue(LazySprite* sprite, std::string source) {
        if (!sprite || source.empty()) return;
        BannerLoader::get()->queue(sprite, std::move(source));
    }

    std::filesystem::path getCacheDir() {
        static auto dir = [] {
            auto path = Mod::get()->getSaveDir() / "banner_cache";
            (void)utils::file::createDirectoryAll(path);
            return path;
        }();
        return dir;
    }

    void clearDiskCache() {
        std::error_code ec;
        std::filesystem::remove_all(getCacheDir(), ec);
        std::filesystem::create_directories(getCacheDir(), ec);
    }
}
