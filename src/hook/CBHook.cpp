#include <Geode/Geode.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/LazySprite.hpp>
#include <Geode/ui/NineSlice.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <dankmeme.argon/argon.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/modify/CommentCell.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/ThreadViewPopup.hpp>
#include "../CBAmethystRewardLayer.hpp"
#include "../CBShopLayer.hpp"
#include "../include/CBConstant.hpp"
#include "../include/CBLocalBanner.hpp"

using namespace geode::prelude;

class $modify(GJGarageLayer) {
    bool init() {
        if (!GJGarageLayer::init())
            return false;
        if (auto shardMenu = this->getChildByID("shards-menu")) {
            auto shopIcon = CircleButtonSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small);
            if (!shopIcon) {
                return true;
            }
            auto shopButton = Button::createWithNode(shopIcon, [](Button* button) {
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
                pushSceneWithLayer(CBShopLayer::create());
            });
            shardMenu->addChild(shopButton);
            shardMenu->updateLayout();
        }

        return true;
    }
};

static void clearRowBanner(CCNode* row) {
    if (!row) return;
    // rows get recycled, so always clear the previous banner
    if (auto prevBanner = row->getChildByID("cb-thread-banner-node")) {
        prevBanner->removeFromParent();
    }
}

class $modify(CBThreadViewPopup, ThreadViewPopup) {
    // todo: get GJThreadMessage fields when geode bindings adds then
    void mountVirtualRow(int index, CCNode* row, CCMenu* menu) {
        ThreadViewPopup::mountVirtualRow(index, row, menu);
        // if (!row) return;

        // // rows get recycled, so drop whatever the previous mount left behind
        // clearRowBanner(row);

        //

        // auto size = row->getContentSize();
        // if (size.width <= 0.f) size.width = m_listWidth > 0.f ? m_listWidth : 320.f;
        // if (size.height <= 0.f) size.height = this->virtualRowHeight(index);

        // if (m_fields->m_mountLogs < 8) {
        //     m_fields->m_mountLogs++;
        //     log::debug("row {}: account {} ({}x{})", index, accountId, size.width, size.height);
        // }

        // if (accountId <= 0 || size.width <= 0.f || size.height <= 0.f) return;

        // // the message text starts 32 px in, so the banner spans that column
        // auto banner = comment::createBannerSprite(accountId,
        //     comment::BannerVariant::Compact,
        //     comment::BannerSource::Server,
        //     CCSize{std::max(1.f, size.width - 36.f), std::max(1.f, size.height - 6.f)});
        // if (!banner) return;

        // banner->setID("cb-thread-banner-node");
        // banner->setPosition({size.width * 0.5f + 8.f, size.height * 0.5f});
        // row->addChild(banner, -1);
    }
};

static CCSize commentBannerSize(CommentCell* cell, bool useNonCompact) {
    if (!cell) return CCSizeZero;

    auto size = cell->m_backgroundLayer ? cell->m_backgroundLayer->getScaledContentSize() : CCSizeZero;
    if (size.width <= 0.f || size.height <= 0.f) size = cell->m_backgroundLayer->getContentSize();
    if (size.width <= 0.f || size.height <= 0.f) size = cell->getContentSize();

    // non compact banners are a full size image, compact ones fill the comment background
    if (!useNonCompact && !cell->m_compactMode) return CCSize{800.f, 800.f};
    return size;
}

// mounts an already known banner source (remote url or local file) in the comment background
static void applyCommentBanner(CommentCell* cell, std::string const& source, bool useNonCompact) {
    if (!cell || !cell->m_backgroundLayer || source.empty()) return;

    auto size = commentBannerSize(cell, useNonCompact);
    if (size.width <= 0.f || size.height <= 0.f) return;

    auto banner = comment::createBannerSpriteFromSource(source, size);
    if (!banner) return;

    banner->setID("cb-comment-banner-node");
    banner->setPosition({size.width / 2.f, size.height / 2.f});
    cell->m_backgroundLayer->setOpacity(100);
    cell->m_backgroundLayer->addChild(banner, -1);
}

