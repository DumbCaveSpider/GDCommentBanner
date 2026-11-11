#include "CBEditBannerPopup.hpp"
#include "CBSubmitBannerPopup.hpp"
#include "CBShopLayer.hpp"
#include <Geode/utils/web.hpp>
#include <dankmeme.argon/argon.hpp>
#include "Geode/ui/General.hpp"
#include <Geode/binding/UploadActionPopup.hpp>
#include "include/CBConstant.hpp"

CBEditBannerPopup* CBEditBannerPopup::create(CBEditBannerData data, std::function<void()> onSuccess) {
    auto ret = new CBEditBannerPopup();
    if (ret && ret->init(std::move(data), std::move(onSuccess))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBEditBannerPopup::init(CBEditBannerData data, std::function<void()> onSuccess) {
    if (!Popup::init(340.f, 250.f, "GJ_square01.png")) return false;

    m_data = std::move(data);
    m_onSuccess = std::move(onSuccess);

    this->setTitle("Edit Banner");
    addSideArt(m_mainLayer, SideArt::All, SideArtStyle::PopupBlue, false);

    auto menu = CCMenu::create();
    menu->setContentSize({300.f, 160.f});
    m_mainLayer->addChildAtPosition(menu, Anchor::Center, {0.f, 5.f}, {0.5f, 0.5f});

    // Name
    m_nameInput = TextInput::create(300.f, "Banner Name");
    m_nameInput->setCommonFilter(CommonFilter::Name);
    m_nameInput->setLabel("Banner Name");
    m_nameInput->setString(m_data.name);
    menu->addChildAtPosition(m_nameInput, Anchor::Center, {0.f, 50.f});

    // Description
    m_descInput = TextInput::create(300.f, "Description");
    m_descInput->setCommonFilter(CommonFilter::Any);
    m_descInput->setLabel("Banner Description");
    m_descInput->setString(m_data.description);
    menu->addChildAtPosition(m_descInput, Anchor::Center, {0.f, 5.f});

    // Price (+ Amount for limited banners)
    m_priceInput = TextInput::create(100.f, "Price");
    m_priceInput->setCommonFilter(CommonFilter::Int);
    m_priceInput->setLabel("Price");
    m_priceInput->setString(numToString(m_data.price));

    if (m_data.isLimited) {
        menu->addChildAtPosition(m_priceInput, Anchor::Center, {-60.f, -40.f});

        m_amountInput = TextInput::create(100.f, "Amount");
        m_amountInput->setCommonFilter(CommonFilter::Int);
        m_amountInput->setLabel("Total Amount");
        m_amountInput->setString(numToString(m_data.amount));
        menu->addChildAtPosition(m_amountInput, Anchor::Center, {60.f, -40.f});
    } else {
        menu->addChildAtPosition(m_priceInput, Anchor::Center, {0.f, -40.f});
    }

    // Bottom buttons
    auto btnMenu = CCMenu::create();
    btnMenu->setContentSize({300.f, 40.f});
    btnMenu->setLayout(RowLayout::create()->setGap(10.f));

    if (comment::isUserSupporter()) {
        auto changeImageBtn = Button::createWithNode(ButtonSprite::create("Change Image", "goldFont.fnt", "GJ_button_04.png", .8f), [this](Button* sender) { this->onChangeImage(sender); });
        btnMenu->addChild(changeImageBtn);
    }

    auto saveBtn = Button::createWithNode(ButtonSprite::create("Save", "goldFont.fnt", "GJ_button_01.png", .8f), [this](Button* sender) { this->onSave(sender); });
    btnMenu->addChild(saveBtn);

    if (comment::isUserSupporter()) {
        auto deleteBtn = Button::createWithNode(ButtonSprite::create("Delete", "goldFont.fnt", "GJ_button_06.png", .8f), [this](Button* sender) { this->onDelete(sender); });
        btnMenu->addChild(deleteBtn);
    }
    btnMenu->updateLayout();

    m_mainLayer->addChildAtPosition(btnMenu, Anchor::Bottom, {0.f, 25.f}, {0.5f, 0.5f});

    return true;
}

void CBEditBannerPopup::onChangeImage(CCObject*) {
    Ref<CBEditBannerPopup> retainedSelf = this;

    // carry over unsaved name/description/amount edits
    auto data = m_data;
    data.name = m_nameInput->getString();
    data.description = m_descInput->getString();
    if (m_amountInput) {
        data.amount = numFromString<int>(m_amountInput->getString()).unwrapOr(data.amount);
    }

    auto popup = CBSubmitBannerPopup::create(
        false,
        true,
        data,
        [retainedSelf]() {
            if (retainedSelf->m_onSuccess) retainedSelf->m_onSuccess();
            if (retainedSelf->getParent()) {
                retainedSelf->onClose(nullptr);
            }
        });
    if (popup) popup->show();
}

void CBEditBannerPopup::onDelete(CCObject*) {
    Ref<CBEditBannerPopup> retainedSelf = this;
    createQuickPopup(
        "Delete Banner",
        fmt::format("Are you sure you want to <cr>permanently delete</c> <cy>{}</c>?\nEveryone who bought it will lose it and get their <cp>amethysts refunded</c>. The refunds are <cr>taken from your balance</c> (the earnings from its sales). This cannot be undone.", m_data.name),
        "Cancel",
        "Delete",
        [retainedSelf](FLAlertLayer*, bool btn2) {
            if (btn2) retainedSelf->deleteBanner();
        });
}

void CBEditBannerPopup::deleteBanner() {
    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;
    if (accountId <= 0) {
        Notification::create("Invalid account ID.", NotificationIcon::Error)->show();
        return;
    }

    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Deleting banner...");
    popup->show();

    Ref<CBEditBannerPopup> retainedSelf = this;
    int bannerId = m_data.id;

    arc::spawn([retainedSelf, accountId, accountData, bannerId, popup]() -> arc::Future<> {
        auto authToken = co_await comment::argonToken(accountData);
        if (authToken.empty()) {
            queueInMainThread([popup] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage("Failed to retrieve auth token");
                }
            });
            co_return;
        }

        auto body = matjson::makeObject({{"accountId", accountId},
            {"argonToken", authToken},
            {"bannerId", bannerId}});

        auto req = web::WebRequest();
        auto response = co_await req.bodyJSON(body).post(fmt::format("{}/deleteBanner", comment::baseUrl));
        if (!response.ok()) {
            std::string errorStr = comment::getResponseMessage(response, "Failed to delete banner.");
            queueInMainThread([popup, errorStr] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage(errorStr);
                }
            });
            co_return;
        }

        int charged = 0;
        if (auto json = response.json(); json.isOk()) {
            charged = json.unwrap()["charged"].asInt().unwrapOr(0);
        }

        queueInMainThread([retainedSelf, popup, bannerId, charged] {
            if (charged > 0) {
                int newAmethyst = std::max(0, Mod::get()->getSavedValue<int>("amethyst", 0) - charged);
                Mod::get()->setSavedValue("amethyst", newAmethyst);
                if (auto shopLayer = CBShopLayer::getInstance()) {
                    shopLayer->updateAmethystLabel(newAmethyst);
                }
            }
            comment::invalidateBannerCache();
            if (auto shop = CBShopLayer::getInstance()) {
                if (shop->getEquippedBannerId() == bannerId) {
                    shop->setEquippedBannerId(-1);
                }
                shop->refreshBanners();
            }
            if (popup && popup->getParent()) {
                popup->showSuccessMessage("Banner deleted.");
            }
            if (retainedSelf->m_onSuccess) retainedSelf->m_onSuccess();
            if (retainedSelf->getParent()) {
                retainedSelf->onClose(nullptr);
            }
        });
        co_return;
    });
}

