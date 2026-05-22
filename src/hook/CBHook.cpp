#include <Geode/Geode.hpp>
#include <Geode/ui/Button.hpp>
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
    }
};

class $modify(CBEndLevelLayer, EndLevelLayer) {
    void customSetup() {
        EndLevelLayer::customSetup();

        auto endLayer = Ref<EndLevelLayer>(this);
        auto orb = CCSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr);
        if (orb) {
            orb->setScale(0.7f);
            m_listLayer->addChild(orb);
        }

        geode::queueInMainThread([endLayer]() {
            auto accountId = argon::getGameAccountData().accountId;
            if (accountId <= 0) {
                Notification::create("Invalid account ID for amethyst reward submission.", NotificationIcon::Error)->show();
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

            arc::spawn([endLayer, accountId, levelId]() -> arc::Future<> {
                auto authResult = co_await argon::startAuth();
                if (authResult.isErr()) {
                    log::warn("argon auth failed for amethyst reward: {}", authResult.unwrapErr());
                    co_return;
                }

                auto authToken = std::move(authResult).unwrap();
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
                    Notification::create(fmt::format("Awarded {} amethyst", amethystReward), CCSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr))->show();
                });

                log::debug("awarded {} amethyst from submitAmethystReward", amethystReward);
                co_return;
            });
        });
    }
};