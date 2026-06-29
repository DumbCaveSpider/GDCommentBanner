#include <Geode/Enums.hpp>
#include <Geode/Geode.hpp>
#include <Geode/binding/CCCounterLabel.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/ui/Button.hpp>
#include "CBShopLayer.hpp"
#include "CBShopLayer.hpp"
#include "CBShopLayer.hpp"
#include "CBBannerCell.hpp"
#include "CBSubmitBannerPopup.hpp"
#include "CBLogsPopup.hpp"
#include "CBAdminPanelLayer.hpp"
#include "CBYourBannersPopup.hpp"
#include <Geode/binding/SetTextPopup.hpp>
#include "Geode/ui/Layout.hpp"
#include "Geode/utils/web.hpp"
#include "ccTypes.h"
#include "include/CBConstant.hpp"
#include <argon/argon.hpp>
#include <cue/LoadingCircle.hpp>
#include <cue/ListBorder.hpp>
#include <cue/ListNode.hpp>
#include <Geode/ui/Scrollbar.hpp>

using namespace geode::prelude;

CBShopLayer* CBShopLayer::s_instance = nullptr;

CBShopLayer* CBShopLayer::getInstance() {
    return s_instance;
}

void CBShopLayer::refreshBanners() {
    if (m_loadingCircle) {
        m_loadingCircle->fadeIn();
    }
    this->fetchBanners();
}

void CBShopLayer::setEquippedBannerId(int bannerId) {
    m_equippedBannerId = bannerId;
}

CBShopLayer::~CBShopLayer() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void CBShopLayer::updateAmethystLabel(int amethyst) {
    if (!m_amethystLabel) {
        return;
    }
    m_amethystLabel->setTargetCount(amethyst);
    m_amethystLabel->updateCounter(0.f);
}

