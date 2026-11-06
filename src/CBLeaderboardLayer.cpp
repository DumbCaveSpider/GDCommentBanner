
#include "CBLeaderboardLayer.hpp"
#include "CBUserCell.hpp"
#include <fmt/format.h>
#include "CBProfileBannerPopup.hpp"
#include "include/CBConstant.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/ui/Scrollbar.hpp>
#include <Geode/ui/Button.hpp>
#include <cue/Colors.hpp>
#include <cue/RepeatingBackground.hpp>

struct LeaderboardTabInfo {
    LeaderboardTab tab;
    char const* name;
    char const* type;  // ?type= query param for /getLeaderboard
    char const* emptyText;
    char const* infoText;
};

static constexpr LeaderboardTabInfo kTabs[] = {
    {LeaderboardTab::Earners, "Top Earners", "earners", "No earners found.",
        "<cy>Top Earners</c> only shows the total <cp>Amethysts</c> earned from users purchasing <cg>banners</c> these users created with <co>taxes</c>.\n"
        "<cy>This leaderboard does not include the amethysts earned from beating rated levels</c>"},
    {LeaderboardTab::Sales, "Top Sales", "sales", "No sales found.", "<cy>Top Sales</c> shows the users whose <cg>banners</c> have been <cl>purchased</c> the most times."},
    {LeaderboardTab::Rate, "Top Rate", "rate", "No earning rates found.",
        "<cy>Top Rate</c> ranks users by how many <cp>Amethysts</c> their <cg>banners</c> will earn in the next <cl>monthly payout</c>.\n"
        "Each banner earns <cg>5%</c> of its price once equipped, <cg>+5%</c> for every <cy>5 users</c> equipping it, up to <cg>30%</c>."},
};

