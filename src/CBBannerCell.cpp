#include "CBBannerCell.hpp"
#include "CBPurchaseItemPopup.hpp"
#include "Geode/ui/Layout.hpp"
#include "ccTypes.h"
#include "include/CBConstant.hpp"
#include "CBShopLayer.hpp"
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/utils/web.hpp>
#include <argon/argon.hpp>

struct EquipRequest {
    int AccountID;
    std::string ArgonToken;
    int BannerID;
};

CBBannerCell* CBBannerCell::create(const CBBannerItem& banner, float width) {
    static constexpr auto cellHeight = 96.f;
    auto cellBg = new CBBannerCell();
    cellBg->m_banner = banner;
    if (!cellBg->init()) {
        delete cellBg;
        return nullptr;
    }
    cellBg->setContentSize({width, cellHeight});
    cellBg->autorelease();

    // @geode-ignore(unknown-resource)
    if (auto background = NineSlice::createWithSpriteFrameName("geode.loader/tab-bg.png")) {
        background->setContentSize({width - 5, cellHeight});
        background->setPosition({width / 2.f, cellHeight / 2.f});
        background->setColor(banner.owns ? ccColor3B{0, 200, 0} : ccColor3B{255, 255, 255});
        cellBg->addChild(background);
    }

    auto sprite = LazySprite::create({324.f, 104.f}, true);
    sprite->setAutoResize(true);
    sprite->setScale(0.9f);
    sprite->setPosition({width / 2.f, cellHeight - 30.f});
    if (!banner.imageUrl.empty()) {
        sprite->loadFromUrl(banner.imageUrl, LazySprite::Format::kFmtWebp, false);
    }
    cellBg->addChild(sprite);

    CCLabelBMFont* nameLabel = nullptr;
    if (!banner.name.empty()) {
        nameLabel = CCLabelBMFont::create(banner.name.c_str(), "bigFont.fnt");
        if (nameLabel) {
            nameLabel->setAnchorPoint({0.f, 0.5f});
            float nameX = 10.f;
            if (banner.isLimited) {
                if (auto starIcon = CCSprite::createWithSpriteFrameName("GJ_sRecentIcon_001.png")) {
                    starIcon->setScale(0.8f);
                    starIcon->setPosition({nameX, 13.f});
                    starIcon->setAnchorPoint({0.f, 0.5f});
                    cellBg->addChild(starIcon);
                    nameLabel->runAction(CCRepeatForever::create(CCSequence::create(
                        CCTintTo::create(1.f, 255, 150, 255),
                        CCTintTo::create(1.f, 255, 255, 255),
                        nullptr)));
                    nameX += starIcon->getContentSize().width * starIcon->getScale() + 4.f;
                }
            }
            nameLabel->setPosition({nameX, 15.f});
            nameLabel->limitLabelWidth(100.f, 0.5f, 0.2f);
            cellBg->addChild(nameLabel);
        }
    }

    if (!banner.username.empty()) {
        if (auto usernameLabel = Button::createWithLabel(fmt::format("By {}", banner.username).c_str(), "goldFont.fnt", [banner](geode::Button* sender) {
                ProfilePage::create(banner.accountId, false)->show();
            })) {
            usernameLabel->setAnchorPoint({0.f, 0.5f});
            float usernameX = 10.f;
            if (nameLabel) {
                usernameX = nameLabel->getPositionX() + nameLabel->getContentSize().width * nameLabel->getScale() + 8.f;
            }
            usernameLabel->setPosition({usernameX, 15.f});
            usernameLabel->setScale(0.4f);
            cellBg->addChild(usernameLabel);
        }
    }

    if (auto price = CCLabelBMFont::create(fmt::format("{}", banner.price).c_str(), "bigFont.fnt")) {
        price->setAnchorPoint({0.f, 0.5f});
        price->setScale(0.5f);
        price->setPosition({-8.f, 0.f});

        auto priceNode = CCNode::create();
        priceNode->setPosition({20.f, 35.f});

        priceNode->addChild(price);

        if (auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr)) {
            amethystIcon->setScale(0.5f);
            auto priceWidth = price->getContentSize().width * price->getScale();
            amethystIcon->setPosition({priceWidth + 4.f, 0.f});
            priceNode->addChild(amethystIcon);
        }

        cellBg->addChild(priceNode);

        auto detailNode = CCNode::create();
        detailNode->setPosition({20.f, 10.f});

        if (banner.isLimited) {
            if (auto amountLabel = CCLabelBMFont::create(fmt::format("Amount Left: {}", banner.amount - banner.totalBought).c_str(), "goldFont.fnt")) {
                amountLabel->setAnchorPoint({1.f, 0.5f});
                amountLabel->limitLabelWidth(60.f, 0.4f, 0.2f);
                amountLabel->setPosition({0.f, 10.f});
                detailNode->addChild(amountLabel);
            }
        }

        if (auto totalBoughtLabel = CCLabelBMFont::create(fmt::format("Bought: {}", banner.totalBought).c_str(), "goldFont.fnt")) {
            totalBoughtLabel->setAnchorPoint({1.f, 0.5f});
            totalBoughtLabel->limitLabelWidth(60.f, 0.4f, 0.2f);
            totalBoughtLabel->setPosition({0.f, banner.isLimited ? -8.f : 0.f});
            detailNode->addChild(totalBoughtLabel);
        }

        if (detailNode->getChildrenCount() > 0) {
            cellBg->addChildAtPosition(detailNode, Anchor::BottomRight, {-90.f, 25.f}, false);
        }
    }

    if (auto buyButton = Button::createWithNode(ButtonSprite::create(banner.equipped ? "Unequip" : (banner.owns ? "Apply" : "Buy"), 100.f, true, "goldFont.fnt", banner.equipped ? "GJ_button_06.png" : (banner.owns ? "GJ_button_02.png" : "GJ_button_01.png"), .0f, 1.f), [cellBg, banner](geode::Button* sender) {
            if (banner.equipped) {
                cellBg->unequipBanner();
                return;
            }
            if (banner.owns) {
                cellBg->applyBanner();
                return;
            }

            auto cost = banner.price;
            auto current = Mod::get()->getSavedValue<int>("amethyst", 0);
            if (current < cost) {
                geode::createQuickPopup(
                    "Not enough amethyst",
                    fmt::format("You need {} more amethyst to buy this banner.", cost - current),
                    "OK",
                    nullptr,
                    300.f,
                    [](FLAlertLayer* layer, bool btn2) {},
                    true,
                    true);
                return;
            }
            if (auto popup = CBPurchaseItemPopup::create(cellBg->m_banner)) {
                popup->show();
            }
        })) {
        buyButton->setScale(0.6f);
        cellBg->addChildAtPosition(buyButton, Anchor::BottomRight, {-50.f, 15.f}, false);
    }

    if (banner.equipped) {
        if (auto equippedLabel = CCLabelBMFont::create("Equipped", "goldFont.fnt")) {
            equippedLabel->setScale(0.5f);
            equippedLabel->setColor({255, 215, 0});
            cellBg->addChildAtPosition(equippedLabel, Anchor::BottomRight, {-50.f, 35.f}, false);
        }
    }

    return cellBg;
}

