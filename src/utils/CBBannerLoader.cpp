#include "../include/CBBannerLoader.hpp"
#include "../include/CBConstant.hpp"
#include <Geode/utils/async.hpp>
#include <Geode/utils/file.hpp>
#include <Geode/utils/web.hpp>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace geode::prelude;

namespace comment {
    constexpr size_t kMaxActiveLoads = 5;

    struct PendingBanner {
        WeakRef<LazySprite> sprite;
        std::string source;
        bool priority = false;
    };

    LazySprite::Format guessFormat(std::string_view urlOrPath) {
        if (urlOrPath.ends_with(".png")) return CCImage::kFmtPng;
        if (urlOrPath.ends_with(".jpg") || urlOrPath.ends_with(".jpeg")) return CCImage::kFmtJpg;
        return CCImage::kFmtWebp;
    }

    bool isRemote(std::string_view source) {
        return source.starts_with("http://") || source.starts_with("https://");
    }

    // stable across runs, unlike std::hash
    // https://github.com/irfantopal/fnv1a
    uint64_t fnv1a(std::string_view str) {
        uint64_t hash = 14695981039346656037ull;
        for (unsigned char c : str) {
            hash ^= c;
            hash *= 1099511628211ull;
        }
        return hash;
    }

    asp::fs::Path getCachePath(std::string_view url) {
        return loader::getCacheDir() / fmt::format("{:016x}.img", fnv1a(url));
    }