static LeaderboardTabInfo const& getTabInfo(LeaderboardTab tab) {
    for (auto const& info : kTabs) {
        if (info.tab == tab) return info;
    }
    return kTabs[0];
}

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

    if (!Mod::get()->getSettingValue<bool>("disable-animated-background")) {
        m_background = cue::RepeatingBackground::create("game_bg_57_001.png", 1.f, cue::RepeatMode::X);
        m_background->setColor({154, 108, 217});
        this->addChild(m_background);
    } else {
        m_background = createLayerBG();
        m_background->setColor({154, 108, 217});
        this->addChild(m_background);
    }

    addBackButton(this, BackButtonStyle::Pink);
    // auto backBtn = Button::createWithSpriteFrameName("GJ_arrow_03_001.png", [this](Button*) { this->keyBackClicked(); });
    // backBtn->setZOrder(CCScene::get()->getHighestChildZ() + 1);
    // this->addChildAtPosition(backBtn, Anchor::TopLeft, {25.f, -25.f}, false);
    addSideArt(this, SideArt::All);

    m_list = cue::ListNode::create({356.f, 220}, cue::Brown, cue::ListBorderStyle::SlimLevels);
    if (m_list) {
        m_list->setZOrder(2);
        this->addChildAtPosition(m_list, Anchor::Center, CCPointZero, false);

        auto scrollbar = Scrollbar::create(m_list->getScrollLayer());
        scrollbar->setZOrder(5);
        this->addChildAtPosition(scrollbar, Anchor::Center, {356.f / 2 + 20.f, 0.f}, false);
    }

    // tabs

    createTabs();

    if (auto infoButton = Button::createWithNode(CircleButtonSprite::createWithSpriteFrameName("CB_info.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small), [this](Button* sender) {
            auto const& info = getTabInfo(m_currentTab);
            FLAlertLayer::create(fmt::format("Amethysts {}", info.name).c_str(), info.infoText, "OK")->show();
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

void CBLeaderboardLayer::createTabs() {
    if (!m_list) return;

    auto tabMenu = CCMenu::create();
    tabMenu->setLayout(RowLayout::create()->setGap(10.f)->setAxisAlignment(AxisAlignment::Center));

    m_tabs.clear();
    for (auto const& info : kTabs) {
        auto tab = TabButton::create(TabBaseColor::Unselected, TabBaseColor::UnselectedDark, info.name, this, menu_selector(CBLeaderboardLayer::onTabSelected));
        if (!tab) continue;
        tab->setTag(static_cast<int>(info.tab));
        // selection state is handled by updateTabSelection, not by the toggler itself
        tab->setClickable(false);
        tabMenu->addChild(tab);
        m_tabs.push_back(tab);
    }

    updateTabSelection();
    tabMenu->setZOrder(-1);
    tabMenu->setContentWidth(m_list->getContentWidth());
    tabMenu->updateLayout();
    m_list->addChildAtPosition(tabMenu, Anchor::Top, {0.f, 10.f}, {0.5f, 0.f});
}

void CBLeaderboardLayer::updateTabSelection() {
    for (auto& tab : m_tabs) {
        tab->toggle(tab->getTag() == static_cast<int>(m_currentTab));
    }
}

void CBLeaderboardLayer::onTabSelected(CCObject* sender) {
    auto tab = static_cast<LeaderboardTab>(sender->getTag());
    if (tab == m_currentTab) return;

    m_currentTab = tab;
    updateTabSelection();
    fetchLeaderboard();
}

void CBLeaderboardLayer::fetchLeaderboard() {
    if (m_loadingCircle) m_loadingCircle->removeFromParent();
    m_loadingCircle = cue::LoadingCircle::create(true);
    if (m_list) {
        m_list->clear();
        m_loadingCircle->addToLayer(m_list, 10);
    }
    m_loadingCircle->fadeIn();

    Ref<CBLeaderboardLayer> retainedSelf = this;
    auto tab = m_currentTab;
    arc::spawn([retainedSelf, tab]() -> arc::Future<> {
        auto req = utils::web::WebRequest();
        auto response = co_await req.get(fmt::format("{}/getLeaderboard?type={}", comment::baseUrl, getTabInfo(tab).type));

        // ignore responses for a tab the user already switched away from
        auto isStale = [retainedSelf, tab] {
            return !retainedSelf || !retainedSelf->getParent() || retainedSelf->m_currentTab != tab;
        };

        if (!response.ok()) {
            queueInMainThread([retainedSelf, isStale] {
                if (isStale()) return;
                if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Failed to fetch leaderboard", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto jsonRes = response.json();
        if (jsonRes.isErr()) {
            queueInMainThread([retainedSelf, isStale] {
                if (isStale()) return;
                if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Invalid JSON response", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isArray()) {
            queueInMainThread([retainedSelf, isStale] {
                if (isStale()) return;
                if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        queueInMainThread([retainedSelf, isStale, tab, json = std::move(json)] {
            if (isStale()) return;
            if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
            if (retainedSelf->m_list) retainedSelf->m_list->clear();

            if (json.size() == 0) {
                auto emptyLabel = Label::create(getTabInfo(tab).emptyText, "bigFont.fnt");
                emptyLabel->setScale(0.5f);
                if (retainedSelf->m_list) {
                    retainedSelf->m_list->addChildAtPosition(emptyLabel, Anchor::Center);
                }
                return;
            }

            bool showBanner = !Mod::get()->getSettingValue<bool>("disable-banner-leaderboard");
            for (size_t i = 0; i < json.size(); ++i) {
                auto data = CBUserData::fromJson(json[i]);
                // leaderboard banner urls are relative to the server
                if (!data.equippedBannerUrl.empty() && data.equippedBannerUrl.starts_with("/")) {
                    data.equippedBannerUrl = fmt::format("{}{}", comment::baseUrl, data.equippedBannerUrl);
                }

                auto cell = CBUserCell::create(data, 356.f, {
                                                                .rank = static_cast<int>(i + 1),
                                                                .showBanner = showBanner,
                                                                .showSalesCount = tab == LeaderboardTab::Sales,
                                                            });
                if (cell && retainedSelf->m_list) {
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
