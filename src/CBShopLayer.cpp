#include <Geode/Enums.hpp>
#include <Geode/Geode.hpp>
#include <Geode/binding/CCCounterLabel.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/ui/Button.hpp>
#include "CBShopLayer.hpp"
#include <Geode/ui/Label.hpp>
#include "Geode/ui/TextArea.hpp"
#include "include/CBLocalBanner.hpp"
#include "CBBannerCell.hpp"
#include "CBSubmitBannerPopup.hpp"
#include "CBLogsPopup.hpp"
#include "CBAdminPanelLayer.hpp"
#include "CBYourBannersPopup.hpp"
#include "CBLocalBannersPopup.hpp"
#include "CBLeaderboardLayer.hpp"
#include "CBBannerFilterPopup.hpp"
#include <Geode/binding/SetTextPopup.hpp>
#include "Geode/ui/Layout.hpp"
#include "Geode/utils/web.hpp"
#include "ccTypes.h"
#include "include/CBConstant.hpp"
#include <dankmeme.argon/argon.hpp>
#include <cue/LoadingCircle.hpp>
#include <cue/ListBorder.hpp>
#include <cue/ListNode.hpp>
#include <Geode/ui/Scrollbar.hpp>
#include <cue/RepeatingBackground.hpp>
#include "CBSupporterPopup.hpp"

using namespace geode::prelude;

CBShopLayer* CBShopLayer::s_instance = nullptr;

CBShopLayer* CBShopLayer::getInstance() {
    return s_instance;
}

void CBShopLayer::refreshBanners() {
    if (!this->getParent() || s_instance != this) {
        return;
    }
    if (m_loadingCircle) {
        m_loadingCircle->fadeIn();
    }
    this->updateLocalEquippedState();
    this->fetchBanners();
}

void CBShopLayer::setEquippedBannerId(int bannerId) {
    m_equippedBannerId = bannerId;
}

void CBShopLayer::updateLocalEquippedState() {
    bool isLocalEquipped = !comment::local::getEquippedLocalBannerUrl().empty();
    if (m_localOverlay) {
        m_localOverlay->setVisible(isLocalEquipped);
    }
    if (m_list) {
        m_list->setVisible(!isLocalEquipped);
    }
    if (m_noBannersLabel && isLocalEquipped) {
        m_noBannersLabel->setVisible(false);
    } else if (m_noBannersLabel && !isLocalEquipped) {
        m_noBannersLabel->setVisible(m_banners.empty());
    }
    if (m_filterSortLabel) {
        m_filterSortLabel->setVisible(!isLocalEquipped);
    }
    if (auto paginationMenu = this->getChildByID("pagination-menu")) {
        paginationMenu->setVisible(!isLocalEquipped);
    }
    if (m_pageLabel) {
        m_pageLabel->setVisible(!isLocalEquipped);
    }
}

void CBShopLayer::updateFilterSortLabel() {
    if (!m_filterSortLabel) return;

    // every selected filter, e.g. "Owned Limited"
    static constexpr char const* kFilterNames[] = {"", "Owned", "Unowned", "Limited", "Featured", "Favorite"};
    std::vector<std::string> filterNames;
    for (int i = 1; i < 6; ++i) {
        if (m_filterState & (1 << i)) filterNames.emplace_back(kFilterNames[i]);
    }
    std::string filterName = filterNames.empty() ? "All" : fmt::format("{}", fmt::join(filterNames, " "));

    std::string sortName;
    switch (m_sortState) {
        case 1: sortName = "Most Bought"; break;
        case 2: sortName = "Most Equipped"; break;
        case 3: sortName = "Highest Price"; break;
        case 4: sortName = "Lowest Price"; break;
        default: sortName = "Recent"; break;
    }

    std::string text;
    if (m_filterState == 0 && m_sortState == 0) {
        text = "All Banners";
    } else if (m_sortState == 0) {
        text = fmt::format("{} Banners", filterName);
    } else if (m_filterState == 0) {
        text = fmt::format("All Banners ({})", sortName);
    } else {
        text = fmt::format("{} Banners ({})", filterName, sortName);
    }

    if (m_supporterOnlyFilter) {
        text = "Supporter Only " + text;
    }

    if (!m_searchQuery.empty()) {
        text += fmt::format(" - \"{}\"", m_searchQuery);
    }

    m_filterSortLabel->setString(text.c_str());
    m_filterSortLabel->setLimitLabelWidth(300.f, 0.6f, 0.2f);
}

void CBShopLayer::onEnter() {
    CCLayer::onEnter();
    s_instance = this;
}

bool CBShopLayer::isStaleFetch(int fetchId) {
    return !this->getParent() || s_instance != this || m_fetchId != fetchId;
}

void CBShopLayer::onExit() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
    CCLayer::onExit();
}

