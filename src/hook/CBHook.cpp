#include <fstream>
#include <sstream>
#include <Geode/Geode.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/LazySprite.hpp>
#include <Geode/ui/NineSlice.hpp>
#include <Geode/utils/web.hpp>
#include <argon/argon.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/modify/CommentCell.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include "../CBShopLayer.hpp"
#include "../include/CBConstant.hpp"

using namespace geode::prelude;

#include <asp/time/SystemTime.hpp>

struct CachedBanner {
    bool equipped;
    std::string imageUrl;
    asp::time::SystemTime timestamp;
};

static std::map<int, CachedBanner> s_bannerCache;
static bool s_cacheLoaded = false;

static void loadCache() {
    if (s_cacheLoaded) return;
    auto path = Mod::get()->getSaveDir() / "banner_cache.json";
    if (std::filesystem::exists(path)) {
        std::ifstream file(path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            if (auto jsonParseRes = matjson::parse(buffer.str())) {
                auto json = jsonParseRes.unwrap();
                if (json.isObject()) {
                    for (auto const& [key, value] : json) {
                        int accountId = numFromString<int>(key).unwrapOr(0);
                        CachedBanner banner;
                        banner.equipped = value["equipped"].asBool().unwrapOr(false);
                        banner.imageUrl = value["imageUrl"].asString().unwrapOr("");
                        long long timeMs = value["timestamp"].asInt().unwrapOr(0);
                        banner.timestamp = asp::time::SystemTime::fromUnixMillis(timeMs);
                        s_bannerCache[accountId] = banner;
                    }
                }
            }
        }
    }
    s_cacheLoaded = true;
}

static void saveCache() {
    auto path = Mod::get()->getSaveDir() / "banner_cache.json";
    matjson::Value obj = matjson::makeObject({});
    for (auto const& [accountId, banner] : s_bannerCache) {
        auto timeMs = banner.timestamp.timeSinceEpoch().millis<uint64_t>();
        obj.set(numToString(accountId), matjson::makeObject({{"equipped", banner.equipped}, {"imageUrl", banner.imageUrl}, {"timestamp", timeMs}}));
    }
    std::ofstream file(path);
    if (file.is_open()) {
        file << obj.dump(matjson::NO_INDENTATION);
    }
}

class $modify(GJGarageLayer) {
    bool init() {
        if (!GJGarageLayer::init())
            return false;
        if (auto shardMenu = this->getChildByID("shards-menu")) {
            auto shopButton = Button::createWithNode(CircleButtonSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small), [](geode::Button* button) {
                // check if rl is loaded and if so disable the nameplates on comments
                if (Loader::get()->isModLoaded("arcticwoof.rated_layouts")) {
                    auto rl = Loader::get()->getLoadedMod("arcticwoof.rated_layouts");
                    if (!rl->getSettingValue<bool>("disableNameplateInComment")) {
                        rl->setSettingValue("disableNameplateInComment", true);  // imagine disabling my own other mod settings sybru
                        FLAlertLayer::create("Compatibility Notice", "<cp>Comment Banners</c> has detected that you have <cl>Rated Layouts</c> installed.\nThe <cy>Disable Nameplate in Comments</c> setting has been forcibly <cg>enabled</c> in <cl>Rated Layouts</c>' settings to prevent conflicts.", "OK")->show();
                        return;
                    }
                    if (GJAccountManager::sharedState()->m_accountID == 0) {
                        FLAlertLayer::create(
                            "Comment Banners",
                            "You must be <cg>logged in</c> to access this feature in <cp>Comment Banners.</c>",
                            "OK")
                            ->show();
                        return;
                    }
                }
                auto scene = CCScene::create();
                scene->addChild(CBShopLayer::create());
                CCDirector::sharedDirector()->pushScene(CCTransitionMoveInT::create(0.5f, scene));
            });
            shardMenu->addChild(shopButton);
            shardMenu->updateLayout();
        }

        return true;
    }
};

