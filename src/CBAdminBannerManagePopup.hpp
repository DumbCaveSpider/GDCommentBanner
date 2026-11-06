#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/Button.hpp>
#include <matjson.hpp>

using namespace geode::prelude;

class CBAdminBannerManagePopup : public Popup {
protected:
    matjson::Value m_bannerData;
    TextInput* m_nameInput;
    TextInput* m_descInput;
    TextInput* m_priceInput;
    TextInput* m_amountInput = nullptr;

    bool init(matjson::Value const& bannerData);

    void onSave(cocos2d::CCObject*);
    void onDelete(cocos2d::CCObject*);

public:
    static CBAdminBannerManagePopup* create(matjson::Value const& bannerData);
};
