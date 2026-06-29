#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <cue/ListNode.hpp>
#include <cue/LoadingCircle.hpp>
#include <Geode/ui/Button.hpp>

using namespace geode::prelude;

#include <Geode/binding/SetTextPopupDelegate.hpp>

class CBAdminPanelLayer : public geode::Popup, public SetTextPopupDelegate {
public:
    static CBAdminPanelLayer* create();

private:
    bool init();

    void fetchPendingBanners();
    void fetchUsers(std::string query = "");
    void fetchBanners(std::string query = "");

    void setTextPopupClosed(SetTextPopup* popup, gd::string text) override;

    void onTabPending(CCObject*);
    void onTabUsers(CCObject*);
    void onTabBanners(CCObject*);

    cue::ListNode* m_list = nullptr;
    cue::LoadingCircle* m_loadingCircle = nullptr;

    CCMenuItemToggler* m_tabPending = nullptr;
    CCMenuItemToggler* m_tabUsers = nullptr;
    geode::Button* m_filterBtn = nullptr;

    enum class Tab { Pending, Users, Banners };
    Tab m_currentTab = Tab::Pending;

    void renderPage();
    void onNextPage(CCObject*);
    void onPrevPage(CCObject*);
    void updatePagination();

    matjson::Value m_currentData;
    std::string m_authToken;
    int m_page = 0;
    CCLabelBMFont* m_pageLabel = nullptr;
    CCMenuItemSpriteExtra* m_nextBtn = nullptr;
    CCMenuItemSpriteExtra* m_prevBtn = nullptr;
    CCMenu* m_paginationMenu = nullptr;
};
