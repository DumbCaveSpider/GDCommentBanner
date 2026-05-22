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

class $modify(GJGarageLayer) {
    bool init() {
        if (!GJGarageLayer::init())
            return false;
        if (auto shardMenu = this->getChildByID("shards-menu")) {
            auto shopButton = Button::createWithNode(CircleButtonSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small), [](geode::Button* button) {
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
                co_return;
            }

            auto imageUrlRes = json["imageUrl"].asString();
            if (imageUrlRes.isErr()) {
                log::debug("getImageBanner missing imageUrl");
                co_return;
            }

            auto imageUrl = imageUrlRes.unwrap();
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
                Notification::create("Invalid account ID for amethyst reward submission.", NotificationIcon::Error)->show();
                return;
            }

            if (endLayer->m_playLayer->m_level->m_stars == 0) {
                log::debug("unrated level completion, skipping amethyst reward submission");
                return;
            }

            if (!endLayer->m_playLayer || !endLayer->m_playLayer->m_level) {
                Notification::create("Unable to determine the completed level.", NotificationIcon::Error)->show();
                return;
            }

            auto level = endLayer->m_playLayer->m_level;
            if (!level->m_isCompletionLegitimate) {
                Notification::create("Level completion is not legitimate for amethyst reward submission.", NotificationIcon::Error)->show();
                log::warn("illegitimate level completion for amethyst reward submission");
                return;
            }

            if (level->m_attemptTime <= 25 || level->m_attemptTime >= 28000000) {
                Notification::create("Attempt time is invalid for amethyst reward submission.", NotificationIcon::Error)->show();
                log::warn("invalid attempt time for amethyst reward submission: {}", level->m_attemptTime);
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