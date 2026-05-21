#include <Geode/Geode.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/modify/GJGarageLayer.hpp>
#include <Geode/modify/CommentCell.hpp>
#include <Geode/modify/EndLevelLayer.hpp>
#include "../CBShopLayer.hpp"

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
    void playCurrencyEffect(float duration) {
        EndLevelLayer::playCurrencyEffect(duration);
        if (auto orbContainer = this->getChildByID("orb-container")) {
            auto orb = CCSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr);
            if (orb) {
                orb->setScale(0.7f);
                orbContainer->addChild(orb);
            }
        }
        int earned = Mod::get()->getSavedValue<int>("amethyst", 0) + m_orbs;
        log::debug("earned {} amethyst, total {}", m_orbs, earned);
        Mod::get()->setSavedValue("amethyst", earned);
    }
};