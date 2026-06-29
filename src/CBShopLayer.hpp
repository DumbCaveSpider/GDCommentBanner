#pragma once
#include <Geode/Geode.hpp>
#include <cue/LoadingCircle.hpp>
#include <cue/ListNode.hpp>
#include "CBBannerCell.hpp"
#include <Geode/binding/SetTextPopupDelegate.hpp>

using namespace geode::prelude;

class CBShopLayer : public CCLayer, public SetTextPopupDelegate {
public:
    static CBShopLayer* create();
    static CBShopLayer* getInstance();
    void updateAmethystLabel(int amethyst);
    void refreshBanners();
    void populateList();
    void onFilterClicked(CCObject* sender);
    void onSortClicked(CCObject* sender);
    void onSearchClicked(CCObject* sender);
    void onNextPage(CCObject* sender);
    void onPrevPage(CCObject* sender);
    void setEquippedBannerId(int bannerId);

    void setTextPopupClosed(SetTextPopup* popup, gd::string text) override;

private:
    bool init() override;
    ~CBShopLayer() override;
    void keyBackClicked() override;
    void fetchBanners();
    void fetchAmethyst();

    static CBShopLayer* s_instance;

    // members
    CCSprite* m_background;
    cue::ListNode* m_list = nullptr;
    CCCounterLabel* m_amethystLabel = nullptr;
    int m_equippedBannerId = -1;
    cue::LoadingCircle* m_loadingCircle = nullptr;
    bool m_isStaff = false;
    bool m_isAdmin = false;
    int m_filterState = 0; // 0 = All, 1 = Owned, 2 = Unowned, 3 = Limited
    int m_sortState = 0; // 0 = Recent, 1 = Most Bought, 2 = Most Equipped, 3 = Highest Price, 4 = Lowest Price
    int m_currentPage = 0;
    int m_itemsPerPage = 20;
    int m_totalItems = 0;
    std::string m_searchQuery = "";
    CCMenuItemSpriteExtra* m_searchButton = nullptr;
    CCMenuItemSpriteExtra* m_prevButton = nullptr;
    CCMenuItemSpriteExtra* m_nextButton = nullptr;
    CCLabelBMFont* m_pageLabel = nullptr;
    CCLabelBMFont* m_noBannersLabel = nullptr;
    std::vector<CBBannerItem> m_banners;
};