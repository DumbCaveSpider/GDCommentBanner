#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

class CBShopLayer : public CCLayer {
public:
    static CBShopLayer* create();

private:
    bool init() override;
    void keyBackClicked() override;

    // members
    CCSprite *m_background;
};