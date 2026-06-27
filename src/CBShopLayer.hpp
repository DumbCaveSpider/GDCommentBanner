#pragma once
#include <Geode/Geode.hpp>
#include <cue/LoadingCircle.hpp>
#include <cue/ListNode.hpp>

using namespace geode::prelude;

class CBShopLayer : public CCLayer {
public:
    static CBShopLayer* create();
    static CBShopLayer* getInstance();
    void updateAmethystLabel(int amethyst);
    void refreshBanners();
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
};