void CBShopLayer::fetchBanners() {
    geode::queueInMainThread([this]() {
        if (m_list) {
            m_list->clear();
        }
        if (m_noBannersLabel) {
            m_noBannersLabel->setVisible(false);
        }
        if (!m_loadingCircle && m_list) {
            m_loadingCircle = cue::LoadingCircle::create(true);
            if (m_loadingCircle) {
                m_loadingCircle->addToLayer(m_list, 10);
            }
        }
        if (m_loadingCircle) {
            m_loadingCircle->fadeIn();
        }

        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;
        if (accountId <= 0) {
            if (m_loadingCircle) {
                m_loadingCircle->fadeOut();
            }
            Notification::create("Failed to retrieve a valid account ID.", NotificationIcon::Error)->show();
            return;
        }

        arc::spawn([this, accountId, accountData]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                geode::queueInMainThread([this]() {
                    if (m_loadingCircle) {
                        m_loadingCircle->fadeOut();
                    }
                    Notification::create("Failed to authenticate.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto authToken = std::move(authResult);
            auto req = geode::utils::web::WebRequest();
            auto body = matjson::makeObject({{"accountId", accountId},
                {"argonToken", authToken},
                {"page", m_currentPage},
                {"limit", m_itemsPerPage},
                {"filter", m_filterState},
                {"sort", m_sortState}});
            if (!m_searchQuery.empty()) {
                body["name"] = m_searchQuery;
            }
            auto response = co_await req.bodyJSON(body).post(fmt::format("{}/getBanners", comment::baseUrl));
            if (!response.ok()) {
                geode::queueInMainThread([this]() {
                    if (m_loadingCircle) {
                        m_loadingCircle->fadeOut();
                    }
                    Notification::create("Failed to fetch banners.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto jsonRes = response.json();
            if (jsonRes.isErr()) {
                geode::queueInMainThread([this]() {
                    if (m_loadingCircle) {
                        m_loadingCircle->fadeOut();
                    }
                    Notification::create("Received invalid data for banners.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto json = jsonRes.unwrap();
            if (!json.isObject()) {
                geode::queueInMainThread([this]() {
                    if (m_loadingCircle) {
                        m_loadingCircle->fadeOut();
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
                geode::queueInMainThread([this]() {
                    if (m_loadingCircle) {
                        m_loadingCircle->fadeOut();
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
                if (auto amountRes = item["amount"].asInt(); amountRes.isOk()) {
                    banner.amount = static_cast<int>(amountRes.unwrap());
                }
                if (auto totalBoughtRes = item["totalBought"].asInt(); totalBoughtRes.isOk()) {
                    banner.totalBought = static_cast<int>(totalBoughtRes.unwrap());
                }
                banner.equipped = (banner.id == m_equippedBannerId);
                banners.push_back(std::move(banner));
            }

            geode::queueInMainThread([this, totalItems, banners = std::move(banners)]() mutable {
                if (!m_list) {
                    return;
                }

                this->m_totalItems = totalItems;
                this->m_banners = std::move(banners);
                this->populateList();

                if (m_loadingCircle) {
                    m_loadingCircle->fadeOut();
                }
            });
            co_return;
        });
    });
}

void CBShopLayer::populateList() {
    if (!m_list) return;

    m_list->clear();
    float listWidth = m_list->getListSize().width;

    for (auto const& banner : m_banners) {
        auto cell = CBBannerCell::create(banner, listWidth);
        if (cell) {
            m_list->setCellColor(ccColor4B{0, 0, 0, 0});
            m_list->addCell(cell);
        }
    }
    m_list->updateLayout();

    if (m_noBannersLabel) {
        m_noBannersLabel->setVisible(m_banners.empty());
    }

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
    geode::queueInMainThread([this]() {
        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;
        if (accountId <= 0) {
            Notification::create("Failed to retrieve a valid account ID.", NotificationIcon::Error)->show();
            return;
        }

        arc::spawn([this, accountId, accountData]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                geode::queueInMainThread([]() {
                    Notification::create("Failed to authenticate.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto authToken = std::move(authResult);
            auto req = geode::utils::web::WebRequest();
            auto body = matjson::makeObject({{"accountId", accountId}, {"argonToken", authToken}});
            auto response = co_await req.bodyJSON(body).post(fmt::format("{}/getUser", comment::baseUrl));
            if (!response.ok()) {
                geode::queueInMainThread([]() {
                    Notification::create("Failed to fetch amethyst.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto jsonRes = response.json();
            if (jsonRes.isErr()) {
                geode::queueInMainThread([]() {
                    Notification::create("Received invalid server data.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto json = jsonRes.unwrap();
            auto amethystRes = json["amethyst"].asInt();
            if (amethystRes.isErr()) {
                geode::queueInMainThread([]() {
                    Notification::create("Failed to parse amethyst amount.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            int amethyst = static_cast<int>(amethystRes.unwrap());
            geode::queueInMainThread([this, amethyst, json]() {
                if (auto equippedIdRes = json["equippedBanner"]["id"].asInt(); equippedIdRes.isOk()) {
                    m_equippedBannerId = static_cast<int>(equippedIdRes.unwrap());
                } else {
                    m_equippedBannerId = -1;
                }
                if (auto isStaffRes = json["is_staff"].asBool(); isStaffRes.isOk()) {
                    m_isStaff = isStaffRes.unwrap();
                } else {
                    m_isStaff = false;
                }
                if (auto isAdminRes = json["is_admin"].asBool(); isAdminRes.isOk()) {
                    m_isAdmin = isAdminRes.unwrap();
                    Mod::get()->setSavedValue("is_admin", m_isAdmin);
                } else {
                    m_isAdmin = false;
                    Mod::get()->setSavedValue("is_admin", false);
                }

                if (auto hasCreatedRes = json["has_created_banner"].asBool(); hasCreatedRes.isOk()) {
                    Mod::get()->setSavedValue("has_created_banner", hasCreatedRes.unwrap());
                } else {
                    Mod::get()->setSavedValue("has_created_banner", false);
                }

                Mod::get()->setSavedValue("amethyst", amethyst);

                if (!m_amethystLabel) {
                    return;
                }
                m_amethystLabel->setTargetCount(amethyst);
                m_amethystLabel->updateCounter(0.f);
                if (m_equippedBannerId >= 0) {
                    this->refreshBanners();
                }

                // Add Admin button dynamically if staff
                if (m_isStaff) {
                    if (auto navMenu = this->getChildByID("nav-menu")) {
                        if (!navMenu->getChildByID("admin-button")) {
                            auto adminBtn = geode::Button::createWithNode(
                                ButtonSprite::create("Admin", "goldFont.fnt", "GJ_button_03.png", .8f),
                                [](geode::Button* sender) {
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

    m_background = createLayerBG();
    m_background->setColor({154, 108, 217});
    this->addChild(m_background);
    addBackButton(this, BackButtonStyle::Pink);
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

    m_noBannersLabel = CCLabelBMFont::create("No Banners Found", "goldFont.fnt");
    m_noBannersLabel->setScale(0.6f);
    m_noBannersLabel->setVisible(false);
    m_noBannersLabel->setZOrder(5);
    this->addChildAtPosition(m_noBannersLabel, Anchor::Center, {0.f, -22.f}, false);

    if (auto refreshButton = geode::Button::createWithSpriteFrameName(
            "GJ_updateBtn_001.png",
            [this](geode::Button* sender) {
                this->fetchBanners();
            })) {
        this->addChildAtPosition(refreshButton, Anchor::BottomRight, {-35.f, 35.f}, false);
    }

    if (auto discordButton = geode::Button::createWithSpriteFrameName("gj_discordIcon_001.png", [this](geode::Button* sender) {
            auto communityUrl = getMod()->getMetadata().getLinks().getCommunityURL();
            if (communityUrl.has_value()) {
                geode::utils::web::openLinkInBrowser(communityUrl.value());
            }
        })) {
        this->addChildAtPosition(discordButton, Anchor::BottomRight, {-35.f, 85.f}, false);
    }

    if (auto infoButton = geode::Button::createWithSpriteFrameName("GJ_infoIcon_001.png", [this](geode::Button* sender) {
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
                "To ensure that the quality of banners is high and there are no <cr>inappropriate banners</c> being uploaded, all banners must be <cc>reviewed and approved by the staff team</c> before being added to the shop.\n\n"
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

    if (auto filterButton = CCMenuItemSpriteExtra::create(
            EditorButtonSprite::createWithSpriteFrameName("GJ_filterIcon_001.png", 1.0f, EditorBaseColor::Gray),
            this,
            menu_selector(CBShopLayer::onFilterClicked))) {
        filterMenu->addChild(filterButton);
    }

    if (auto sortButton = CCMenuItemSpriteExtra::create(
            EditorButtonSprite::createWithSpriteFrameName("GJ_sortIcon_001.png", 1.0f, EditorBaseColor::Gray),
            this,
            menu_selector(CBShopLayer::onSortClicked))) {
        filterMenu->addChild(sortButton);
    }

    filterMenu->updateLayout();
    this->addChildAtPosition(filterMenu, Anchor::TopLeft, {25.f, -45.f}, false);

    if (auto searchButton = CCMenuItemSpriteExtra::create(
            EditorButtonSprite::createWithSpriteFrameName("edit_findBtn_001.png", 1.0f, EditorBaseColor::Gray),
            this,
            menu_selector(CBShopLayer::onSearchClicked))) {
        m_searchButton = searchButton;

        filterMenu->addChild(searchButton);
        filterMenu->updateLayout();
    }

    // Navigation Menu
    auto navMenu = CCMenu::create();
    navMenu->setID("nav-menu");
    navMenu->setContentSize({356, 30});
    navMenu->setLayout(RowLayout::create()->setGap(10.f));

    auto submitBtn = geode::Button::createWithNode(
        ButtonSprite::create("Submit", "goldFont.fnt", "GJ_button_01.png", .8f),
        [](geode::Button* sender) {
            if (auto popup = CBSubmitBannerPopup::create()) {
                popup->show();
            }
        });
    submitBtn->setID("submit-button");
    navMenu->addChild(submitBtn);

    auto logsBtn = geode::Button::createWithNode(
        ButtonSprite::create("Logs", "goldFont.fnt", "GJ_button_01.png", .8f),
        [](geode::Button* sender) {
            if (auto popup = CBLogsPopup::create()) {
                popup->show();
            }
        });
    logsBtn->setID("logs-button");
    navMenu->addChild(logsBtn);

    auto yourBannersBtn = geode::Button::createWithNode(
        ButtonSprite::create("Your Banners", "goldFont.fnt", "GJ_button_01.png", .8f),
        [](geode::Button* sender) {
            if (auto popup = CBYourBannersPopup::create()) {
                popup->show();
            }
        });
    yourBannersBtn->setID("your-banners-button");
    navMenu->addChild(yourBannersBtn);

    this->addChildAtPosition(navMenu, Anchor::Top, {0.f, -35.f}, false);
    navMenu->updateLayout();

    // Pagination Menu
    auto paginationMenu = CCMenu::create();

    auto prevSprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    prevSprite->setScale(0.8f);
    m_prevButton = CCMenuItemSpriteExtra::create(prevSprite, this, menu_selector(CBShopLayer::onPrevPage));

    auto nextSprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    nextSprite->setFlipX(true);
    nextSprite->setScale(0.8f);
    m_nextButton = CCMenuItemSpriteExtra::create(nextSprite, this, menu_selector(CBShopLayer::onNextPage));

    m_pageLabel = CCLabelBMFont::create("Page 1 of 1 (Total: 0)", "goldFont.fnt");
    m_pageLabel->limitLabelWidth(100.f, 0.4f, 0.1f);

    paginationMenu->addChild(m_prevButton);
    this->addChildAtPosition(m_pageLabel, Anchor::Bottom, {0, 10}, false);
    paginationMenu->addChild(m_nextButton);

    paginationMenu->setLayout(RowLayout::create()->setGap(370.f));
    this->addChildAtPosition(paginationMenu, Anchor::Center, {0.f, -15.f}, false);
    paginationMenu->updateLayout();

    this->fetchBanners();
    this->fetchAmethyst();

    auto listBg = NineSlice::create("square02_001.png");
    if (listBg) {
        listBg->setPosition(m_list->getPosition());
        listBg->setContentSize(m_list->getContentSize() + CCSize(5.f, 10.f));
        listBg->setOpacity(100);
        this->addChild(listBg);
    }

    // auto authButton = geode::Button::createWithNode(
    //     CircleButtonSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small),
    //     [](geode::Button* button) {
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
    //                 geode::queueInMainThread([popup] {
    //                     popup->showFailMessage("Authentication failed.");
    //                 });
    //                 co_return;
    //             }

    //             auto authToken = std::move(authResult);
    //             auto req = geode::utils::web::WebRequest();
    //             auto url = fmt::format("{}/userInfo?accountId={}&argonToken={}", comment::baseUrl, accountId, authToken);

    //             auto response = co_await req.get(url);
    //             if (!response.ok()) {
    //                 geode::queueInMainThread([popup] {
    //                     popup->showFailMessage("Failed to fetch user info.");
    //                 });
    //                 co_return;
    //             }

    //             auto jsonRes = response.json();
    //             if (jsonRes.isErr()) {
    //                 geode::queueInMainThread([popup] {
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
    //             geode::queueInMainThread([popup, isStaff] {
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

    this->setKeypadEnabled(true);

    return true;
}

void CBShopLayer::keyBackClicked() {
    CCDirector::sharedDirector()->popSceneWithTransition(0.5f, PopTransition::kPopTransitionFade);
}

void CBShopLayer::onFilterClicked(CCObject* sender) {
    auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);

    m_filterState = (m_filterState + 1) % 4;

    EditorBaseColor baseColor = EditorBaseColor::Gray;
    if (m_filterState == 1) {
        baseColor = EditorBaseColor::Green;
        Notification::create("Showing Owned Banners", NotificationIcon::Info)->show();
    } else if (m_filterState == 2) {
        baseColor = EditorBaseColor::Orange;
        Notification::create("Showing Unowned Banners", NotificationIcon::Info)->show();
    } else if (m_filterState == 3) {
        baseColor = EditorBaseColor::Pink;
        Notification::create("Showing Limited Banners", NotificationIcon::Info)->show();
    } else {
        Notification::create("Showing All Banners", NotificationIcon::Info)->show();
    }

    auto newSpr = EditorButtonSprite::createWithSpriteFrameName("GJ_filterIcon_001.png", 1.0f, baseColor);
    btn->setNormalImage(newSpr);

    m_currentPage = 0;
    this->fetchBanners();
}

void CBShopLayer::onSortClicked(CCObject* sender) {
    auto btn = static_cast<CCMenuItemSpriteExtra*>(sender);

    m_sortState = (m_sortState + 1) % 5;

    EditorBaseColor baseColor = EditorBaseColor::Gray;
    std::string text = "Sorting by Most Recent";

    switch (m_sortState) {
        case 1:
            baseColor = EditorBaseColor::Green;
            text = "Sorting by Most Bought";
            break;
        case 2:
            baseColor = EditorBaseColor::Cyan;
            text = "Sorting by Most Equipped";
            break;
        case 3:
            baseColor = EditorBaseColor::Orange;
            text = "Sorting by Highest Price";
            break;
        case 4:
            baseColor = EditorBaseColor::Pink;
            text = "Sorting by Lowest Price";
            break;
    }

    Notification::create(text, NotificationIcon::Info)->show();

    auto newSpr = EditorButtonSprite::createWithSpriteFrameName("GJ_sortIcon_001.png", 1.0f, baseColor);
    btn->setNormalImage(newSpr);

    m_currentPage = 0;
    this->fetchBanners();
}

void CBShopLayer::onSearchClicked(CCObject* sender) {
    auto popup = SetTextPopup::create(m_searchQuery, "Search by name...", 32, "Search Banner", "Search", true, 60);
    popup->m_delegate = this;
    popup->show();
}

void CBShopLayer::setTextPopupClosed(SetTextPopup* popup, gd::string text) {
    m_searchQuery = text;

    if (m_searchButton) {
        EditorBaseColor baseColor = m_searchQuery.empty() ? EditorBaseColor::Gray : EditorBaseColor::Cyan;
        auto newSpr = EditorButtonSprite::createWithSpriteFrameName("edit_findBtn_001.png", 1.0f, baseColor);
        m_searchButton->setNormalImage(newSpr);
    }

    m_currentPage = 0;
    this->fetchBanners();
}

void CBShopLayer::onNextPage(CCObject* sender) {
    m_currentPage++;
    this->fetchBanners();
}

void CBShopLayer::onPrevPage(CCObject* sender) {
    m_currentPage--;
    this->fetchBanners();
}