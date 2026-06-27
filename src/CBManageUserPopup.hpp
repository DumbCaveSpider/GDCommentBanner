#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/ui/TextInput.hpp>
#include "include/CBConstant.hpp"
#include <matjson.hpp>
#include <cue/ListNode.hpp>
#include <cue/LoadingCircle.hpp>

class CBManageUserPopup : public geode::Popup {
protected:
    matjson::Value m_userData;
    int m_targetAccountId;
    std::string m_username;
    bool m_isStaff;
    int m_amethyst;

    geode::TextInput* m_amethystInput;
    geode::TextInput* m_banHoursInput;
    CCMenuItemToggler* m_staffToggler;
    
    cue::ListNode* m_bannersList;
    cue::LoadingCircle* m_loadingCircle;

    bool init(matjson::Value const& userData);

    void onUpdateAmethyst(cocos2d::CCObject* sender);
    void onToggleStaff(cocos2d::CCObject* sender);
    void onTempBan(cocos2d::CCObject* sender);

    void fetchUserBanners();
    void createBannerCell(matjson::Value const& banner);
    void deleteBanner(int bannerId);
    void updateBannerPrice(int bannerId, geode::TextInput* input);

public:
    static CBManageUserPopup* create(matjson::Value const& userData);
};
