#include <Geode/Geode.hpp>
#include <Geode/ui/Button.hpp>
#include "Geode/ui/General.hpp"
#include "CBShopLayer.hpp"
#include "Geode/utils/web.hpp"
#include "include/CBConstant.hpp"
#include <argon/argon.hpp>
#include <cue/ListNode.hpp>

using namespace geode::prelude;

CBShopLayer* CBShopLayer::create() {
    auto ret = new CBShopLayer();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBShopLayer::init() {
    if (!CCLayer::init()) return false;

    m_background = createLayerBG();
    m_background->setColor({154, 108, 217});
    this->addChild(m_background);
    addBackButton(this, BackButtonStyle::Pink);

    this->setKeypadEnabled(true);

    auto authButton = geode::Button::createWithNode(
        CircleButtonSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small),
        [](geode::Button* button) {
            arc::spawn([]() -> arc::Future<> {
                auto authResult = co_await argon::startAuth();
                if (authResult.isErr()) {
                    log::warn("(CommentBanner) argon::startAuth failed: {}", authResult.unwrapErr());
                    co_return;
                }

                auto authToken = std::move(authResult).unwrap();
                auto accountId = argon::getGameAccountData().accountId;
                if (accountId <= 0) {
                    log::warn("(CommentBanner) invalid accountId from game account data");
                    co_return;
                }

                auto url = fmt::format(
                    "{}?accountId={}&argonToken={}",
                    comment::baseUrl,
                    accountId,
                    authToken
                );
                utils::web::openLinkInBrowser(url.c_str());
                co_return;
            });
        });

    if (authButton) {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        authButton->setPosition({ winSize.width * 0.5f, winSize.height * 0.5f });
        this->addChild(authButton);
    }

    return true;
}

void CBShopLayer::keyBackClicked() {
    CCDirector::sharedDirector()->popSceneWithTransition(0.5f, PopTransition::kPopTransitionMoveInT);
}