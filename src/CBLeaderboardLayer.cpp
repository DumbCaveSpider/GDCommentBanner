
#include "CBLeaderboardLayer.hpp"
#include <fmt/format.h>
#include "CBProfileBannerPopup.hpp"
#include "include/CBConstant.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/ui/Scrollbar.hpp>
#include <Geode/ui/Button.hpp>
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
    {LeaderboardTab::Sales, "Top Sales", "sales", "No sales found.",
        "<cy>Top Sales</c> shows the users whose <cg>banners</c> have been <cl>purchased</c> the most times."},
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
    addSideArt(this, SideArt::All);

    m_list = cue::ListNode::create({356.f, 220}, {0, 0, 0, 0}, cue::ListBorderStyle::SlimLevels);
    if (m_list) {
        m_list->setZOrder(2);
        this->addChildAtPosition(m_list, Anchor::Center, CCPointZero, false);

        auto scrollbar = Scrollbar::create(m_list->getScrollLayer());
        scrollbar->setZOrder(5);
        this->addChildAtPosition(scrollbar, Anchor::Center, {356.f / 2 + 20.f, 0.f}, false);
    }

    // tabs
    

    createTabs();

    if (auto infoButton = geode::Button::createWithNode(CircleButtonSprite::createWithSpriteFrameName("CB_info.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small), [this](geode::Button* sender) {
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
        auto req = geode::utils::web::WebRequest();
        auto response = co_await req.get(fmt::format("{}/getLeaderboard?type={}", comment::baseUrl, getTabInfo(tab).type));

        // ignore responses for a tab the user already switched away from
        auto isStale = [retainedSelf, tab] {
            return !retainedSelf || !retainedSelf->getParent() || retainedSelf->m_currentTab != tab;
        };

        if (!response.ok()) {
            geode::queueInMainThread([retainedSelf, isStale] {
                if (isStale()) return;
                if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Failed to fetch leaderboard", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto jsonRes = response.json();
        if (jsonRes.isErr()) {
            geode::queueInMainThread([retainedSelf, isStale] {
                if (isStale()) return;
                if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Invalid JSON response", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isArray()) {
            geode::queueInMainThread([retainedSelf, isStale] {
                if (isStale()) return;
                if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        geode::queueInMainThread([retainedSelf, isStale, tab, json = std::move(json)] {
            if (isStale()) return;
            if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
            if (retainedSelf->m_list) retainedSelf->m_list->clear();

            if (json.size() == 0) {
                auto emptyLabel = CCLabelBMFont::create(getTabInfo(tab).emptyText, "bigFont.fnt");
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
                std::string equippedBannerUrl = item["equippedBannerUrl"].asString().unwrapOr("");
                bool isAdmin = item["is_admin"].asBool().unwrapOr(false);
                bool isStaff = item["is_staff"].asBool().unwrapOr(false);
                bool isSupporter = item["is_supporter"].asBool().unwrapOr(false);

                auto cell = CCNode::create();
                cell->setContentSize({356.f, 40.f});

                if (!equippedBannerUrl.empty()) {
                    if (!Mod::get()->getSettingValue<bool>("disable-banner-leaderboard")) {
                        auto bannerSprite = comment::createBannerNode(fmt::format("{}{}", comment::baseUrl, equippedBannerUrl), {cell->getContentWidth() + 10, cell->getContentHeight() + 10});
                        if (bannerSprite) {
                            bannerSprite->setPosition({cell->getContentWidth() / 2, cell->getContentHeight() / 2});
                            cell->addChild(bannerSprite, -2);
                        }
                    }
                }

                // Rank
                auto rankLabel = CCLabelBMFont::create(fmt::format("{}", i + 1).c_str(), "goldFont.fnt");
                rankLabel->setScale(0.6f);
                rankLabel->setAnchorPoint({0.f, 0.5f});
                rankLabel->setPosition({10.f, 20.f});
                cell->addChild(rankLabel);

                // Badges
                auto badgesNode = CCNode::create();
                badgesNode->setPosition({45.f, 20.f});
                badgesNode->setContentSize({90.f, 25.f});
                badgesNode->setAnchorPoint({0.f, 0.5f});
                badgesNode->setLayout(RowLayout::create()->setAxisAlignment(AxisAlignment::Start)->setGap(5)->setAutoScale(false));
                cell->addChild(badgesNode, 1);

                float badgesWidth = 0.f;
                if (isAdmin) {
                    if (auto adminBadge = CCSprite::createWithSpriteFrameName("CB_admin_badge.png"_spr)) {
                        adminBadge->setScale(0.07f);
                        adminBadge->setAnchorPoint({0.f, 0.5f});
                        adminBadge->setPosition({badgesWidth, 0.f});
                        badgesNode->addChild(adminBadge);
                        badgesWidth += adminBadge->getScaledContentSize().width + 4.f;
                    }
                } else if (isStaff) {
                    if (auto staffBadge = CCSprite::createWithSpriteFrameName("CB_staff_badge.png"_spr)) {
                        staffBadge->setScale(0.07f);
                        staffBadge->setAnchorPoint({0.f, 0.5f});
                        staffBadge->setPosition({badgesWidth, 0.f});
                        badgesNode->addChild(staffBadge);
                        badgesWidth += staffBadge->getScaledContentSize().width + 4.f;
                    }
                }
                // will add later
                if (isSupporter) {
                    //if (auto supporterBadge = CCSprite::createWithSpriteFrameName("CB_supporter_badge.png"_spr)) {
                    //    supporterBadge->setScale(0.65f);
                    //    supporterBadge->setAnchorPoint({0.f, 0.5f});
                    //    supporterBadge->setPosition({badgesWidth, 0.f});
                    //    badgesNode->addChild(supporterBadge);
                    //    badgesWidth += supporterBadge->getScaledContentSize().width + 4.f;
                    //}
                }
                badgesNode->setContentSize({badgesWidth, 0.f});
                badgesNode->updateLayout();

                float currentX = 45.f + badgesWidth;

                // Username
                auto menu = CCMenu::create();
                menu->setPosition({0, 0});
                cell->addChild(menu);

                auto usernameLabel = CCLabelBMFont::create(username.c_str(), "bigFont.fnt");
                usernameLabel->limitLabelWidth(std::max(60.f, 165.f - currentX), 0.6f, 0.1f);

                auto usernameBtn = geode::Button::createWithNode(
                    usernameLabel,
                    [accountId, username](geode::Button* sender) {
                        if (auto popup = CBProfileBannerPopup::create(accountId, username)) {
                            popup->show();
                        }
                    });

                usernameBtn->setAnchorPoint({0.f, 0.5f});
                usernameBtn->setPosition({currentX, 20.f});
                menu->addChild(usernameBtn);

                // Main stat for the current tab, with a secondary stat underneath
                std::string mainText;
                std::string subText;
                bool showAmethyst = true;
                switch (tab) {
                    case LeaderboardTab::Earners:
                        mainText = GameToolbox::pointsToString(totalEarned);
                        subText = fmt::format("Total Sales: {}", totalSales);
                        break;
                    case LeaderboardTab::Sales:
                        mainText = fmt::format("{} Sales", GameToolbox::pointsToString(totalSales));
                        subText = fmt::format("Total Earned: {}", GameToolbox::pointsToString(totalEarned));
                        showAmethyst = false;
                        break;
                }

                auto earnLabel = CCLabelBMFont::create(mainText.c_str(), "bigFont.fnt");
                earnLabel->limitLabelWidth(150, 0.5f, 0.1f);
                earnLabel->setAnchorPoint({1.f, 0.5f});
                earnLabel->setPosition({showAmethyst ? 320.f : 345.f, 26.f});
                cell->addChild(earnLabel);

                if (showAmethyst) {
                    auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr);
                    amethystIcon->setScale(0.6f);
                    amethystIcon->setPosition({335.f, 26.f});
                    cell->addChild(amethystIcon);
                }

                auto salesLabel = CCLabelBMFont::create(subText.c_str(), "bigFont.fnt");
                salesLabel->limitLabelWidth(150, 0.25f, 0.1f);
                salesLabel->setAnchorPoint({1.f, 0.5f});
                salesLabel->setPosition({345.f, 10.f});
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
