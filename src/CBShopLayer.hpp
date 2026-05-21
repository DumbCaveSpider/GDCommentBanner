#pragma once
#include <Geode/Geode.hpp>
#include <cue/ListNode.hpp>

using namespace geode::prelude;

class CBShopLayer : public CCLayer {
public:
    static CBShopLayer* create();

private:
    bool init() override;
    void keyBackClicked() override;
    void fetchBanners();

    // members
    CCSprite* m_background;
    cue::ListNode* m_list = nullptr;
};