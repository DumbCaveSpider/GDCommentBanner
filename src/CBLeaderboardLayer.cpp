#include "CBLeaderboardLayer.hpp"
#include "CBProfileBannerPopup.hpp"
#include "include/CBConstant.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/ui/Scrollbar.hpp>
#include <Geode/ui/Button.hpp>

CBLeaderboardLayer* CBLeaderboardLayer::create() {
    auto ret = new CBLeaderboardLayer();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBLeaderboardLayer::init() {
    if (!CCLayer::init()) return false;

    this->setKeypadEnabled(true);

    auto winSize = CCDirector::sharedDirector()->getWinSize();

    m_background = createLayerBG();
    m_background->setColor({154, 108, 217});
    this->addChild(m_background, -5);

    addBackButton(this, BackButtonStyle::Pink);
    addSideArt(this, SideArt::All);

    m_list = cue::ListNode::create({356.f, 220}, {0, 0, 0, 0}, cue::ListBorderStyle::Levels);
    if (m_list) {
        m_list->setZOrder(2);
        this->addChildAtPosition(m_list, Anchor::Center, CCPointZero, false);

        auto scrollbar = Scrollbar::create(m_list->getScrollLayer());
        scrollbar->setZOrder(5);
        this->addChildAtPosition(scrollbar, Anchor::Center, {356.f / 2 + 25.f, 0.f}, false);
    }

    auto title = CCLabelBMFont::create("Top Earners", "bigFont.fnt");
    title->setPosition({m_list->getContentSize().width / 2, m_list->getContentSize().height + 19.f});
    title->setScale(0.8f);
    m_list->addChild(title, 5);

    if (auto infoButton = geode::Button::createWithSpriteFrameName("GJ_infoIcon_001.png", [this](geode::Button* sender) {
            FLAlertLayer::create(
                "Amethysts Top Earners",
                "<cy>Top Earners</c> only shows the total <cp>Amethysts</c> earned from users purchasing <cg>banners</c> these users created with <co>taxes</c>.\n"
                "<cy>This leaderboard does not include the amethysts earned from beating rated levels</c>",
                "OK")
                ->show();
        })) {
        this->addChildAtPosition(infoButton, Anchor::BottomLeft, {25.f, 25.f}, false);
    }

    fetchLeaderboard();

    return true;
}

void CBLeaderboardLayer::keyBackClicked() {
    CCDirector::sharedDirector()->popSceneWithTransition(0.5f, PopTransition::kPopTransitionFade);
}

void CBLeaderboardLayer::onBack(CCObject* sender) {
    keyBackClicked();
}

void CBLeaderboardLayer::fetchLeaderboard() {
    if (m_loadingCircle) m_loadingCircle->removeFromParent();
    m_loadingCircle = cue::LoadingCircle::create(true);
    if (m_list) {
        m_loadingCircle->addToLayer(m_list, 10);
    }
    m_loadingCircle->fadeIn();

    Ref<CBLeaderboardLayer> retainedSelf = this;
    arc::spawn([retainedSelf]() -> arc::Future<> {
        auto req = geode::utils::web::WebRequest();
        auto response = co_await req.get(fmt::format("{}/getLeaderboard", comment::baseUrl));

        if (!response.ok()) {
            geode::queueInMainThread([retainedSelf] {
                if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Failed to fetch leaderboard", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto jsonRes = response.json();
        if (jsonRes.isErr()) {
            geode::queueInMainThread([retainedSelf] {
                if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Invalid JSON response", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isArray()) {
            geode::queueInMainThread([retainedSelf] {
                if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        geode::queueInMainThread([retainedSelf, json = std::move(json)] {
            if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
            if (retainedSelf->m_list) retainedSelf->m_list->clear();

            if (json.size() == 0) {
                auto emptyLabel = CCLabelBMFont::create("No earners found.", "bigFont.fnt");
                emptyLabel->setScale(0.5f);
                if (retainedSelf->m_list) {
                    retainedSelf->m_list->addChildAtPosition(emptyLabel, Anchor::Center);
                }
                return;
            }

            for (size_t i = 0; i < json.size(); ++i) {
                auto item = json[i];
                int accountId = item["accountId"].asInt().unwrapOr(0);
                std::string username = item["username"].asString().unwrapOr("Unknown");
                int totalEarned = item["totalEarned"].asInt().unwrapOr(0);
                int totalSales = item["totalSales"].asInt().unwrapOr(0);

                auto cell = CCNode::create();
                cell->setContentSize({356.f, 40.f});

                // Rank
                auto rankLabel = CCLabelBMFont::create(fmt::format("{}", i + 1).c_str(), "goldFont.fnt");
                rankLabel->setScale(0.6f);
                rankLabel->setAnchorPoint({0.f, 0.5f});
                rankLabel->setPosition({10.f, 20.f});
                cell->addChild(rankLabel);

                // Username
                auto menu = CCMenu::create();
                menu->setPosition({0, 0});
                cell->addChild(menu);

                auto usernameLabel = CCLabelBMFont::create(username.c_str(), "bigFont.fnt");
                usernameLabel->limitLabelWidth(120.f, 0.6f, 0.1f);

                auto usernameBtn = geode::Button::createWithNode(
                    usernameLabel,
                    [accountId, username](geode::Button* sender) {
                        if (auto popup = CBProfileBannerPopup::create(accountId, username)) {
                            popup->show();
                        }
                    });

                usernameBtn->setAnchorPoint({0.f, 0.5f});
                usernameBtn->setPosition({45.f, 20.f});
                menu->addChild(usernameBtn);

                // Earned
                auto earnLabel = CCLabelBMFont::create(GameToolbox::pointsToString(totalEarned).c_str(), "bigFont.fnt");
                earnLabel->limitLabelWidth(150, 0.5f, 0.1f);
                earnLabel->setAnchorPoint({1.f, 0.5f});
                earnLabel->setPosition({320.f, 26.f});
                cell->addChild(earnLabel);

                auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr);
                amethystIcon->setScale(0.6f);
                amethystIcon->setPosition({335.f, 26.f});
                cell->addChild(amethystIcon);

                // Sales
                auto salesLabel = CCLabelBMFont::create(fmt::format("Total Sales: {}", totalSales).c_str(), "bigFont.fnt");
                salesLabel->limitLabelWidth(150, 0.25f, 0.1f);
                salesLabel->setAnchorPoint({1.f, 0.5f});
                salesLabel->setPosition({320.f, 10.f});
                salesLabel->setColor({150, 255, 150});
                cell->addChild(salesLabel);

                if (retainedSelf->m_list) {
                    retainedSelf->m_list->addCell(cell);
                }
            }

            if (retainedSelf->m_list) {
                retainedSelf->m_list->updateLayout();
            }
        });

        co_return;
    });
}