CBShopLayer::~CBShopLayer() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void CBShopLayer::updateAmethystLabel(int amethyst) {
    if (!m_amethystLabel || !this->getParent() || s_instance != this) {
        return;
    }
    m_amethystLabel->setTargetCount(amethyst);
    m_amethystLabel->updateCounter(0.f);
}

void CBShopLayer::fetchBanners() {
    Ref<CBShopLayer> self = this;
    queueInMainThread([self]() {
        if (!self || !self->getParent() || s_instance != self.data()) {
            return;
        }

        if (self->m_list) {
            self->m_list->clear();
        }
        if (self->m_noBannersLabel) {
            self->m_noBannersLabel->setVisible(false);
        }
        if (!self->m_loadingCircle && self->m_list) {
            self->m_loadingCircle = cue::LoadingCircle::create(true);
            if (self->m_loadingCircle) {
                self->m_loadingCircle->addToLayer(self->m_list, 10);
            }
        }
        if (self->m_loadingCircle) {
            self->m_loadingCircle->fadeIn();
        }

        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;
        if (accountId <= 0) {
            if (self->m_loadingCircle) {
                self->m_loadingCircle->fadeOut();
            }
            Notification::create("Failed to retrieve a valid account ID.", NotificationIcon::Error)->show();
            return;
        }

        int currentPage = self->m_currentPage;
        int itemsPerPage = self->m_itemsPerPage;
        int filterState = self->m_filterState;
        int sortState = self->m_sortState;
        // only supporters can filter for supporter-only banners
        bool supporterOnlyFilter = self->m_supporterOnlyFilter && comment::isUserSupporter();
        std::string searchQuery = self->m_searchQuery;
        int equippedBannerId = self->m_equippedBannerId;
        int fetchId = ++self->m_fetchId;

        arc::spawn([self, fetchId, accountId, accountData, currentPage, itemsPerPage, filterState, sortState, supporterOnlyFilter, searchQuery, equippedBannerId]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                queueInMainThread([self, fetchId]() {
                    if (!self || self->isStaleFetch(fetchId)) {
                        return;
                    }
                    if (self->m_loadingCircle) {
                        self->m_loadingCircle->fadeOut();
                    }
                    Notification::create("Failed to authenticate.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto authToken = std::move(authResult);
            auto req = utils::web::WebRequest();
            auto body = matjson::makeObject({{"accountId", accountId},
                {"argonToken", authToken},
                {"page", currentPage},
                {"limit", itemsPerPage},
                {"filters", filterState},
                {"sort", sortState},
                {"supporterOnly", supporterOnlyFilter}});
            if (!searchQuery.empty()) {
                body["name"] = searchQuery;
            }
            auto response = co_await req.bodyJSON(body).post(fmt::format("{}/getBanners", comment::baseUrl));
            if (!response.ok()) {
                queueInMainThread([self, fetchId]() {
                    if (!self || self->isStaleFetch(fetchId)) {
                        return;
                    }
                    if (self->m_loadingCircle) {
                        self->m_loadingCircle->fadeOut();
                    }
                    Notification::create("Failed to fetch banners.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto jsonRes = response.json();
            if (jsonRes.isErr()) {
                queueInMainThread([self, fetchId]() {
                    if (!self || self->isStaleFetch(fetchId)) {
                        return;
                    }
                    if (self->m_loadingCircle) {
                        self->m_loadingCircle->fadeOut();
                    }
                    Notification::create("Received invalid data for banners.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto json = jsonRes.unwrap();
            if (!json.isObject()) {
                queueInMainThread([self, fetchId]() {
                    if (!self || self->isStaleFetch(fetchId)) {
                        return;
                    }
                    if (self->m_loadingCircle) {
                        self->m_loadingCircle->fadeOut();
                    }
                    Notification::create("Received invalid data for banners.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            int totalItems = 0;
            if (auto totalRes = json["total"].asInt(); totalRes.isOk()) {
                totalItems = static_cast<int>(totalRes.unwrap());
            }

            auto bannersArr = json["banners"];
            if (!bannersArr.isArray()) {
                queueInMainThread([self, fetchId]() {
                    if (!self || self->isStaleFetch(fetchId)) {
                        return;
                    }
                    if (self->m_loadingCircle) {
                        self->m_loadingCircle->fadeOut();
                    }
                    Notification::create("Received invalid data for banners.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            std::vector<CBBannerItem> banners;
            banners.reserve(bannersArr.size());

            for (size_t i = 0; i < bannersArr.size(); ++i) {
                auto item = bannersArr[i];
                CBBannerItem banner;
                if (auto urlRes = item["imageUrl"].asString(); urlRes.isOk()) {
                    banner.imageUrl = urlRes.unwrap();
                }
                if (auto usernameRes = item["username"].asString(); usernameRes.isOk()) {
                    banner.username = usernameRes.unwrap();
                }
                if (auto nameRes = item["name"].asString(); nameRes.isOk()) {
                    banner.name = nameRes.unwrap();
                }
                if (auto descriptionRes = item["description"].asString(); descriptionRes.isOk()) {
                    banner.description = descriptionRes.unwrap();
                }
                if (auto priceRes = item["price"].asInt(); priceRes.isOk()) {
                    banner.price = static_cast<int>(priceRes.unwrap());
                }
                if (auto idRes = item["id"].asInt(); idRes.isOk()) {
                    banner.id = static_cast<int>(idRes.unwrap());
                }
                if (auto accountIdRes = item["accountId"].asInt(); accountIdRes.isOk()) {
                    banner.accountId = static_cast<int>(accountIdRes.unwrap());
                }
                if (auto ownsRes = item["owns"].asBool(); ownsRes.isOk()) {
                    banner.owns = ownsRes.unwrap();
                }
                if (auto isLimitedRes = item["isLimited"].asBool(); isLimitedRes.isOk()) {
                    banner.isLimited = isLimitedRes.unwrap();
                }
                if (auto isFeaturedRes = item["isFeatured"].asBool(); isFeaturedRes.isOk()) {
                    banner.isFeatured = isFeaturedRes.unwrap();
                }
                banner.isSupporterOnly = item["isSupporterOnly"].asBool().unwrapOr(false);
                banner.createdAt = item["createdAt"].asString().unwrapOr("");
                banner.editedAt = item["editedAt"].asString().unwrapOr("");
                banner.approvedAt = item["approvedAt"].asString().unwrapOr("");
                banner.isFavorite = item["isFavorite"].asBool().unwrapOr(false);
                banner.nonCompactImageUrl = item["nonCompactImageUrl"].asString().unwrapOr("");
                if (auto amountRes = item["amount"].asInt(); amountRes.isOk()) {
                    banner.amount = static_cast<int>(amountRes.unwrap());
                }
                if (auto totalBoughtRes = item["totalBought"].asInt(); totalBoughtRes.isOk()) {
                    banner.totalBought = static_cast<int>(totalBoughtRes.unwrap());
                }
                if (auto equippedCountRes = item["equippedCount"].asInt(); equippedCountRes.isOk()) {
                    banner.equippedCount = static_cast<int>(equippedCountRes.unwrap());
                }
                banner.equipped = (banner.id == equippedBannerId);
                banners.push_back(std::move(banner));
            }

            queueInMainThread([self, fetchId, totalItems, banners = std::move(banners)]() mutable {
                if (!self || self->isStaleFetch(fetchId)) {
                    return;
                }
                if (!self->m_list) {
                    return;
                }

                self->m_totalItems = totalItems;
                self->m_banners = std::move(banners);
                self->populateList();

                if (self->m_loadingCircle) {
                    self->m_loadingCircle->fadeOut();
                }
            });
            co_return;
        });
    });
}

void CBShopLayer::populateList() {
    if (!this->getParent() || s_instance != this) return;
    if (!m_list) return;

    m_list->clear();
    float listWidth = m_list->getListSize().width;

    for (auto const& banner : m_banners) {
        auto cell = CBBannerCell::create(banner, listWidth, true);
        if (cell) {
            m_list->setCellColor(ccColor4B{0, 0, 0, 0});
            m_list->addCell(cell);
        }
    }
    m_list->updateLayout();

    if (m_noBannersLabel) {
        m_noBannersLabel->setVisible(m_banners.empty());
    }
    this->updateLocalEquippedState();

    int totalPages = std::ceil((float)m_totalItems / m_itemsPerPage);
    if (m_pageLabel) {
        m_pageLabel->setString(fmt::format("Page {} of {} (Total: {})", m_currentPage + 1, totalPages, m_totalItems).c_str());
    }
    if (m_prevButton) {
        m_prevButton->setVisible(m_currentPage > 0);
    }
    if (m_nextButton) {
        m_nextButton->setVisible(m_currentPage < totalPages - 1);
    }
}

void CBShopLayer::fetchAmethyst() {
    Ref<CBShopLayer> self = this;
    queueInMainThread([self]() {
        if (!self || !self->getParent() || s_instance != self.data()) {
            return;
        }

        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;
        if (accountId <= 0) {
            Notification::create("Failed to retrieve a valid account ID.", NotificationIcon::Error)->show();
            return;
        }

        // your cube, so leaderboard/admin cells can draw it (read here, GameManager isn't thread safe)
        auto gm = GameManager::sharedState();
        auto icon = matjson::makeObject({{"iconId", gm->getPlayerFrame()},
            {"color1", gm->getPlayerColor()},
            {"color2", gm->getPlayerColor2()},
            {"glowColor", gm->getPlayerGlowColor()},
            {"hasGlow", gm->getPlayerGlow()}});

        arc::spawn([self, accountId, accountData, icon]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                queueInMainThread([self]() {
                    if (!self || !self->getParent() || s_instance != self.data()) {
                        return;
                    }
                    Notification::create("Failed to authenticate.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto authToken = std::move(authResult);
            auto req = utils::web::WebRequest();
            auto body = icon;
            body["accountId"] = accountId;
            body["argonToken"] = authToken;
            auto response = co_await req.bodyJSON(body).post(fmt::format("{}/getUser", comment::baseUrl));
            if (!response.ok()) {
                queueInMainThread([self]() {
                    if (!self || !self->getParent() || s_instance != self.data()) {
                        return;
                    }
                    Notification::create("Failed to fetch amethyst.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto jsonRes = response.json();
            if (jsonRes.isErr()) {
                queueInMainThread([self]() {
                    if (!self || !self->getParent() || s_instance != self.data()) {
                        return;
                    }
                    Notification::create("Received invalid server data.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto json = jsonRes.unwrap();
            auto amethystRes = json["amethyst"].asInt();
            if (amethystRes.isErr()) {
                queueInMainThread([self]() {
                    if (!self || !self->getParent() || s_instance != self.data()) {
                        return;
                    }
                    Notification::create("Failed to parse amethyst amount.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            int amethyst = static_cast<int>(amethystRes.unwrap());
            queueInMainThread([self, amethyst, json = std::move(json)]() {
                if (!self || !self->getParent() || s_instance != self.data()) {
                    return;
                }

                if (auto equippedIdRes = json["equippedBanner"]["id"].asInt(); equippedIdRes.isOk()) {
                    self->m_equippedBannerId = static_cast<int>(equippedIdRes.unwrap());
                } else {
                    self->m_equippedBannerId = -1;
                }
                if (auto isStaffRes = json["is_staff"].asBool(); isStaffRes.isOk()) {
                    self->m_isStaff = isStaffRes.unwrap();
                } else {
                    self->m_isStaff = false;
                }
                if (auto isAdminRes = json["is_admin"].asBool(); isAdminRes.isOk()) {
                    self->m_isAdmin = isAdminRes.unwrap();
                    Mod::get()->setSavedValue("is_admin", self->m_isAdmin);
                } else {
                    self->m_isAdmin = false;
                    Mod::get()->setSavedValue("is_admin", false);
                }
                Mod::get()->setSavedValue("is_supporter", json["is_supporter"].asBool().unwrapOr(false));
                bool oldHasCreated = Mod::get()->getSavedValue<bool>("has_created_banner", false);
                bool newHasCreated = false;

                if (auto hasCreatedRes = json["has_created_banner"].asBool(); hasCreatedRes.isOk()) {
                    newHasCreated = hasCreatedRes.unwrap();
                }

                if (oldHasCreated && !newHasCreated) {
                    log::info("Your banner was rejected, you can now create another banner for free.");
                }

                Mod::get()->setSavedValue("has_created_banner", newHasCreated);
                Mod::get()->setSavedValue("amethyst", amethyst);

                int monthlyEarnings = static_cast<int>(json["monthly_earnings"].asInt().unwrapOr(0));
                Mod::get()->setSavedValue("monthly_earnings", monthlyEarnings);
                if (self->m_earningRateLabel) {
                    self->m_earningRateLabel->setString(fmt::format("+{}/mo", GameToolbox::pointsToString(monthlyEarnings)).c_str());
                }

                if (!self->m_amethystLabel) {
                    return;
                }
                self->m_amethystLabel->setTargetCount(amethyst);
                self->m_amethystLabel->updateCounter(0.f);
                if (self->m_equippedBannerId >= 0) {
                    self->refreshBanners();
                }

                // Add Admin button dynamically if staff
                if (self->m_isStaff) {
                    if (auto navMenu = self->getChildByID("nav-menu")) {
                        if (!navMenu->getChildByID("admin-button")) {
                            auto adminBtn = Button::createWithNode(
                                ButtonSprite::create("Admin", "goldFont.fnt", "GJ_button_03.png", .8f),
                                [](Button* sender) {
                                    if (auto popup = CBAdminPanelLayer::create()) {
                                        popup->show();
                                    }
                                });
                            adminBtn->setID("admin-button");
                            navMenu->addChild(adminBtn);
                            static_cast<CCMenu*>(navMenu)->updateLayout();
                        }
                    }
                }
            });
            co_return;
        });
    });
}

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

    s_instance = this;

    bool animatedBgDisabled = Mod::get()->getSettingValue<bool>("disable-animated-background");
    if (!animatedBgDisabled) {
        m_background = cue::RepeatingBackground::create("game_bg_57_001.png", 1.f, cue::RepeatMode::X);
    } else {
        m_background = createLayerBG();
    }
    if (m_background) {
        m_background->setColor({154, 108, 217});
        this->addChild(m_background);
    }

    addBackButton(this, BackButtonStyle::Pink);
    // auto backBtn = Button::createWithSpriteFrameName("GJ_arrow_03_001.png", [this](Button*) { this->keyBackClicked(); });
    // backBtn->setZOrder(CCScene::get()->getHighestChildZ() + 1);
    // this->addChildAtPosition(backBtn, Anchor::TopLeft, {25.f, -25.f}, false);
    addSideArt(this, SideArt::Bottom);
    addSideArt(this, SideArt::TopLeft);

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    m_list = cue::ListNode::create({356, winSize.height - 85}, {0, 0, 0, 0}, cue::ListBorderStyle::None);
    if (m_list) {
        m_list->setZOrder(2);
        this->addChildAtPosition(m_list, Anchor::Center, {0.f, -22.f}, false);

        // auto scrollbar = Scrollbar::create(m_list->getScrollLayer());
        //this->addChildAtPosition(scrollbar, Anchor::Center, {356.f / 2 + 15.f, -22.f}, false);
    }

    // label that shows what filters/sort it is
    m_filterSortLabel = Label::create("All Banners", "goldFont.fnt");
    m_filterSortLabel->setScale(0.5f);
    m_filterSortLabel->setAlignment(Label::Alignment::Left);
    m_filterSortLabel->setVisible(true);
    m_filterSortLabel->setZOrder(5);
    this->addChildAtPosition(m_filterSortLabel, Anchor::Top, {0.f, -15.f}, false);
    this->updateFilterSortLabel();

    m_noBannersLabel = Label::create("No Banners Found", "goldFont.fnt");
    m_noBannersLabel->setScale(0.6f);
    m_noBannersLabel->setVisible(false);
    m_noBannersLabel->setZOrder(5);
    this->addChildAtPosition(m_noBannersLabel, Anchor::Center, {0.f, -22.f}, false);

    auto bottomRightMenu = CCMenu::create();
    bottomRightMenu->setAnchorPoint({0.5f, 0.f});
    bottomRightMenu->setContentSize({40.f, 0.f});
    bottomRightMenu->setLayout(ColumnLayout::create()
            ->setAxisAlignment(AxisAlignment::Start)
            ->setGap(0.f)
            ->setAutoGrowAxis(0.f));
    this->addChildAtPosition(bottomRightMenu, Anchor::BottomRight, {-30.f, 10.f}, false);

    if (auto refreshButton = Button::createWithNode(CircleButtonSprite::createWithSpriteFrameName("CB_refresh.png"_spr, 1.2f, CircleBaseColor::DarkPurple, CircleBaseSize::Small),
            [this](Button* sender) {
                this->fetchBanners();
            })) {
        bottomRightMenu->addChild(refreshButton);
        bottomRightMenu->updateLayout();
    }

    if (auto leaderboardButton = Button::createWithNode(CircleButtonSprite::createWithSpriteFrameName("CB_leaderboard.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small), [this](Button* sender) {
            pushSceneWithLayer(CBLeaderboardLayer::create());
        })) {
        bottomRightMenu->addChild(leaderboardButton);
        bottomRightMenu->updateLayout();
    }

    if (auto discordButton = Button::createWithNode(CircleButtonSprite::createWithSpriteFrameName("CB_discord.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small), [this](Button* sender) {
            auto communityUrl = getMod()->getMetadata().getLinks().getCommunityURL();
            if (communityUrl.has_value()) {
                utils::web::openLinkInBrowser(communityUrl.value());
            }
        })) {
        bottomRightMenu->addChild(discordButton);
        bottomRightMenu->updateLayout();
    }

    if (auto supporterButton = Button::createWithNode(CircleButtonSprite::createWithSpriteFrameName("gj_heartOn_001.png", 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small), [](Button* sender) {
            if (auto popup = CBSupporterPopup::create()) {
                popup->show();
            }
        })) {
        supporterButton->setID("supporter-button");
        bottomRightMenu->addChild(supporterButton);
        bottomRightMenu->updateLayout();
    }

    if (auto settingsButton = Button::createWithNode(CircleButtonSprite::createWithSpriteFrameName("geode.loader/settings.png", 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small), [this](Button* sender) {
            openSettingsPopup(getMod());
        })) {
        this->addChildAtPosition(settingsButton, Anchor::BottomLeft, {25.f, 65.f}, false);
    }

    if (auto infoButton = Button::createWithNode(CircleButtonSprite::createWithSpriteFrameName("CB_info.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small), [this](Button* sender) {
            MDPopup::create(
                "About Comment Banners",
                "<cp>**Comment Banners**</c> is a <cb>community-run</c> banner shop that lets you buy <cg>user-generated</c> banners to apply to your <cc>Comment Cell</c> for everyone to see using this mod.\n\n"
                "-----\n"
                "### About Amethysts\n"
                "<cp>Amethysts</c> are the custom currency used for <cp>Comment Banners</c> and can be obtained by completing any <cy>Rated Level</c>.\n\n"
                "The following <cp>amethyst values</c> are rewarded based on <co>difficulty</c>:\n"
                "- **<co>Auto</c>**: 50-100 Amethysts\n"
                "- **<cl>Easy</c>**: 100-200 Amethysts\n"
                "- **<cg>Normal</c>**: 500-1000 Amethysts\n"
                "- **<cy>Hard</c>**: 1000-2000 Amethysts\n"
                "- **<cr>Harder</c>**: 1000-5000 Amethysts\n"
                "- **<cp>Insane</c>**: 5000-10k Amethysts\n"
                "- **<cc>Demon</c>**: 10k-15k Amethysts\n"
                "-----\n"
                "### Submission Rules\n"
                "To ensure that the quality of banners is high and there are no <cr>inappropriate banners</c> being uploaded, all banners must be <cc>reviewed and approved by the staff team</c> before being added to the shop. <cy>Changing a banner's image sends it back for review</c>; until it's approved the banner is pending and can't be equipped.\n\n"
                "Every submission must follow these rules:\n\n"
                "- The banner must not contain any <cr>inappropriate content, including nudity, hate speech, or gore</c>.\n"
                "- Banners must be of an appropriate quality and must not be <cr>low-quality</c>.\n"
                "- Banners that are <cr>mostly AI-Generated</c> are <cr>not accepted</c>.\n"
                "- Banners must be in a <cg>1500 x 150</c> resolution.\n"
                "- Banners must be in a <cc>PNG, WebP, JPEG or GIF</c> format.\n"
                "- Any form of <cy>self-promotion</c> on your submission is <cr>not accepted</c>.\n"
                "- Strong profanity like swearing or slurs is <cr>not accepted</c>. Some <cg>soft profanity</c> is accepted.\n"
                "- Any form of <cc>plagiarism, like stealing other users' banners,</cc> will result in a <cr>Temporary Ban</c>.\n\n"
                "#### Any misuse of this mod or repeatedly breaking the rules will result in a <cr>temporary ban</c> on submitting your own banners depending on the <cr>severity.</c>\n\n"
                "-----\n"
                "### Amethysts Economy\n"
                "This mod also runs its own <cg>Economy</c> that allows banner creators to earn more <cp>Amethysts</c> passively. There are currently three ways to earn <cp>Amethysts</c>:\n"
                "- Beating Rated Levels\n"
                "- When someone buys your banner (<cg>10%</c> cut)\n"
                "- Every month you get <cp>amethysts</c> based on the amount of users that have your banner equipped. This increases by <cg>5% for every 5 people</c> that have your banner equipped (this <cr>caps at 30%</c> so it doesn't break the economy).\n"
                "#### The rates may change from time to time depending on the <cc>popularity of the mod</c>, thus the information about the <cg>economy</c> may be outdated.\n\n",
                "OK")
                ->show();
        })) {
        this->addChildAtPosition(infoButton, Anchor::BottomLeft, {25.f, 25.f}, false);
    }

    auto filterMenu = CCMenu::create();
    filterMenu->setAnchorPoint({0.5f, 1.f});
    filterMenu->setLayout(ColumnLayout::create()->setAxisAlignment(AxisAlignment::End)->setAutoGrowAxis(0.f));

    if (auto filterButton = Button::createWithNode(EditorButtonSprite::createWithSpriteFrameName("GJ_filterIcon_001.png", 1.0f, EditorBaseColor::Gray), [this](Button* sender) { this->onFilterClicked(sender); })) {
        m_filterButton = filterButton;
        filterMenu->addChild(filterButton);
    }

    filterMenu->updateLayout();
    this->addChildAtPosition(filterMenu, Anchor::TopLeft, {25.f, -45.f}, false);

    if (auto searchButton = Button::createWithNode(EditorButtonSprite::createWithSpriteFrameName("edit_findBtn_001.png", 1.0f, EditorBaseColor::Gray), [this](Button* sender) { this->onSearchClicked(sender); })) {
        m_searchButton = searchButton;

        filterMenu->addChild(searchButton);
        filterMenu->updateLayout();
    }

    // Navigation Menu
    auto navMenu = CCMenu::create();
    navMenu->setID("nav-menu");
    navMenu->setContentSize({356, 30});
    navMenu->setLayout(RowLayout::create()->setGap(6.f));

    auto submitBtn = Button::createWithNode(
        ButtonSprite::create("Submit", "goldFont.fnt", "GJ_button_01.png", .75f),
        [](Button* sender) {
            if (auto popup = CBSubmitBannerPopup::create()) {
                popup->show();
            }
        });
    submitBtn->setID("submit-button");
    navMenu->addChild(submitBtn);

    auto logsBtn = Button::createWithNode(
        ButtonSprite::create("Logs", "goldFont.fnt", "GJ_button_01.png", .75f),
        [](Button* sender) {
            if (auto popup = CBLogsPopup::create()) {
                popup->show();
            }
        });
    logsBtn->setID("logs-button");
    navMenu->addChild(logsBtn);

    auto yourBannersBtn = Button::createWithNode(
        ButtonSprite::create("Your Banners", "goldFont.fnt", "GJ_button_01.png", .75f),
        [](Button* sender) {
            if (auto popup = CBYourBannersPopup::create()) {
                popup->show();
            }
        });
    yourBannersBtn->setID("your-banners-button");
    navMenu->addChild(yourBannersBtn);

    auto localBtn = Button::createWithNode(
        ButtonSprite::create("Local", "goldFont.fnt", "GJ_button_05.png", .75f),
        [](Button* sender) {
            if (auto popup = CBLocalBannersPopup::create()) {
                popup->show();
            }
        });
    localBtn->setID("local-button");
    navMenu->addChild(localBtn);

    this->addChildAtPosition(navMenu, Anchor::Top, {0.f, -40.f}, false);
    navMenu->updateLayout();

    m_localOverlay = CCNode::create();
    m_localOverlay->setContentSize({356.f, winSize.height - 85.f});
    m_localOverlay->setAnchorPoint({0.5f, 0.5f});
    m_localOverlay->setZOrder(10);
    this->addChildAtPosition(m_localOverlay, Anchor::Center, {0.f, -22.f}, false);

    if (auto headerLabel = Label::create("Local Banner Equipped", "goldFont.fnt")) {
        headerLabel->setScale(0.7f);
        m_localOverlay->addChildAtPosition(headerLabel, Anchor::Center, {0.f, 25.f}, false);
    }

    if (auto descLabel = SimpleTextArea::create(
            "You currently have a local banner equipped.\n"
            "Shop banners are disabled.")) {
        descLabel->setAlignment(CCTextAlignment::kCCTextAlignmentCenter);
        m_localOverlay->addChildAtPosition(descLabel, Anchor::Center, {0.f, -10.f}, false);
    }

    this->updateLocalEquippedState();

    // Pagination Menu
    auto paginationMenu = CCMenu::create();
    paginationMenu->setID("pagination-menu");

    m_prevButton = Button::createWithSpriteFrameName("GJ_arrow_01_001.png", [this](Button* sender) { this->onPrevPage(sender); });
    m_prevButton->setScale(0.8f);

    m_nextButton = Button::createWithSpriteFrameName("GJ_arrow_01_001.png", [this](Button* sender) { this->onNextPage(sender); });
    static_cast<CCSprite*>(m_nextButton->getDisplayNode())->setFlipX(true);
    m_nextButton->setScale(0.8f);

    m_pageLabel = Label::create("Page 1 of 1 (Total: 0)", "goldFont.fnt");
    m_pageLabel->setLimitLabelWidth(100.f, 0.4f, 0.1f);

    paginationMenu->addChild(m_prevButton);
    this->addChildAtPosition(m_pageLabel, Anchor::Bottom, {0, 10}, false);
    paginationMenu->addChild(m_nextButton);

    paginationMenu->setLayout(RowLayout::create()->setGap(370.f));
    this->addChildAtPosition(paginationMenu, Anchor::Center, {0.f, -15.f}, false);
    paginationMenu->updateLayout();

    this->fetchBanners();
    this->fetchAmethyst();

    auto listBg = NineSlice::create("square02_001.png");
    if (listBg && m_list) {
        listBg->setPosition(m_list->getPosition());
        listBg->setContentSize(m_list->getContentSize() + CCSize(5.f, 10.f));
        listBg->setOpacity(100);
        this->addChild(listBg);
    }

    // auto authButton = Button::createWithNode(
    //     CircleButtonSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small),
    //     [](Button* button) {
    //         Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Checking permissions...");
    //         popup->show();

    //         auto accountData = argon::getGameAccountData();
    //         auto accountId = accountData.accountId;

    //         if (accountId <= 0) {
    //             popup->showFailMessage("Invalid account ID.");
    //             return;
    //         }

    //         arc::spawn([accountId, accountData, popup]() -> arc::Future<> {
    //             auto authResult = co_await comment::argonToken(accountData);
    //             if (authResult.empty()) {
    //                 queueInMainThread([popup] {
    //                     popup->showFailMessage("Authentication failed.");
    //                 });
    //                 co_return;
    //             }

    //             auto authToken = std::move(authResult);
    //             auto req = utils::web::WebRequest();
    //             auto url = fmt::format("{}/userInfo?accountId={}&argonToken={}", comment::baseUrl, accountId, authToken);

    //             auto response = co_await req.get(url);
    //             if (!response.ok()) {
    //                 queueInMainThread([popup] {
    //                     popup->showFailMessage("Failed to fetch user info.");
    //                 });
    //                 co_return;
    //             }

    //             auto jsonRes = response.json();
    //             if (jsonRes.isErr()) {
    //                 queueInMainThread([popup] {
    //                     popup->showFailMessage("Invalid response from server.");
    //                 });
    //                 co_return;
    //             }

    //             auto json = jsonRes.unwrap();
    //             bool isStaff = false;
    //             if (auto staffRes = json["is_staff"].asBool(); staffRes.isOk()) {
    //                 isStaff = staffRes.unwrap();
    //             }
    //             if (auto adminRes = json["is_admin"].asBool(); adminRes.isOk()) {
    //                 Mod::get()->setSavedValue("is_admin", adminRes.unwrap());
    //             }
    //             queueInMainThread([popup, isStaff] {
    //                 if (isStaff) {
    //                     popup->onClose(nullptr);
    //                     if (auto adminPopup = CBAdminPanelLayer::create()) {
    //                         adminPopup->show();
    //                     }
    //                 } else {
    //                     popup->showFailMessage("You do not have staff permissions.");
    //                 }
    //             });
    //             co_return;
    //         });
    //     });

    // if (authButton) {
    //     auto winSize = CCDirector::sharedDirector()->getWinSize();
    //     this->addChildAtPosition(authButton, Anchor::TopLeft, {25.f, -80}, false);
    // }

    // amethyst counter
    auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr);
    if (amethystIcon) {
        amethystIcon->setScale(0.7f);
        this->addChildAtPosition(amethystIcon, Anchor::TopRight, {-18.f, -16}, false);
    }

    int amethystValue = Mod::get()->getSavedValue<int>("amethyst", 0);
    m_amethystLabel = CCCounterLabel::create(amethystValue, "bigFont.fnt", FormatterType::Integer);
    if (m_amethystLabel) {
        m_amethystLabel->setScale(0.6f);
        this->addChildAtPosition(m_amethystLabel, Anchor::TopRight, {-32.f, -16}, {1.f, 0.5f}, false);
    }

    // current monthly earning rate from your banners, filled in by fetchAmethyst
    m_earningRateLabel = Label::create(
        fmt::format("+{}/mo", GameToolbox::pointsToString(Mod::get()->getSavedValue<int>("monthly_earnings", 0))).c_str(),
        "bigFont.fnt");
    m_earningRateLabel->setScale(0.35f);
    m_earningRateLabel->setColor({100, 255, 100});
    this->addChildAtPosition(m_earningRateLabel, Anchor::TopRight, {-8.f, -32.f}, {1.f, 0.5f}, false);

    this->setKeypadEnabled(true);

    return true;
}

void CBShopLayer::keyBackClicked() {
    CCDirector::sharedDirector()->popSceneWithTransition(0.5f, PopTransition::kPopTransitionFade);
}

void CBShopLayer::onFilterClicked(CCObject* sender) {
    if (auto popup = CBBannerFilterPopup::create()) {
        popup->show();
    }
}

void CBShopLayer::setFilterState(int state) {
    if (state == m_filterState) return;
    m_filterState = state;
    this->onFilterSortChanged();
}

void CBShopLayer::setSortState(int state) {
    if (state == m_sortState) return;
    m_sortState = state;
    this->onFilterSortChanged();
}

void CBShopLayer::setSupporterOnlyFilter(bool enabled) {
    if (enabled == m_supporterOnlyFilter) return;
    m_supporterOnlyFilter = enabled;
    this->onFilterSortChanged();
}

void CBShopLayer::resetFilterSort() {
    if (m_filterState == 0 && m_sortState == 0 && !m_supporterOnlyFilter) return;
    m_filterState = 0;
    m_sortState = 0;
    m_supporterOnlyFilter = false;
    this->onFilterSortChanged();
}

void CBShopLayer::onFilterSortChanged() {
    if (m_filterButton) {
        bool isDefault = m_filterState == 0 && m_sortState == 0 && !m_supporterOnlyFilter;
        auto newSpr = EditorButtonSprite::createWithSpriteFrameName("GJ_filterIcon_001.png", 1.0f, isDefault ? EditorBaseColor::Gray : EditorBaseColor::Cyan);
        m_filterButton->setDisplayNode(newSpr);
    }

    this->updateFilterSortLabel();

    m_currentPage = 0;
    this->fetchBanners();
}

void CBShopLayer::onSearchClicked(CCObject* sender) {
    auto popup = SetTextPopup::create(m_searchQuery, "Search by name...", 32, "Search Banner", "Search", true, 60, false, 0, 0);
    popup->m_delegate = this;
    popup->show();
}

void CBShopLayer::setTextPopupClosed(SetTextPopup* popup, std::string text) {
    m_searchQuery = text;

    if (m_searchButton) {
        EditorBaseColor baseColor = m_searchQuery.empty() ? EditorBaseColor::Gray : EditorBaseColor::Cyan;
        auto newSpr = EditorButtonSprite::createWithSpriteFrameName("edit_findBtn_001.png", 1.0f, baseColor);
        m_searchButton->setDisplayNode(newSpr);
    }

    this->updateFilterSortLabel();

    m_currentPage = 0;
    this->fetchBanners();
}

void CBShopLayer::onNextPage(CCObject* sender) {
    m_currentPage++;
    this->fetchBanners();
}

void CBShopLayer::onPrevPage(CCObject* sender) {
    if (m_currentPage > 0) {
        m_currentPage--;
        this->fetchBanners();
    }
}