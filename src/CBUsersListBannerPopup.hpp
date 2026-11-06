#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/web.hpp>

#include <cue/LoadingCircle.hpp>
#include <cue/ListNode.hpp>

using namespace geode::prelude;

class CBUsersListBannerPopup : public Popup {
private:
    int m_bannerId;
    int m_listType;  // 1 for bought, 2 for equipped
    cue::ListNode* m_list = nullptr;
    cue::LoadingCircle* m_loadingCircle = nullptr;

    int m_currentPage = 0;
    int m_itemsPerPage = 10;
    int m_totalItems = 0;
    CCMenuItemSpriteExtra* m_nextBtn = nullptr;
    CCMenuItemSpriteExtra* m_prevBtn = nullptr;
    Label* m_pageLabel = nullptr;

    bool init(int bannerId, int listType);
    void updatePaginationButtons();

    void onNextPage(cocos2d::CCObject* sender);
    void onPrevPage(cocos2d::CCObject* sender);

    void fetchUsers();

public:
    static CBUsersListBannerPopup* create(int bannerId, int listType);
};