void CBBannerCell::showPurchaseConfirm() {
    Ref<CBBannerCell> retainedSelf = this;
    geode::createQuickPopup(
        "Confirm Purchase",
        fmt::format("Buy banner #{} for {} amethyst?", m_banner.id, m_banner.price),
        "Cancel",
        "Buy",
        300.f,
        [retainedSelf](FLAlertLayer* layer, bool btn2) {
            if (btn2) {
                retainedSelf->purchaseBanner();
            }
        },
        true,
        false);
}

void CBBannerCell::onClosePopup(UploadActionPopup* popup) {
    popup->removeFromParent();
}

void CBBannerCell::applyBanner() {
    Ref<CBBannerCell> retainedSelf = this;
    geode::queueInMainThread([retainedSelf]() {
        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;
        Ref<UploadActionPopup> popup = nullptr;
        popup = UploadActionPopup::create(nullptr, "Equipping banner...");
        if (popup) {
            popup->show();
        }

        arc::spawn([retainedSelf, accountId, accountData, popup]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                log::warn("argon failed");
                co_return;
            }

            auto authToken = std::move(authResult);

            EquipRequest reqBody{
                accountId,
                std::move(authToken),
                retainedSelf->m_banner.id,
            };

            arc::spawn([retainedSelf, reqBody = std::move(reqBody), popup]() -> arc::Future<> {
                auto request = geode::utils::web::WebRequest();
                auto body = matjson::makeObject({{"accountId", reqBody.AccountID},
                    {"argonToken", reqBody.ArgonToken},
                    {"bannerId", reqBody.BannerID}});
                auto response = co_await request.bodyJSON(body).post(fmt::format("{}/equipBanner", comment::baseUrl));

                if (!response.ok()) {
                    log::warn("equipBanner failed: {}", response.errorMessage());
                    if (popup) {
                        geode::queueInMainThread([popup, error = response.errorMessage()] {
                            popup->showFailMessage(fmt::format("Equip failed: {}", error));
                        });
                    }
                    co_return;
                }

                if (popup) {
                    geode::queueInMainThread([popup] {
                        popup->showSuccessMessage("Banner equipped successfully");
                    });
                }
                if (auto shop = CBShopLayer::getInstance()) {
                    shop->setEquippedBannerId(retainedSelf->m_banner.id);
                    shop->refreshBanners();
                }
                log::debug("banner {} equipped successfully", retainedSelf->m_banner.id);
                co_return;
            });
            co_return;
        });
    });
}