    bool isOnScreen(CCNode* node) {
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

        void startDecode(LazySprite* sprite, asp::fs::Path const& path, LazySprite::Format format) {
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
            if (cachePath.exists()) {
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
                    queueInMainThread([url] {
                        BannerLoader::get()->onDownloaded(url, {});
                    });
                    co_return;
                }

                auto const& body = response.data();
                std::vector<uint8_t> data(body.begin(), body.end());

                // write to a temp file first so a half-written file is never read back
                auto tmpPath = cachePath;
                tmpPath += ".tmp";
                if (auto res = utils::file::writeBinary(tmpPath, data); res) {
                    auto renamed = asp::fs::rename(tmpPath, cachePath);
                    if (!renamed) log::warn("Failed to save banner cache '{}': {}", url, renamed.unwrapErr());
                } else {
                    log::warn("Failed to save banner cache '{}': {}", url, res.unwrapErr());
                }

                queueInMainThread([url, data = std::move(data)]() mutable {
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

        void queue(LazySprite* sprite, std::string source, bool priority) {
            if (priority) {
                // in front of banners that were queued earlier (e.g. a list behind a preview)
                m_pending.insert(m_pending.begin(), {WeakRef(sprite), std::move(source), true});
            } else {
                m_pending.push_back({WeakRef(sprite), std::move(source), false});
            }
            this->ensureScheduled();
        }

        void update(float) override {
            std::erase_if(m_decoding, [](WeakRef<LazySprite>& weak) {
                auto sprite = weak.lock();
                return !sprite || !sprite->isLoading();
            });

            for (auto it = m_pending.begin(); it != m_pending.end();) {
                // priority loads aren't held back by the limit, the rest wait for a free slot
                if (!it->priority && activeCount() >= kMaxActiveLoads) {
                    ++it;
                    continue;
                }
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

    namespace loader {
        void queue(LazySprite* sprite, std::string source, bool priority) {
            if (!sprite || source.empty()) return;
            BannerLoader::get()->queue(sprite, std::move(source), priority);
        }

        asp::fs::Path getCacheDir() {
            static auto dir = [] {
                auto path = Mod::get()->getSaveDir() / "banner_cache";
                (void)utils::file::createDirectoryAll(path);
                return path;
            }();
            return dir;
        }

        void clearDiskCache() {
            (void)asp::fs::removeDirAll(getCacheDir(), false);
            (void)asp::fs::createDirAll(getCacheDir());
        }
    }

    using Waiter = std::function<void(BannerInfo const&)>;

    struct BannerState {
        std::mutex mutex;
        std::unordered_map<int, BannerInfo> entries;
        std::unordered_map<int, std::vector<Waiter>> waiters;
        std::unordered_set<int> inflight;
        std::unordered_set<int> fromServer;
    };

    // leaked on purpose, sprites may finish after static teardown
    BannerState& state() {
        static auto instance = new BannerState();
        return *instance;
    }

    void resolve(int accountId, BannerInfo const& info) {
        std::vector<Waiter> waiters;
        {
            auto& s = state();
            std::lock_guard<std::mutex> lock(s.mutex);
            s.entries[accountId] = info;
            s.fromServer.insert(accountId);
            s.inflight.erase(accountId);
            if (auto it = s.waiters.find(accountId); it != s.waiters.end()) {
                waiters = std::move(it->second);
                s.waiters.erase(it);
            }
        }
        for (auto& waiter : waiters) {
            if (waiter) waiter(info);
        }
    }

    void fetch(int accountId) {
        arc::spawn([accountId]() -> arc::Future<> {
            BannerInfo info;

            auto request = web::WebRequest();
            auto body = matjson::makeObject({{"targetAccountId", accountId}});
            auto response = co_await request.bodyJSON(body).post(fmt::format("{}/getImageBanner", baseUrl));

            if (!response.ok()) {
                log::warn("getImageBanner failed for account {}: {}", accountId, response.code());
            } else if (auto jsonRes = response.json(); jsonRes.isOk()) {
                auto json = jsonRes.unwrap();
                if (json["equipped"].asBool().unwrapOr(false)) {
                    info.imageUrl = json["imageUrl"].asString().unwrapOr("");
                    info.nonCompactImageUrl = json["nonCompactImageUrl"].asString().unwrapOr("");
                    info.equipped = !info.imageUrl.empty();
                }
            } else {
                log::warn("getImageBanner returned invalid JSON for account {}", accountId);
            }

            log::debug("banner for account {}: equipped={} url='{}'", accountId, info.equipped, info.imageUrl);

            queueInMainThread([accountId, info] {
                resolve(accountId, info);
            });

            co_return;
        });
    }

    void requestBannerInfo(int accountId, BannerSource source, std::function<void(BannerInfo const&)> callback) {
        if (accountId <= 0) return;

        auto& s = state();
        std::optional<BannerInfo> known;
        bool shouldFetch = false;
        {
            std::lock_guard<std::mutex> lock(s.mutex);
            auto it = s.entries.find(accountId);
            bool usable = it != s.entries.end() && (source == BannerSource::Cached || s.fromServer.contains(accountId));
            if (usable) known = it->second;

            if (!known) {
                if (callback) s.waiters[accountId].push_back(std::move(callback));
                shouldFetch = s.inflight.insert(accountId).second;
            }
        }

        if (known) {
            if (callback) callback(*known);
            return;
        }
        if (shouldFetch) fetch(accountId);
    }

    LazySprite* emptySprite(CCSize const& size) {
        if (size.width <= 0.f || size.height <= 0.f) return nullptr;

        auto sprite = LazySprite::create(size, true);
        if (!sprite) return nullptr;
        sprite->setAutoResize(true);
        return sprite;
    }

    void loadSprite(LazySprite* sprite, std::string url, bool priority) {
        if (!sprite || url.empty()) return;
        if (url.starts_with("/")) url = fmt::format("{}{}", baseUrl, url);
        log::debug("queueing banner '{}'", url);
        loader::queue(sprite, std::move(url), priority);
    }

    LazySprite* createBannerSprite(int accountId, BannerVariant variant, BannerSource source, CCSize const& size, bool priority) {
        if (accountId <= 0) return nullptr;

        auto sprite = emptySprite(size);
        if (!sprite) return nullptr;

        requestBannerInfo(accountId, source, [weak = WeakRef<LazySprite>(sprite), variant, priority](BannerInfo const& info) {
            auto locked = weak.lock();
            if (!locked) return;

            auto url = info.urlFor(variant);
            if (!info.equipped || url.empty()) {
                locked->setVisible(false);
                return;
            }
            loadSprite(locked.data(), std::move(url), priority);
        });

        return sprite;
    }

    LazySprite* createBannerSpriteFromSource(std::string const& source, CCSize const& size, bool priority) {
        auto sprite = emptySprite(size);
        if (!sprite) return nullptr;
        loadSprite(sprite, source, priority);
        return sprite;
    }

    void invalidateBannerCache(int accountId) {
        auto& s = state();
        std::lock_guard<std::mutex> lock(s.mutex);
        if (accountId <= 0) {
            s.entries.clear();
            s.fromServer.clear();
        } else {
            s.entries.erase(accountId);
            s.fromServer.erase(accountId);
        }
    }

    void updateBannerCache(int accountId, bool equipped, std::string const& imageUrl, std::string const& nonCompactImageUrl) {
        if (accountId <= 0) return;
        auto& s = state();
        std::lock_guard<std::mutex> lock(s.mutex);
        s.entries[accountId] = BannerInfo{equipped, imageUrl, nonCompactImageUrl};
        s.fromServer.erase(accountId);
    }
}
