#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <cue/ListNode.hpp>
#include <cue/LoadingCircle.hpp>

using namespace geode::prelude;

class CBAdminPanelLayer : public geode::Popup {
public:
    static CBAdminPanelLayer* create();

private:
    bool init();
    
    void fetchPendingBanners();
    void fetchUsers();
    
    void onTabPending(CCObject*);
    void onTabUsers(CCObject*);

    cue::ListNode* m_list = nullptr;
    cue::LoadingCircle* m_loadingCircle = nullptr;
    
    CCMenuItemToggler* m_tabPending = nullptr;
    CCMenuItemToggler* m_tabUsers = nullptr;

    enum class Tab { Pending, Users };
    Tab m_currentTab = Tab::Pending;
};
