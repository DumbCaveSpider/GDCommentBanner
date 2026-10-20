#pragma once
#include <Geode/Geode.hpp>
#include <cue/LoadingCircle.hpp>
#include <cue/ListNode.hpp>
#include <cue/RepeatingBackground.hpp>
#include "CBBannerCell.hpp"
#include "Geode/cocos/label_nodes/CCLabelBMFont.h"
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
    int getEquippedBannerId() const { return m_equippedBannerId; }
    void updateLocalEquippedState();
    void updateFilterSortLabel();

    void setTextPopupClosed(SetTextPopup* popup, gd::string text) override;

private:
    bool init() override;
    void onEnter() override;
    void onExit() override;
    bool isStaleFetch(int fetchId);
    ~CBShopLayer() override;
    void keyBackClicked() override;
    void fetchBanners();
    void fetchAmethyst();

    static CBShopLayer* s_instance;

    // members
    Ref<CCSprite> m_background;
    Ref<cue::ListNode> m_list = nullptr;
    Ref<CCCounterLabel> m_amethystLabel = nullptr;
    int m_equippedBannerId = -1;
    Ref<cue::LoadingCircle> m_loadingCircle = nullptr;
    bool m_isStaff = false;
    bool m_isAdmin = false;
    int m_filterState = 0;  // 0 = All, 1 = Owned, 2 = Unowned, 3 = Limited, 4 = Featured
    int m_sortState = 0;    // 0 = Recent, 1 = Most Bought, 2 = Most Equipped, 3 = Highest Price, 4 = Lowest Price
    int m_currentPage = 0;
    int m_itemsPerPage = 10;
    int m_totalItems = 0;
    int m_fetchId = 0;  // bumped on every fetchBanners, older responses are ignored
    std::string m_searchQuery = "";
    Ref<CCMenuItemSpriteExtra> m_searchButton = nullptr;
    Ref<CCMenuItemSpriteExtra> m_prevButton = nullptr;
    Ref<CCMenuItemSpriteExtra> m_nextButton = nullptr;
    Ref<CCLabelBMFont> m_pageLabel = nullptr;
    Ref<CCLabelBMFont> m_noBannersLabel = nullptr;
    Ref<CCLabelBMFont> m_filterSortLabel = nullptr;
    Ref<CCNode> m_localOverlay = nullptr;
    std::vector<CBBannerItem> m_banners;
};