#include "CBBannerCell.hpp"
#include "include/CBConstant.hpp"
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
        cellBg->addChild(background);
    }

    auto sprite = LazySprite::create({104.f, 104.f}, true);
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
            nameLabel->setPosition({10.f, 15.f});
            nameLabel->setScale(0.5f);
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
            usernameLabel->setScale(0.5f);
            cellBg->addChild(usernameLabel);
        }
    }

    if (auto price = CCLabelBMFont::create(fmt::format("{}", banner.amethyst).c_str(), "bigFont.fnt")) {
        price->setAnchorPoint({0.f, 0.5f});
        price->setScale(0.5f);
        price->setPosition({10.f, 0.f});

        auto priceNode = CCNode::create();
        priceNode->setPosition({20.f, 35.f});

        if (auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr)) {
            amethystIcon->setPosition({0.f, 0.f});
            amethystIcon->setScale(0.5f);
            priceNode->addChild(amethystIcon);
        }

        priceNode->addChild(price);
        cellBg->addChild(priceNode);
    }

    if (auto buyButton = Button::createWithNode(ButtonSprite::create("Buy", "goldFont.fnt", "GJ_button_01.png"), [cellBg](geode::Button* sender) {
            auto cost = cellBg->m_banner.amethyst;
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
            cellBg->showPurchaseConfirm();
        })) {
        buyButton->setScale(0.6f);
        cellBg->addChildAtPosition(buyButton, Anchor::BottomRight, {-30.f, 15.f}, false);
    }

    return cellBg;
}

void CBBannerCell::showPurchaseConfirm() {
    geode::createQuickPopup(
        "Confirm Purchase",
        fmt::format("Buy banner #{} for {} amethyst?", m_banner.id, m_banner.amethyst),
        "Cancel",
        "Buy",
        300.f,
        [this](FLAlertLayer* layer, bool btn2) {
            if (btn2) {
                this->purchaseBanner();
            }
        },
        true,
        false);
}

void CBBannerCell::onClosePopup(UploadActionPopup* popup) {
    popup->removeFromParent();
}

void CBBannerCell::purchaseBanner() {
    arc::spawn([this]() -> arc::Future<> {
        auto authResult = co_await argon::startAuth();
        if (authResult.isErr()) {
            log::warn("argon failed: {}", authResult.unwrapErr());
            co_return;
        }

        auto authToken = std::move(authResult).unwrap();
        auto accountId = argon::getGameAccountData().accountId;
        if (accountId <= 0) {
            log::warn("invalid accountId from game account data");
            co_return;
        }

        auto current = Mod::get()->getSavedValue<int>("amethyst", 0);
        if (current < m_banner.amethyst) {
            co_return;
        }

        EquipRequest reqBody{
            accountId,
            std::move(authToken),
            m_banner.id,
        };

        auto request = geode::utils::web::WebRequest();
        auto body = matjson::makeObject({{"accountId", reqBody.AccountID},
            {"argonToken", reqBody.ArgonToken},
            {"bannerId", reqBody.BannerID}});
        auto popup = UploadActionPopup::create(this, "Equipping banner...");
        if (popup) {
            popup->show();
        }

        auto response = co_await request.bodyString(matjson::format_as(body)).post(fmt::format("{}/equipBanner", comment::baseUrl));

        if (!response.ok()) {
            log::warn("equipBanner failed: {}", response.errorMessage());
            if (popup) {
                geode::queueInMainThread([popup, error = response.errorMessage()] {
                    popup->showFailMessage(fmt::format("Equip failed: {}", error));
                });
            }
            co_return;
        }

        Mod::get()->setSavedValue("amethyst", current - m_banner.amethyst);
        if (popup) {
            geode::queueInMainThread([popup] {
                popup->showSuccessMessage("Banner equipped successfully");
            });
        }
        log::debug("banner {} equipped successfully", m_banner.id);
        co_return;
    });
}
