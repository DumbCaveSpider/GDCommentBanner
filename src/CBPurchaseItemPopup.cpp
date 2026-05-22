#include "CBPurchaseItemPopup.hpp"
#include "CBShopLayer.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Button.hpp>
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

    this->setTitle(m_banner.name.c_str());
    m_title->setFntFile("bigFont.fnt");

    if (m_closeBtn) m_closeBtn->removeFromParent();

    auto priceNode = CCNode::create();

    auto priceLabel = CCLabelBMFont::create(fmt::format("{}", m_banner.price).c_str(), "bigFont.fnt");
    if (priceLabel) {
        priceLabel->setAnchorPoint({1.f, 0.5f});
        priceLabel->setPosition({10.f, 0.f});
        priceLabel->setScale(0.45f);
        priceNode->addChild(priceLabel);

        if (auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr)) {
            amethystIcon->setAnchorPoint({0.f, 0.5f});
            amethystIcon->setScale(0.5f);
            amethystIcon->setPosition({15, 0.f});
            priceNode->addChild(amethystIcon);
        }

        m_mainLayer->addChildAtPosition(priceNode, Anchor::Top, {0.f, -45.f}, false);
    }

    if (!m_banner.imageUrl.empty()) {
        auto bannerSprite = LazySprite::create({200.f, 80.f}, true);
        if (bannerSprite) {
            bannerSprite->loadFromUrl(m_banner.imageUrl, LazySprite::Format::kFmtWebp, false);
            bannerSprite->setScale(0.9f);
            m_mainLayer->addChildAtPosition(bannerSprite, Anchor::Center, {0.f, 15.f}, false);
        }
    }

    auto descriptionLabel = SimpleTextArea::create(m_banner.description.c_str(), "chatFont.fnt");
    if (descriptionLabel) {
        descriptionLabel->setWidth(m_mainLayer->getContentSize().width - 40.f);
        descriptionLabel->setMaxLines(3);
        descriptionLabel->setScale(0.8);
        descriptionLabel->setAlignment(kCCTextAlignmentCenter);
        m_mainLayer->addChildAtPosition(descriptionLabel, Anchor::Center, {0.f, -30.f}, false);
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

        auto popup = UploadActionPopup::create(nullptr, "Purchasing banner...");
        if (popup) {
            popup->show();
        }

        arc::spawn([this, accountId, accountData, popup, itemPopup]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                if (popup) {
                    geode::queueInMainThread([popup] {
                        popup->showFailMessage("Authentication failed.");
                    });
                } else {
                    geode::queueInMainThread([]() {
                        Notification::create("Authentication failed.", NotificationIcon::Error)->show();
                    });
                }
                co_return;
            }

            auto authToken = std::move(authResult);
            auto request = geode::utils::web::WebRequest();
            auto body = matjson::makeObject({
                {"accountId", accountId},
                {"argonToken", authToken},
                {"bannerId", m_banner.id},
            });
            auto response = co_await request.bodyJSON(body).post(fmt::format("{}/buyBanner", comment::baseUrl));
            if (!response.ok()) {
                if (popup) {
                    geode::queueInMainThread([popup, response] {
                        popup->showFailMessage(comment::getResponseMessage(response, "Failed to purchase banner."));
                    });
                } else {
                    geode::queueInMainThread([response] {
                        Notification::create(comment::getResponseMessage(response, "Failed to purchase banner."), NotificationIcon::Error)->show();
                    });
                }
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
                if (popup) {
                    geode::queueInMainThread([popup, message = std::move(message)] {
                        popup->showFailMessage(message);
                    });
                } else {
                    geode::queueInMainThread([message = std::move(message)] {
                        Notification::create(message, NotificationIcon::Error)->show();
                    });
                }
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
                if (popup) {
                    popup->showSuccessMessage("Banner purchased successfully");
                }
                if (latestAmethyst >= 0) {
                    if (auto shop = CBShopLayer::getInstance()) {
                        shop->updateAmethystLabel(latestAmethyst);
                    }
                }
                itemPopup->onClose(nullptr);
            });

            co_return;
        });
    });
}
