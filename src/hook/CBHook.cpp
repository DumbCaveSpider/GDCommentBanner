#include <Geode/Geode.hpp>
#include <Geode/binding/GameLevelManager.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/LazySprite.hpp>
#include <Geode/ui/NineSlice.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <argon/argon.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/modify/CommentCell.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/CurrencyRewardLayer.hpp>
#include "../CBShopLayer.hpp"
#include "../include/CBConstant.hpp"
#include "../include/CBLocalBanner.hpp"

using namespace geode::prelude;

#include <asp/time/SystemTime.hpp>
#include <mutex>

struct CachedBanner {
    bool equipped;
    std::string imageUrl;
    asp::time::SystemTime timestamp;
};

static std::map<int, CachedBanner> s_bannerCache;
static std::mutex s_cacheMutex;

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
                }
                if (GJAccountManager::sharedState()->m_accountID == 0) {
                    FLAlertLayer::create(
                        "Comment Banners",
                        "You must be <cg>logged in</c> to access this feature in <cp>Comment Banners.</c>",
                        "OK")
                        ->show();
                    return;
                }
                auto scene = CCScene::create();
                if (auto shop = CBShopLayer::create()) {
                    scene->addChild(shop);
                    CCDirector::sharedDirector()->pushScene(CCTransitionMoveInT::create(0.5f, scene));
                }
            });
            shardMenu->addChild(shopButton);
            shardMenu->updateLayout();
        }

        return true;
    }
};

// this is where to fetch the images
static void setupBannerSprite(CommentCell* cell, std::string const& imageUrl) {
    Ref<CommentCell> self = cell;
    if (!self || !self->getParent() || !self->m_backgroundLayer) return;

    if (imageUrl.empty()) return;

    auto size = self->m_backgroundLayer->getScaledContentSize();
    auto bannerSize = self->m_compactMode ? size : CCSize{800.f, 800.f};

    auto bannerNode = comment::createBannerNode(imageUrl, bannerSize);
    bannerNode->setID("cb-comment-banner-node");
    bannerNode->setPosition({size.width / 2.f, size.height / 2.f});

    self->m_backgroundLayer->setOpacity(100);
    self->m_backgroundLayer->addChild(bannerNode, -1);

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
}

class $modify(CBCommentCell, CommentCell) {
    void loadFromComment(GJComment* comment) {
        CommentCell::loadFromComment(comment);
        if (!m_backgroundLayer) {
            return;
        }

        // Clean up recycled state
        m_backgroundLayer->setOpacity(255);
        if (auto bg = m_mainLayer->getChildByID("cb-comment-banner-bg")) {
            bg->removeFromParent();
        }
        if (auto prevBanner = m_backgroundLayer->getChildByID("cb-comment-banner-node")) {
            prevBanner->removeFromParent();
        }

        // account comment
        //if (m_accountComment) {
        //    if (Mod::get()->getSettingValue<bool>("hide-account-comment")) return;
        //}

        auto self = Ref<CBCommentCell>(this);
        int accountId = comment->m_accountID;

        // Check local banner priority for own comment
        int myAccountId = GJAccountManager::sharedState()->m_accountID;
        if (myAccountId == 0) {
            myAccountId = argon::getGameAccountData().accountId;
        }
        if (myAccountId > 0 && accountId == myAccountId) {
            std::string equippedLocal = Mod::get()->getSavedValue<std::string>("equipped-local-banner", "");
            if (!equippedLocal.empty()) {
                auto localPath = comment::local::getLocalBannersDir() / equippedLocal;
                if (std::filesystem::exists(localPath)) {
                    setupBannerSprite(self.data(), localPath.string());
                    return;
                }
            }
        }

        // Check cache
        {
            std::lock_guard<std::mutex> lock(s_cacheMutex);
            if (s_bannerCache.contains(accountId)) {
                auto& cached = s_bannerCache[accountId];
                auto now = asp::time::SystemTime::now();
                auto durationHours = Mod::get()->getSettingValue<int64_t>("cache-duration-hours");
                auto ageDur = now.durationSince(cached.timestamp);

                if (ageDur && ageDur.value().hours() < durationHours) {
                    if (!cached.equipped) return;

                    std::string imageUrl = cached.imageUrl;
                    setupBannerSprite(self.data(), imageUrl);
                    return;
                }
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
                {
                    std::lock_guard<std::mutex> lock(s_cacheMutex);
                    s_bannerCache[accountId] = newCached;
                }
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

            {
                std::lock_guard<std::mutex> lock(s_cacheMutex);
                s_bannerCache[accountId] = newCached;
            }

            geode::queueInMainThread([self, imageUrl] {
                if (!self || !self->getParent()) return;
                setupBannerSprite(self.data(), imageUrl);
            });

            co_return;
        });
    }
};