void CBEditBannerPopup::onSave(CCObject*) {
    auto name = m_nameInput->getString();
    auto desc = m_descInput->getString();
    if (name.empty() || desc.empty() || m_priceInput->getString().empty()) {
        Notification::create("Please fill out all required fields.", NotificationIcon::Error)->show();
        return;
    }

    int price = numFromString<int>(m_priceInput->getString()).unwrapOr(0);
    if (price <= 0) {
        Notification::create("Price must be greater than 0.", NotificationIcon::Error)->show();
        return;
    }

    int amount = 0;
    if (m_data.isLimited) {
        amount = numFromString<int>(m_amountInput->getString()).unwrapOr(0);
        if (amount < m_data.totalBought) {
            Notification::create("Amount cannot be less than total bought.", NotificationIcon::Error)->show();
            return;
        }
    }

    // raising the price into a higher fee bracket costs the fee difference
    int feeDifference = std::max(0, comment::submissionFeeForPrice(price) - comment::submissionFeeForPrice(m_data.price));
    if (feeDifference == 0) {
        submitEdit(name, desc, price, amount);
        return;
    }

    int currentAmethysts = Mod::get()->getSavedValue<int>("amethyst", 0);
    if (currentAmethysts < feeDifference) {
        Notification::create(fmt::format("Not enough amethysts! You need {} more.", feeDifference - currentAmethysts), NotificationIcon::Error)->show();
        return;
    }

    Ref<CBEditBannerPopup> retainedSelf = this;
    createQuickPopup(
        "Increased Submission Fee",
        fmt::format("Raising the price to <cp>{} amethysts</c> moves this banner into a higher submission fee bracket. You will be charged <cy>{} amethysts</c>.\nDo you want to continue?",
            GameToolbox::pointsToString(price),
            GameToolbox::pointsToString(feeDifference)),
        "Cancel",
        "Save",
        [retainedSelf, name, desc, price, amount](FLAlertLayer*, bool btn2) {
            if (!btn2) return;
            retainedSelf->submitEdit(name, desc, price, amount);
        });
}

