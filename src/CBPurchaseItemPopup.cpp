#include "CBPurchaseItemPopup.hpp"
#include "CBShopLayer.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/MDTextArea.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/utils/web.hpp>
#include <argon/argon.hpp>
#include "include/CBConstant.hpp"

using namespace geode::prelude;

CBPurchaseItemPopup* CBPurchaseItemPopup::create(const CBBannerItem& banner) {
    auto popup = new CBPurchaseItemPopup();
    if (!popup || !popup->init(banner)) {
        delete popup;
        return nullptr;
    }
    popup->autorelease();
    return popup;
}

bool CBPurchaseItemPopup::init(const CBBannerItem& banner) {
    if (!Popup::init(380.f, 190.f, "GJ_square02.png")) {
        return false;
    }

    m_banner = banner;
    if (m_banner.owns == true) {
        Notification::create("You already own this banner.", NotificationIcon::Info)->show();
        return false;
    }

    this->setTitle(m_banner.name.c_str(), "bigFont.fnt", 0.7f);

    if (m_closeBtn) m_closeBtn->removeFromParent();

    auto priceNode = CCNode::create();
    priceNode->setScale(0.5f);
    priceNode->setContentSize({300.f, 30.f});
    priceNode->setAnchorPoint({0.5f, 0.5f});
    priceNode->setLayout(RowLayout::create()->setAxisAlignment(AxisAlignment::Center)->setGap(5));

    auto priceLabel = CCLabelBMFont::create(fmt::format("{}", GameToolbox::pointsToString(m_banner.price)).c_str(), "bigFont.fnt");
    if (priceLabel) {
        priceLabel->setAnchorPoint({1.f, 0.5f});
        priceLabel->limitLabelWidth(150.f, 0.45f, 0.2f);
        priceNode->addChild(priceLabel);

        if (auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr)) {
            amethystIcon->setAnchorPoint({0.f, 0.5f});
            amethystIcon->setScale(0.5f);
            priceNode->addChild(amethystIcon);
        }

        m_mainLayer->addChildAtPosition(priceNode, Anchor::Top, {0.f, -45.f}, false);
        priceNode->updateLayout();
    }

    if (!m_banner.imageUrl.empty()) {
        auto bannerSprite = comment::createBannerNode(m_banner.imageUrl, {345.f, 35.f});
        if (bannerSprite) {
            m_mainLayer->addChildAtPosition(bannerSprite, Anchor::Center, {0.f, 15.f}, false);
        }
    }

    auto descriptionLabel = geode::MDTextArea::create(m_banner.description, {m_mainLayer->getContentSize().width - 40.f, 45.f});
    if (descriptionLabel) {
        m_mainLayer->addChildAtPosition(descriptionLabel, Anchor::Center, {0.f, -25.f}, false);
    }

    auto cancelButton = geode::Button::createWithNode(
        ButtonSprite::create("Cancel", "goldFont.fnt", "GJ_button_06.png"),
        [this](geode::Button* sender) {
            this->onClose(nullptr);
        });
    if (cancelButton) {
        m_mainLayer->addChildAtPosition(cancelButton, Anchor::Bottom, {-50.f, 25.f}, false);
    }

    m_buyButton = geode::Button::createWithNode(
        ButtonSprite::create("Buy", "goldFont.fnt", "GJ_button_01.png"),
        [this](geode::Button* sender) {
            this->buyBanner();
        });
    if (m_buyButton) {
        m_buyButton->setPosition({180.f, 20.f});
        m_mainLayer->addChildAtPosition(m_buyButton, Anchor::Bottom, {55.f, 25.f}, false);
    }

    return true;
}

void CBPurchaseItemPopup::buyBanner() {
    auto itemPopup = Ref<CBPurchaseItemPopup>(this);
    geode::queueInMainThread([this, itemPopup]() {
        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;
        if (accountId <= 0) {
            Notification::create("Failed to retrieve a valid account ID.", NotificationIcon::Error)->show();
            return;
        }

        Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Purchasing banner...");
        if (popup) {
            popup->show();
        }

        auto bannerId = m_banner.id;
        arc::spawn([bannerId, accountId, accountData, popup, itemPopup]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                geode::queueInMainThread([popup] {
                    if (popup && popup->getParent()) {
                        popup->showFailMessage("Authentication failed.");
                    } else {
                        Notification::create("Authentication failed.", NotificationIcon::Error)->show();
                    }
                });
                co_return;
            }

            auto authToken = std::move(authResult);
            auto request = geode::utils::web::WebRequest();
            auto body = matjson::makeObject({
                {"accountId", accountId},
                {"argonToken", authToken},
                {"bannerId", bannerId},
            });
            auto response = co_await request.bodyJSON(body).post(fmt::format("{}/buyBanner", comment::baseUrl));
            if (!response.ok()) {
                geode::queueInMainThread([popup, response] {
                    if (popup && popup->getParent()) {
                        popup->showFailMessage(comment::getResponseMessage(response, "Failed to purchase banner."));
                    } else {
                        Notification::create(comment::getResponseMessage(response, "Failed to purchase banner."), NotificationIcon::Error)->show();
                    }
                });
                co_return;
            }

            auto jsonRes = response.json();
            if (jsonRes.isErr()) {
                geode::queueInMainThread([]() {
                    Notification::create("Invalid response from server.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto json = jsonRes.unwrap();
            if (!json["success"].asBool().unwrapOr(false)) {
                auto message = json["message"].asString().unwrapOr("Purchase failed.");
                geode::queueInMainThread([popup, message = std::move(message)] {
                    if (popup && popup->getParent()) {
                        popup->showFailMessage(message);
                    } else {
                        Notification::create(message, NotificationIcon::Error)->show();
                    }
                });
                co_return;
            }

            int latestAmethyst = -1;
            {
                auto userRequest = geode::utils::web::WebRequest();
                auto userBody = matjson::makeObject({
                    {"accountId", accountId},
                    {"argonToken", authToken},
                });
                auto userResponse = co_await userRequest.bodyJSON(userBody).post(fmt::format("{}/getUser", comment::baseUrl));
                if (userResponse.ok()) {
                    auto userJsonRes = userResponse.json();
                    if (userJsonRes.isOk()) {
                        auto userJson = userJsonRes.unwrap();
                        if (auto amRes = userJson["amethyst"].asInt(); amRes.isOk()) {
                            latestAmethyst = static_cast<int>(amRes.unwrap());
                            Mod::get()->setSavedValue("amethyst", latestAmethyst);
                        }
                    }
                }
            }

            geode::queueInMainThread([popup, itemPopup, latestAmethyst] {
                if (popup && popup->getParent()) {
                    popup->showSuccessMessage("Banner purchased successfully");
                }
                if (auto shop = CBShopLayer::getInstance()) {
                    if (latestAmethyst >= 0) {
                        shop->updateAmethystLabel(latestAmethyst);
                    }
                    shop->refreshBanners();
                }
                if (itemPopup && itemPopup->getParent()) {
                    itemPopup->onClose(nullptr);
                }
            });

            co_return;
        });
    });
}