// this is where to fetch the images
class $modify(CBCommentCell, CommentCell) {
    void loadFromComment(GJComment* comment) {
        CommentCell::loadFromComment(comment);
        if (!m_backgroundLayer) {
            return;
        }

        if (m_accountComment) return;  // don't load banner for account comment

        auto self = Ref<CBCommentCell>(this);
        int accountId = comment->m_accountID;
        loadCache();

        // Check cache
        if (s_bannerCache.contains(accountId)) {
            auto& cached = s_bannerCache[accountId];
            auto now = asp::time::SystemTime::now();
            auto durationHours = Mod::get()->getSettingValue<int64_t>("cache-duration-hours");
            auto ageDur = now.durationSince(cached.timestamp);

            if (ageDur && ageDur.value().hours() < durationHours) {
                if (!cached.equipped) return;

                std::string imageUrl = cached.imageUrl;
                geode::queueInMainThread([self, imageUrl]() {
                    if (!self->m_backgroundLayer) return;
                    auto size = self->m_backgroundLayer->getScaledContentSize();
                    auto bannerSize = self->m_compactMode ? size : CCSize{800.f, 800.f};
                    auto bannerSprite = LazySprite::create(bannerSize, true);
                    bannerSprite->setAutoResize(true);
                    bannerSprite->loadFromUrl(imageUrl, LazySprite::Format::kFmtWebp, true);
                    bannerSprite->setPosition({size.width / 2.f, size.height / 2.f});
                    self->m_backgroundLayer->setOpacity(100);
                    self->m_backgroundLayer->addChild(bannerSprite, -1);

                    if (self->m_compactMode) {
                        if (auto commentText = self->m_mainLayer->getChildByIDRecursive("comment-text-label")) {
                            if (!self->m_mainLayer->getChildByID("cb-comment-banner-bg")) {
                                auto commentBg = NineSlice::create("square02_small.png");
                                commentBg->setID("cb-comment-banner-bg");
                                commentBg->setInsets({5, 5, 5, 5});
                                commentBg->setContentSize(commentText->getScaledContentSize() + CCSize(5, 0));
                                commentBg->setPosition({commentText->getPosition().x - 2, commentText->getPosition().y});
                                commentBg->setOpacity(150);
                                commentBg->setAnchorPoint(commentText->getAnchorPoint());
                                self->m_mainLayer->addChild(commentBg, -1);
                            }
                        }
                    }
                });
                return;
            }
        }

        arc::spawn([self, accountId]() -> arc::Future<> {
            auto request = geode::utils::web::WebRequest();
            auto body = matjson::makeObject({
                {"targetAccountId", accountId},
            });

            auto response = co_await request.bodyJSON(body).post(fmt::format("{}/getImageBanner", comment::baseUrl));
            if (!response.ok()) {
                log::debug("getImageBanner failed: {}", response.errorMessage());
                co_return;
            }

            auto jsonRes = response.json();
            if (jsonRes.isErr()) {
                log::debug("getImageBanner returned invalid JSON");
                co_return;
            }

            auto json = jsonRes.unwrap();
            auto equipped = json["equipped"].asBool().unwrapOr(false);
            if (!equipped) {
                CachedBanner newCached;
                newCached.equipped = false;
                newCached.imageUrl = "";
                newCached.timestamp = asp::time::SystemTime::now();
                s_bannerCache[accountId] = newCached;
                saveCache();
                co_return;
            }

            auto imageUrlRes = json["imageUrl"].asString();
            if (imageUrlRes.isErr()) {
                log::debug("getImageBanner missing imageUrl");
                co_return;
            }

            auto imageUrl = imageUrlRes.unwrap();

            // Save to cache
            CachedBanner newCached;
            newCached.equipped = true;
            newCached.imageUrl = imageUrl;
            newCached.timestamp = asp::time::SystemTime::now();
            s_bannerCache[accountId] = newCached;
            saveCache();

            geode::queueInMainThread([self, imageUrl]() {
                if (!self->m_backgroundLayer) {
                    return;
                }

                auto size = self->m_backgroundLayer->getScaledContentSize();
                auto bannerSize = self->m_compactMode ? size : CCSize{800.f, 800.f};
                auto bannerSprite = LazySprite::create(bannerSize, true);
                bannerSprite->setAutoResize(true);
                bannerSprite->loadFromUrl(imageUrl, LazySprite::Format::kFmtWebp, true);
                bannerSprite->setPosition({size.width / 2.f, size.height / 2.f});
                self->m_backgroundLayer->setOpacity(100);
                self->m_backgroundLayer->addChild(bannerSprite, -1);

                if (self->m_compactMode) {
                    if (auto commentText = self->m_mainLayer->getChildByIDRecursive("comment-text-label")) {
                        if (!self->m_mainLayer->getChildByID("cb-comment-banner-bg")) {
                            auto commentBg = NineSlice::create("square02_small.png");
                            commentBg->setID("cb-comment-banner-bg");
                            commentBg->setInsets({5, 5, 5, 5});
                            commentBg->setContentSize(commentText->getScaledContentSize() + CCSize(5, 0));
                            commentBg->setPosition({commentText->getPosition().x - 2, commentText->getPosition().y});
                            commentBg->setOpacity(150);
                            commentBg->setAnchorPoint(commentText->getAnchorPoint());
                            self->m_mainLayer->addChild(commentBg, -1);
                        }
                    }
                }
            });

            co_return;
        });
    }
};

