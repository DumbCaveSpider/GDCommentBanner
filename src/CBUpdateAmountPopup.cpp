#include "CBUpdateAmountPopup.hpp"
#include <Geode/utils/web.hpp>
#include <argon/argon.hpp>
#include "Geode/ui/General.hpp"
#include <Geode/binding/UploadActionPopup.hpp>
#include "include/CBConstant.hpp"

CBUpdateAmountPopup* CBUpdateAmountPopup::create(int bannerId, int currentAmount, int totalBought, std::function<void()> onSuccess) {
    auto ret = new CBUpdateAmountPopup();
    if (ret && ret->init(bannerId, currentAmount, totalBought, onSuccess)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool CBUpdateAmountPopup::init(int bannerId, int currentAmount, int totalBought, std::function<void()> onSuccess) {
    if (!Popup::init(260.f, 160.f, "GJ_square01.png")) return false;

    m_bannerId = bannerId;
    m_currentAmount = currentAmount;
    m_totalBought = totalBought;
    m_onSuccess = onSuccess;

    this->setTitle("Update Amount");
    addSideArt(m_mainLayer, SideArt::All, SideArtStyle::PopupBlue, false);

    m_amountInput = TextInput::create(100.f, "Amount", "bigFont.fnt");
    m_amountInput->setCommonFilter(CommonFilter::Int);
    m_amountInput->setLabel("Total Amount");
    m_amountInput->setString(numToString(m_currentAmount));
    m_mainLayer->addChildAtPosition(m_amountInput, Anchor::Center, CCPointZero, false);

    auto updateBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Update", "goldFont.fnt", "GJ_button_01.png", .8f),
        this,
        menu_selector(CBUpdateAmountPopup::onUpdate));

    auto menu = CCMenu::create();
    menu->addChild(updateBtn);
    m_mainLayer->addChildAtPosition(menu, Anchor::Bottom, {0.f, 25.f});

    return true;
}

void CBUpdateAmountPopup::onUpdate(CCObject*) {
    if (!m_amountInput || m_amountInput->getString().empty()) return;

    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Updating amount...");
    popup->show();
    int newAmount = numFromString<int>(m_amountInput->getString()).unwrapOr(0);
    if (newAmount < m_totalBought) {
        if (popup && popup->getParent()) {
            popup->showFailMessage("Amount cannot be less than total bought.");
        }
        return;
    }

    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;

    if (accountId <= 0) {
        if (popup && popup->getParent()) {
            popup->showFailMessage("Invalid account ID.");
        }
        return;
    }

    Ref<CBUpdateAmountPopup> retainedSelf = this;

    arc::spawn([retainedSelf, accountId, accountData, newAmount, popup]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            geode::queueInMainThread([popup] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage("Failed to retrieve auth token");
                }
            });
            co_return;
        }

        auto authToken = std::move(authResult);

        auto req = web::WebRequest();
        auto body = matjson::makeObject({{"accountId", accountId},
            {"argonToken", authToken},
            {"bannerId", retainedSelf->m_bannerId},
            {"amount", newAmount}});

        auto response = co_await req.bodyJSON(body).post(fmt::format("{}/updateBannerAmount", comment::baseUrl));

        if (response.ok()) {
            geode::queueInMainThread([retainedSelf, popup] {
                if (popup && popup->getParent()) {
                    popup->showSuccessMessage("Banner amount updated successfully!");
                }
                if (retainedSelf->m_onSuccess) retainedSelf->m_onSuccess();
                if (retainedSelf->getParent()) {
                    retainedSelf->onClose(nullptr);
                }
            });
        } else {
            std::string errorStr = comment::getResponseMessage(response, "Failed to update amount.");
            geode::queueInMainThread([popup, errorStr] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage(errorStr);
                }
            });
        }
        co_return;
    });
}
