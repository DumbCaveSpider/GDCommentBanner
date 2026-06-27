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
#include "Geode/utils/web.hpp"
#include "ccTypes.h"
#include "include/CBConstant.hpp"
#include <argon/argon.hpp>
#include <cue/LoadingCircle.hpp>
#include <cue/ListBorder.hpp>
#include <cue/ListNode.hpp>

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
            auto body = matjson::makeObject({{"accountId", accountId}, {"argonToken", authToken}});
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
            if (!json.isArray()) {
                geode::queueInMainThread([this]() {
                    if (m_loadingCircle) {
                        m_loadingCircle->fadeOut();
                    }
                    Notification::create("Received invalid data for banners.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            std::vector<CBBannerItem> banners;
            banners.reserve(json.size());

            for (size_t i = 0; i < json.size(); ++i) {
                auto item = json[i];
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

            geode::queueInMainThread([this, banners = std::move(banners)]() mutable {
                if (!m_list) {
                    return;
                }

                float listWidth = m_list->getListSize().width;
                for (auto const& banner : banners) {
                    auto cell = CBBannerCell::create(banner, listWidth);
                    if (cell) {
                        m_list->setCellColor(ccColor4B{0, 0, 0, 0});
                        m_list->addCell(cell);
                    }
                }
                if (m_loadingCircle) {
                    m_loadingCircle->fadeOut();
                }
                m_list->updateLayout();
            });
            co_return;
        });
    });
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
            if (auto equippedIdRes = json["equippedBanner"]["id"].asInt(); equippedIdRes.isOk()) {
                m_equippedBannerId = static_cast<int>(equippedIdRes.unwrap());
            } else {
                m_equippedBannerId = -1;
            }
            if (auto isStaffRes = json["isStaff"].asBool(); isStaffRes.isOk()) {
                m_isStaff = isStaffRes.unwrap();
            } else {
                m_isStaff = false;
            }
            Mod::get()->setSavedValue("amethyst", amethyst);
            geode::queueInMainThread([this, amethyst]() {
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
                                ButtonSprite::create("Admin", "goldFont.fnt", "GJ_button_02.png", .8f),
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

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    m_list = cue::ListNode::create({356, winSize.height - 85}, {0, 0, 0, 0}, cue::ListBorderStyle::None);
    if (m_list) {
        m_list->setZOrder(2);
        this->addChildAtPosition(m_list, Anchor::Center, {0.f, -22.f}, false);
    }

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

    this->addChildAtPosition(navMenu, Anchor::Top, {0.f, -25.f}, false);
    navMenu->updateLayout();

    this->fetchBanners();
    this->fetchAmethyst();

    auto listBg = NineSlice::create("square02_001.png");
    if (listBg) {
        listBg->setPosition(m_list->getPosition());
        listBg->setContentSize(m_list->getContentSize() + CCSize(5.f, 10.f));
        listBg->setOpacity(100);
        this->addChild(listBg);
    }

    auto authButton = geode::Button::createWithNode(
        CircleButtonSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small),
        [](geode::Button* button) {
            Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Checking permissions...");
            popup->show();

            auto accountData = argon::getGameAccountData();
            auto accountId = accountData.accountId;

            if (accountId <= 0) {
                popup->showFailMessage("Invalid account ID.");
                return;
            }

            arc::spawn([accountId, accountData, popup]() -> arc::Future<> {
                auto authResult = co_await comment::argonToken(accountData);
                if (authResult.empty()) {
                    geode::queueInMainThread([popup] {
                        popup->showFailMessage("Authentication failed.");
                    });
                    co_return;
                }

                auto authToken = std::move(authResult);
                auto req = geode::utils::web::WebRequest();
                auto url = fmt::format("{}/userInfo?accountId={}&argonToken={}", comment::baseUrl, accountId, authToken);

                auto response = co_await req.get(url);
                if (!response.ok()) {
                    geode::queueInMainThread([popup] {
                        popup->showFailMessage("Failed to fetch user info.");
                    });
                    co_return;
                }

                auto jsonRes = response.json();
                if (jsonRes.isErr()) {
                    geode::queueInMainThread([popup] {
                        popup->showFailMessage("Invalid response from server.");
                    });
                    co_return;
                }

                auto json = jsonRes.unwrap();
                bool isStaff = false;
                if (auto staffRes = json["is_staff"].asBool(); staffRes.isOk()) {
                    isStaff = staffRes.unwrap();
                }

                geode::queueInMainThread([popup, isStaff] {
                    if (isStaff) {
                        popup->onClose(nullptr);
                        if (auto adminPopup = CBAdminPanelLayer::create()) {
                            adminPopup->show();
                        }
                    } else {
                        popup->showFailMessage("You do not have staff permissions.");
                    }
                });
                co_return;
            });
        });

    if (authButton) {
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        this->addChildAtPosition(authButton, Anchor::TopLeft, {25.f, -80}, false);
    }

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