class $modify(CBEndLevelLayer, EndLevelLayer) {
    void customSetup() {
        EndLevelLayer::customSetup();

        auto endLayer = Ref<EndLevelLayer>(this);

        geode::queueInMainThread([endLayer]() {
            auto accountId = argon::getGameAccountData().accountId;
            if (accountId <= 0) {
                log::warn("Invalid account ID for amethyst reward submission.");
                return;
            }

            if (endLayer->m_playLayer->m_level->m_stars == 0) {
                log::debug("unrated level completion, skipping amethyst reward submission");
                return;
            }

            if (!endLayer->m_playLayer || !endLayer->m_playLayer->m_level) {
                log::warn("Unable to determine the completed level.");
                return;
            }

            auto level = endLayer->m_playLayer->m_level;
            if (!level->m_isCompletionLegitimate) {
                log::warn("Level completion is not legitimate for amethyst reward submission.");
                return;
            }

            if (level->m_attemptTime <= 25 || level->m_attemptTime >= 28000000) {
                log::warn("Attempt time is invalid for amethyst reward submission: {}", level->m_attemptTime);
                return;
            }

            int levelId = level->m_levelID;
            auto accountData = argon::getGameAccountData();

            arc::spawn([endLayer, accountId, accountData, levelId]() -> arc::Future<> {
                auto authResult = co_await comment::argonToken(accountData);
                if (authResult.empty()) {
                    log::warn("argon auth failed for amethyst reward");
                    co_return;
                }

                auto authToken = std::move(authResult);
                auto request = geode::utils::web::WebRequest();
                auto body = matjson::makeObject({
                    {"accountId", accountId},
                    {"argonToken", authToken},
                    {"levelId", levelId},
                });

                auto response = co_await request.bodyJSON(body).post(fmt::format("{}/submitAmethystReward", comment::baseUrl));
                if (!response.ok()) {
                    log::warn("submitAmethystReward failed: {}", response.errorMessage());
                    co_return;
                }

                auto jsonRes = response.json();
                if (jsonRes.isErr()) {
                    log::warn("submitAmethystReward returned invalid JSON");
                    co_return;
                }

                auto json = jsonRes.unwrap();
                if (!json["success"].asBool().unwrapOr(false)) {
                    log::warn("submitAmethystReward returned failure");
                    co_return;
                }

                auto rewardRes = json["amethystReward"].asInt();
                if (rewardRes.isErr()) {
                    log::warn("submitAmethystReward missing amethystReward");
                    co_return;
                }

                int amethystReward = static_cast<int>(rewardRes.unwrap());
                auto current = Mod::get()->getSavedValue<int>("amethyst", 0);
                Mod::get()->setSavedValue("amethyst", current + amethystReward);

                geode::queueInMainThread([amethystReward]() {
                    Notification::create(fmt::format("Awarded {} amethyst", GameToolbox::pointsToString(amethystReward)), CCSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr))->show();
                    FMODAudioEngine::sharedEngine()->playEffect("secretKey.ogg");
                });

                log::debug("awarded {} amethyst from submitAmethystReward", amethystReward);
                co_return;
            });
        });
    }
};