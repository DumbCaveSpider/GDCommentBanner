#pragma once
#include <Geode/Geode.hpp>
#include <cue/LoadingCircle.hpp>
#include <cue/ListNode.hpp>
#include "CBBannerCell.hpp"

using namespace geode::prelude;

class CBShopLayer : public CCLayer {
public:
    static CBShopLayer* create();
    static CBShopLayer* getInstance();
    void updateAmethystLabel(int amethyst);
    void refreshBanners();
    void populateList();
    void onFilterClicked(CCObject* sender);
    void setEquippedBannerId(int bannerId);

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
    int m_filterState = 0; // 0 = All, 1 = Owned, 2 = Unowned
    std::vector<CBBannerItem> m_banners;
};