void CBBannerCell::unequipBanner() {
    Ref<CBBannerCell> retainedSelf = this;
    geode::queueInMainThread([retainedSelf]() {
        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;
        Ref<UploadActionPopup> popup = nullptr;
        popup = UploadActionPopup::create(nullptr, "Unequipping banner...");
        if (popup) {
            popup->show();
        }

        arc::spawn([retainedSelf, accountId, accountData, popup]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                log::warn("argon failed");
                co_return;
            }

            auto authToken = std::move(authResult);

            auto request = geode::utils::web::WebRequest();
            auto body = matjson::makeObject({{"accountId", accountId},
                {"argonToken", authToken}});
            auto response = co_await request.bodyJSON(body).post(fmt::format("{}/unequipBanner", comment::baseUrl));

            if (!response.ok()) {
                log::warn("unequipBanner failed: {}", response.errorMessage());
                if (popup) {
                    geode::queueInMainThread([popup, error = response.errorMessage()] {
                        popup->showFailMessage(fmt::format("Unequip failed: {}", error));
                    });
                }
                co_return;
            }

            if (popup) {
                geode::queueInMainThread([popup] {
                    popup->showSuccessMessage("Banner unequipped successfully");
                });
            }
            if (auto shop = CBShopLayer::getInstance()) {
                shop->setEquippedBannerId(-1);
                shop->refreshBanners();
            }
            log::debug("banner {} unequipped successfully", retainedSelf->m_banner.id);
            co_return;
        });
    });
}

void CBBannerCell::purchaseBanner() {
    Ref<CBBannerCell> retainedSelf = this;
    geode::queueInMainThread([retainedSelf]() {
        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;
        Ref<UploadActionPopup> popup = nullptr;
        popup = UploadActionPopup::create(retainedSelf, "Equipping banner...");
        if (popup) {
            popup->show();
        }

        arc::spawn([retainedSelf, accountId, accountData, popup]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                log::warn("argon failed");
                co_return;
            }

            auto authToken = std::move(authResult);
            auto current = Mod::get()->getSavedValue<int>("amethyst", 0);
            if (!retainedSelf->m_banner.owns && current < retainedSelf->m_banner.price) {
                co_return;
            }

            EquipRequest reqBody{
                accountId,
                std::move(authToken),
                retainedSelf->m_banner.id,
            };

            arc::spawn([retainedSelf, reqBody = std::move(reqBody), current, popup]() -> arc::Future<> {
                auto request = geode::utils::web::WebRequest();
                auto body = matjson::makeObject({{"accountId", reqBody.AccountID},
                    {"argonToken", reqBody.ArgonToken},
                    {"bannerId", reqBody.BannerID}});
                auto response = co_await request.bodyJSON(body).post(fmt::format("{}/equipBanner", comment::baseUrl));

                if (!response.ok()) {
                    log::warn("equipBanner failed: {}", response.errorMessage());
                    if (popup) {
                        geode::queueInMainThread([popup, error = response.errorMessage()] {
                            popup->showFailMessage(fmt::format("Equip failed: {}", error));
                        });
                    }
                    co_return;
                }

                if (!retainedSelf->m_banner.owns) {
                    Mod::get()->setSavedValue("amethyst", current - retainedSelf->m_banner.price);
                }
                if (popup) {
                    geode::queueInMainThread([popup] {
                        popup->showSuccessMessage("Banner equipped successfully");
                    });
                }
                log::debug("banner {} equipped successfully", retainedSelf->m_banner.id);
                co_return;
            });
            co_return;
        });
    });
}
