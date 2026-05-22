#pragma once
#include <Geode/Geode.hpp>
#include <cue/ListNode.hpp>

using namespace geode::prelude;

class CBShopLayer : public CCLayer {
public:
    static CBShopLayer* create();
    static CBShopLayer* getInstance();
    void updateAmethystLabel(int amethyst);

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
};