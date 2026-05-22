#include <Geode/Enums.hpp>
#include <Geode/Geode.hpp>
#include <Geode/binding/CCCounterLabel.hpp>
#include <Geode/ui/Button.hpp>
#include "CBShopLayer.hpp"
#include "CBBannerCell.hpp"
#include "Geode/utils/web.hpp"
#include "ccTypes.h"
#include "include/CBConstant.hpp"
#include <argon/argon.hpp>
#include <cue/ListBorder.hpp>
#include <cue/ListNode.hpp>

using namespace geode::prelude;

CBShopLayer* CBShopLayer::s_instance = nullptr;

CBShopLayer* CBShopLayer::getInstance() {
    return s_instance;
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
            auto response = co_await req.bodyJSON(body).post(fmt::format("{}/getBanners", comment::baseUrl));
            if (!response.ok()) {
                geode::queueInMainThread([]() {
                    Notification::create("Failed to fetch banners.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto jsonRes = response.json();
            if (jsonRes.isErr()) {
                geode::queueInMainThread([]() {
                    Notification::create("Received invalid data for banners.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto json = jsonRes.unwrap();
            if (!json.isArray()) {
                geode::queueInMainThread([]() {
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
            Mod::get()->setSavedValue("amethyst", amethyst);
            geode::queueInMainThread([this, amethyst]() {
                if (!m_amethystLabel) {
                    return;
                }
                m_amethystLabel->setTargetCount(amethyst);
                m_amethystLabel->updateCounter(0.f);
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
    m_list = cue::ListNode::create({356, winSize.height - 60}, {0, 0, 0, 100}, cue::ListBorderStyle::None);
    if (m_list) {
        m_list->setPosition({winSize.width / 2.f, winSize.height / 2.f});
        this->addChild(m_list);
    }
    this->fetchBanners();
    this->fetchAmethyst();

    auto authButton = geode::Button::createWithNode(
        CircleButtonSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small),
        [](geode::Button* button) {
            geode::queueInMainThread([]() {
                auto accountData = argon::getGameAccountData();
                arc::spawn([accountData]() -> arc::Future<> {
                    auto authResult = co_await comment::argonToken(accountData);
                    if (authResult.empty()) {
                        geode::queueInMainThread([]() {
                            Notification::create("Failed to start authentication.", NotificationIcon::Error)->show();
                        });
                        co_return;
                    }

                    auto authToken = std::move(authResult);
                    geode::queueInMainThread([authToken = std::move(authToken)]() mutable {
                        auto accountId = argon::getGameAccountData().accountId;
                        if (accountId <= 0) {
                            geode::queueInMainThread([]() {
                                Notification::create("Failed to retrieve a valid account ID from the game data.", NotificationIcon::Error)->show();
                            });
                            return;
                        }

                        auto url = fmt::format(
                            "{}?accountId={}&argonToken={}",
                            comment::baseUrl,
                            accountId,
                            authToken);
                        utils::web::openLinkInBrowser(url.c_str());
                    });
                    co_return;
                });
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