class $modify(CBPlayLayer, PlayLayer) {
    struct Fields {
        bool m_wasCompletedBefore = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        m_fields->m_wasCompletedBefore = GameStatsManager::sharedState()->hasCompletedOnlineLevel(level->m_levelID) || GameStatsManager::sharedState()->hasCompletedLevel(level);
        log::debug("completed level for ame? {}", m_fields->m_wasCompletedBefore);
        return true;
    }
};

class $modify(CBEndLevelLayer, EndLevelLayer) {
    struct Fields {
        async::TaskHolder<web::WebResponse> m_submitTask;
        ~Fields() {
            m_submitTask.cancel();
        }
    };

    void customSetup() override {
        EndLevelLayer::customSetup();

        auto playLayer = PlayLayer::get();
        if (!playLayer) return;

        GJGameLevel* level = playLayer->m_level;
        if (!level || level->m_levelID == 0) return;

        if (level->m_normalPercent < 100) {
            log::info("Level ID: {} not completed, skipping amethyst reward", level->m_levelID);
            return;
        }

        if (playLayer->m_isPracticeMode) {
            log::warn("Completed in Practice Mode, ignore reward");
            return;
        }

        if (level->m_stars == 0) {
            log::debug("unrated level completion, skipping amethyst reward submission");
            return;
        }

        auto cbPlayLayer = static_cast<CBPlayLayer*>(playLayer);
        if (cbPlayLayer && cbPlayLayer->m_fields->m_wasCompletedBefore) {
            log::warn("Level already completed before this run, skip amethyst reward");
            return;
        }

        if ((level->m_attemptTime <= 25 || level->m_attemptTime >= 28000000) && !level->isPlatformer()) {
            log::warn("Attempt time is invalid for amethyst reward submission: {}", level->m_attemptTime);
            return;
        }

        if (level->m_jumps == 0 && level->m_stars != 1) {
            log::warn("Level has no jumps on non auto? skip amethyst reward.");
            return;
        }

        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;
        if (accountId <= 0) {
            log::warn("Invalid account ID for amethyst reward submission.");
            return;
        }

        int levelId = level->m_levelID;

        Ref<EndLevelLayer> endLayerRef = this;

        m_fields->m_submitTask.spawn(
            [accountData, accountId, levelId]() -> arc::Future<web::WebResponse> {
                auto authResult = co_await comment::argonToken(accountData);
                if (authResult.empty()) {
                    log::warn("argon auth failed for amethyst reward");
                    co_return web::WebResponse();
                }

                auto authToken = std::move(authResult);
                auto request = geode::utils::web::WebRequest();
                auto body = matjson::makeObject({
                    {"accountId", accountId},
                    {"argonToken", authToken},
                    {"levelId", levelId},
                });

                co_return co_await request.bodyJSON(body).post(fmt::format("{}/submitAmethystReward", comment::baseUrl));
            },
            [endLayerRef, levelId](web::WebResponse submitResponse) {
                log::info("Received submitAmethystReward response for level ID: {}", levelId);

                if (!endLayerRef || !endLayerRef->m_mainLayer) {
                    log::warn("EndLevelLayer has been destroyed or m_mainLayer is invalid");
                    return;
                }

                if (!submitResponse.ok()) {
                    log::warn("submitAmethystReward returned non-ok status: {}", submitResponse.code());
                    return;
                }

                auto submitJsonRes = submitResponse.json();
                if (!submitJsonRes) {
                    log::warn("Failed to parse submitAmethystReward JSON response");
                    return;
                }

                auto submitJson = submitJsonRes.unwrap();
                bool success = submitJson["success"].asBool().unwrapOrDefault();
                int amethystReward = submitJson["amethystReward"].asInt().unwrapOrDefault();

                log::info("submitAmethystReward success: {}, amethyst reward: {}", success, amethystReward);

                if (success) {
                    int current = submitJson["totalAmethyst"].asInt().unwrapOrDefault();
                    Mod::get()->setSavedValue("amethyst", current);

                    if (amethystReward <= 0) {
                        log::warn("No amethyst rewarded, possibly already rewarded before");
                        Notification::create("Amethyst has already been claimed for this level!", CCSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr))->show();
                        return;
                    }

                    // make the amethyst reward pop when u complete the level
                    auto amethystPop = CCSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr);
                    amethystPop->setScale(1.2f);
                    amethystPop->setPosition({endLayerRef->m_mainLayer->getContentSize().width / 2 + 155,
                        endLayerRef->m_mainLayer->getContentSize().height / 2 - 85});
                    amethystPop->setOpacity(0);
                    amethystPop->setID("cb-amethyst-reward-sprite");
                    endLayerRef->m_mainLayer->addChild(amethystPop);

                    auto amethystLabel = CCLabelBMFont::create(
                        fmt::format("+{}", amethystReward).c_str(), "bigFont.fnt");
                    amethystLabel->setPosition({-5, amethystPop->getContentSize().height / 2});
                    amethystLabel->setAnchorPoint({1.0f, 0.5f});
                    amethystLabel->limitLabelWidth(80, 1.f, 0.5f);
                    amethystLabel->setID("cb-amethyst-reward-label");
                    amethystLabel->setOpacity(0);
                    amethystPop->addChild(amethystLabel);

                    auto scaleAction = CCScaleBy::create(.6f, .6f);
                    auto bounceAction = CCEaseBounceOut::create(scaleAction);
                    auto fadeAction = CCFadeIn::create(0.4f);
                    auto spawnAction = CCSpawn::createWithTwoActions(bounceAction, fadeAction);
                    amethystPop->runAction(spawnAction);

                    amethystLabel->runAction(CCFadeIn::create(fadeAction->getDuration()));

                    bool animationEnabled = !Mod::get()->getSettingValue<bool>("disable-reward-animation");
                    if (animationEnabled && amethystReward > 0) {
                        if (auto rewardLayer = CurrencyRewardLayer::create(
                                0, 0, 0, amethystReward, CurrencySpriteType::Star, 0, CurrencySpriteType::Star, 0, amethystPop->getPosition(), CurrencyRewardType::Default, 0.0, 1.0)) {
                            rewardLayer->setID("cb-currency-reward-layer");

                            if (rewardLayer->m_diamondsLabel) {
                                rewardLayer->m_diamonds = 0;
                                rewardLayer->incrementDiamondsCount(current - amethystReward);
                                rewardLayer->m_diamondsLabel->runAction(CCRepeatForever::create(CCSequence::create(
                                    CCTintTo::create(0.5f, 255, 100, 255),
                                    CCTintTo::create(0.5f, 255, 255, 255),
                                    nullptr)));
                            }

                            std::string frameName = "CB_amethyst_001.png"_spr;
                            auto displayFrame = CCSpriteFrameCache::sharedSpriteFrameCache()->spriteFrameByName(frameName.c_str());
                            CCTexture2D* texture = nullptr;
                            if (!displayFrame) {
                                texture = CCTextureCache::sharedTextureCache()->addImage(frameName.c_str(), false);
                                if (texture) {
                                    displayFrame = CCSpriteFrame::createWithTexture(texture, {{0, 0}, texture->getContentSize()});
                                }
                            } else {
                                texture = displayFrame->getTexture();
                            }

                            if (rewardLayer->m_diamondsSprite && displayFrame) {
                                rewardLayer->m_diamondsSprite->setDisplayFrame(displayFrame);
                            }

                            if (rewardLayer->m_currencyBatchNode && texture) {
                                rewardLayer->m_currencyBatchNode->setTexture(texture);
                            }

                            for (auto sprite : CCArrayExt<CurrencySprite*>(rewardLayer->m_objects)) {
                                if (!sprite) continue;
                                if (sprite->m_burstSprite) sprite->m_burstSprite->setVisible(false);
                                if (auto child = sprite->getChildByIndex(0)) {
                                    child->setVisible(false);
                                }

                                if (sprite->m_spriteType == CurrencySpriteType::Diamond) {
                                    if (displayFrame) {
                                        sprite->setDisplayFrame(displayFrame);
                                    }
                                }
                            }

                            endLayerRef->addChild(rewardLayer, 100);
                            FMODAudioEngine::sharedEngine()->playEffect("secretKey.ogg");
                        }
                    } else {
                        log::info("Reward animation disabled");
                        Notification::create(
                            fmt::format("Awarded {} amethyst", GameToolbox::pointsToString(amethystReward)),
                            CCSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr),
                            2.f)
                            ->show();
                        FMODAudioEngine::sharedEngine()->playEffect("secretKey.ogg");
                    }
                } else if (!success && amethystReward == 0) {
                    Notification::create(
                        "Amethyst has already been claimed for this level!",
                        CCSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr))
                        ->show();
                    log::info("already claimed for level ID: {}", levelId);
                }
            });
    }
};