void CBEditBannerPopup::submitEdit(std::string name, std::string desc, int price, int amount) {
    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;
    if (accountId <= 0) {
        Notification::create("Invalid account ID.", NotificationIcon::Error)->show();
        return;
    }

    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Saving banner...");
    popup->show();

    Ref<CBEditBannerPopup> retainedSelf = this;
    bool isLimited = m_data.isLimited;
    int bannerId = m_data.id;

    arc::spawn([retainedSelf, accountId, accountData, bannerId, name, desc, price, amount, isLimited, popup]() -> arc::Future<> {
        auto authToken = co_await comment::argonToken(accountData);
        if (authToken.empty()) {
            queueInMainThread([popup] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage("Failed to retrieve auth token");
                }
            });
            co_return;
        }

        utils::web::MultipartForm form;
        form.param("accountId", numToString(accountId));
        form.param("argonToken", authToken);
        form.param("bannerId", numToString(bannerId));
        form.param("name", name);
        form.param("description", desc);
        form.param("price", numToString(price));
        if (isLimited) {
            form.param("amount", numToString(amount));
        }

        auto req = web::WebRequest();
        req.bodyMultipart(std::move(form));
        auto response = co_await req.post(fmt::format("{}/editBanner", comment::baseUrl));

        if (!response.ok()) {
            std::string errorStr = comment::getResponseMessage(response, "Failed to update banner.");
            queueInMainThread([popup, errorStr] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage(errorStr);
                }
            });
            co_return;
        }

        int charged = 0;
        if (auto json = response.json(); json.isOk()) {
            charged = json.unwrap()["charged"].asInt().unwrapOr(0);
        }

        queueInMainThread([retainedSelf, popup, charged] {
            if (charged > 0) {
                int newAmethyst = std::max(0, Mod::get()->getSavedValue<int>("amethyst", 0) - charged);
                Mod::get()->setSavedValue("amethyst", newAmethyst);
                if (auto shopLayer = CBShopLayer::getInstance()) {
                    shopLayer->updateAmethystLabel(newAmethyst);
                }
            }
            if (popup && popup->getParent()) {
                popup->showSuccessMessage("Banner updated successfully!");
            }
            if (retainedSelf->m_onSuccess) retainedSelf->m_onSuccess();
            if (retainedSelf->getParent()) {
                retainedSelf->onClose(nullptr);
            }
        });
        co_return;
    });
}
