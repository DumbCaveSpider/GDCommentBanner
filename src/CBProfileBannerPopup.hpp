#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/web.hpp>
#include <string>

#include <cue/LoadingCircle.hpp>
#include <cue/ListNode.hpp>

class CBProfileBannerPopup : public geode::Popup {
private:
    int m_targetAccountId;
    std::string m_targetUsername;
    cue::ListNode* m_list = nullptr;
    cue::LoadingCircle* m_loadingCircle = nullptr;

    int m_currentPage = 0;
    int m_itemsPerPage = 10;
    int m_totalItems = 0;
    CCMenuItemSpriteExtra* m_nextBtn = nullptr;
    CCMenuItemSpriteExtra* m_prevBtn = nullptr;
    cocos2d::CCLabelBMFont* m_pageLabel = nullptr;

    bool init(int targetAccountId, const std::string& targetUsername);
    void fetchBanners();
    void updatePaginationButtons();

    void onNextPage(cocos2d::CCObject* sender);
    void onPrevPage(cocos2d::CCObject* sender);

public:
    static CBProfileBannerPopup* create(int targetAccountId, const std::string& targetUsername = "");
};
