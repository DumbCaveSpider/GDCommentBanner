#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/Toggler.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/utils/web.hpp>
#include <string>

#include <cue/LoadingCircle.hpp>
#include <cue/ListNode.hpp>

using namespace geode::prelude;

class CBProfileBannerPopup : public Popup {
public:
    void fetchBanners();
private:
    void fetchStats();
    void showStats(matjson::Value const& user);

    CCNode* m_statsNode = nullptr;
    int m_targetAccountId;
    std::string m_targetUsername;
    cue::ListNode* m_list = nullptr;
    cue::LoadingCircle* m_loadingCircle = nullptr;

    int m_currentPage = 0;
    int m_itemsPerPage = 10;
    int m_totalItems = 0;
    Button* m_nextBtn = nullptr;
    Button* m_prevBtn = nullptr;
    Label* m_pageLabel = nullptr;

    bool init(int targetAccountId, const std::string& targetUsername);
    void updatePaginationButtons();

    void onNextPage(cocos2d::CCObject* sender);
    void onPrevPage(cocos2d::CCObject* sender);

public:
    static CBProfileBannerPopup* create(int targetAccountId, const std::string& targetUsername = "");
};