static void setupCommentSprite(CommentCell* cell, int accountId, comment::BannerSource source) {
    Ref<CommentCell> self = cell;
    if (!self || !self->m_backgroundLayer || accountId <= 0) return;

    comment::requestBannerInfo(accountId, source, [self](comment::BannerInfo const& info) {
        if (!self || !self->m_backgroundLayer) return;

        self->m_backgroundLayer->setOpacity(255);
        if (auto prevBanner = self->m_backgroundLayer->getChildByID("cb-comment-banner-node")) {
            prevBanner->removeFromParent();
        }
        if (!info.equipped || info.imageUrl.empty()) return;

        bool useNonCompact = !self->m_compactMode && !info.nonCompactImageUrl.empty();
        applyCommentBanner(self.data(), info.urlFor(useNonCompact ? comment::BannerVariant::NonCompact : comment::BannerVariant::Compact), useNonCompact);

        if (self->m_compactMode && self->m_mainLayer) {
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
}

class $modify(CBCommentCell, CommentCell) {
    void loadFromComment(GJComment* comment) {
        CommentCell::loadFromComment(comment);
        if (!m_backgroundLayer || !comment) return;

        // cells are recycled, so drop whatever the previous comment left behind
        m_backgroundLayer->setOpacity(255);
        if (auto prevBanner = m_backgroundLayer->getChildByID("cb-comment-banner-node")) {
            prevBanner->removeFromParent();
        }
        if (m_mainLayer) {
            if (auto bg = m_mainLayer->getChildByID("cb-comment-banner-bg")) bg->removeFromParent();
        }

        int accountId = comment->m_accountID;
        if (accountId <= 0) return;
        if (m_accountComment && Mod::get()->getSettingValue<bool>("hide-account-comment")) return;

        // the own banner may be a local file that is not uploaded yet
        int myAccountId = GJAccountManager::sharedState()->m_accountID;
        if (myAccountId == 0) myAccountId = argon::getGameAccountData().accountId;
        if (myAccountId > 0 && accountId == myAccountId) {
            auto equippedLocal = Mod::get()->getSavedValue<std::string>("equipped-local-banner", "");
            if (!equippedLocal.empty()) {
                auto localPath = comment::local::getLocalBannersDir() / equippedLocal;
                if (localPath.exists()) {
                    applyCommentBanner(this, localPath.string(), false);
                    return;
                }
            }
        }

        setupCommentSprite(this, accountId, comment::BannerSource::Server);
    }
};

class $modify(CBPlayLayer, PlayLayer) {
    struct Fields {
        bool m_wasCompletedBefore = false;
    };

    bool init(GJGameLevel* level, bool dontCreateObjects) {
        if (!PlayLayer::init(level, dontCreateObjects)) return false;

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
                auto request = utils::web::WebRequest();
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

                    auto amethystLabel = Label::create(
                        fmt::format("+{}", amethystReward).c_str(), "bigFont.fnt");
                    amethystLabel->setPosition({-5, amethystPop->getContentSize().height / 2});
                    amethystLabel->setAnchorPoint({1.0f, 0.5f});
                    amethystLabel->setLimitLabelWidth(80, 1.f, 0.5f);
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
                        auto burstPos = endLayerRef->convertToNodeSpace(
                            endLayerRef->m_mainLayer->convertToWorldSpace(amethystPop->getPosition()));
                        auto gdRewardLayer = CBAmethystRewardLayer::findGDRewardLayer(CCDirector::sharedDirector()->getRunningScene());
                        if (auto rewardLayer = CBAmethystRewardLayer::create(current - amethystReward, amethystReward, burstPos, gdRewardLayer)) {
                            rewardLayer->setID("cb-amethyst-reward-layer");
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
