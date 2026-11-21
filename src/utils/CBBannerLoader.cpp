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
    static bool isRemote(std::string_view source) {
        return source.starts_with("http://") || source.starts_with("https://");
    }

    // stable across runs, unlike std::hash
    // https://github.com/irfantopal/fnv1a
    static uint64_t fnv1a(std::string_view str) {
        uint64_t hash = 14695981039346656037ull;
        for (unsigned char c : str) {
            hash ^= c;
            hash *= 1099511628211ull;
        }
        return hash;
    }

    static asp::fs::Path getCachePath(std::string_view url) {
        return loader::getCacheDir() / fmt::format("{:016x}.img", fnv1a(url));
    }

    // i know its gonna be webp but i dont want to hardcode it in case i change my mind later
    static void loadFile(LazySprite* lazy, asp::fs::Path path) {
        lazy->loadFromFile(std::move(path), CCImage::kFmtUnKnown, true);
    }

    // drops the loading circle for a banner that will never show up
    static void hideLater(WeakRef<LazySprite> weak) {
        Loader::get()->queueInMainThread([weak = std::move(weak)]() {
            if (auto sprite = weak.lock()) sprite->setVisible(false);
        });
    }

    // yang's implementation for loading nameplates in rated layouts. :)
    static void loadRemote(LazySprite* lazy, std::string url) {
        auto cachePath = getCachePath(url);
        if (cachePath.exists()) {
            loadFile(lazy, std::move(cachePath));
            return;
        }

        arc::spawn([weak = WeakRef(lazy), url = std::move(url), cachePath = std::move(cachePath)]() -> arc::Future<> {
            auto response = co_await web::WebRequest().get(url);
            if (!response.ok()) {
                log::warn("failed to fetch banner '{}' ({}): {}", url, response.code(), response.errorMessage());
                hideLater(weak);
                co_return;
            }

            auto data = std::move(response).data();
            if (data.empty()) {
                log::warn("banner '{}' returned an empty body", url);
                hideLater(weak);
                co_return;
            }

            if (auto saved = utils::file::writeBinarySafe(cachePath, data); !saved) {
                log::warn("failed to cache banner '{}': {}", url, saved.unwrapErr());
            }

            Loader::get()->queueInMainThread([weak, data = std::move(data)]() mutable {
                if (auto sprite = weak.lock()) {
                    sprite->loadFromData(std::move(data), CCImage::kFmtUnKnown);
                }
            });

            co_return;
        });
    }

    namespace loader {
        void queue(LazySprite* sprite, std::string source, bool priority) {
            if (!sprite || source.empty()) return;

            // loads are not throttled anymore, so there is nothing for a priority load to jump ahead of
            (void)priority;

            if (isRemote(source)) {
                loadRemote(sprite, std::move(source));
            } else {
                loadFile(sprite, asp::fs::Path(std::move(source)));
            }
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
        // a banner that fails to load shouldnt leave a spinning loading circle behind
        sprite->setLoadCallback([weak = WeakRef<LazySprite>(sprite)](Result<> res) {
            if (res) return;
            log::warn("banner failed to load: {}", res.unwrapErr());
            if (auto locked = weak.lock()) locked->setVisible(false);
        });
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
