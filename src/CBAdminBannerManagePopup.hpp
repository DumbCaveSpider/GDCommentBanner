#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/Button.hpp>
#include <matjson.hpp>

using namespace geode::prelude;

class CBAdminBannerManagePopup : public geode::Popup {
protected:
    matjson::Value m_bannerData;
    geode::TextInput* m_nameInput;
    geode::TextInput* m_descInput;
    geode::TextInput* m_priceInput;
    geode::TextInput* m_amountInput = nullptr;
    CCMenuItemToggler* m_featuredToggler = nullptr;

    bool init(matjson::Value const& bannerData);

    void onSave(cocos2d::CCObject*);
    void onDelete(cocos2d::CCObject*);

public:
    static CBAdminBannerManagePopup* create(matjson::Value const& bannerData);
};
