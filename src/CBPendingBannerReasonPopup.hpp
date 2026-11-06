#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

class CBAdminPanelLayer;

class CBPendingBannerReasonPopup : public Popup {
protected:
    int m_bannerId;
    bool m_isReject;
    CBAdminPanelLayer* m_adminPanel;
    TextInput* m_reasonInput;

    bool init(int bannerId, bool isReject, CBAdminPanelLayer* adminPanel);
    void onConfirm(cocos2d::CCObject* sender);

public:
    static CBPendingBannerReasonPopup* create(int bannerId, bool isReject, CBAdminPanelLayer* adminPanel);
};
