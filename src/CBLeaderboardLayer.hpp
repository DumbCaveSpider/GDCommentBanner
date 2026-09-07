#pragma once
#include <Geode/Geode.hpp>
#include <cue/ListNode.hpp>
#include <cue/LoadingCircle.hpp>

using namespace geode::prelude;

class CBLeaderboardLayer : public CCLayer {
public:
    static CBLeaderboardLayer* create();
    static cocos2d::CCScene* scene();

private:
    bool init() override;
    void keyBackClicked() override;
    void onBack(CCObject* sender);
    void fetchLeaderboard();

    Ref<CCSprite> m_background;
    Ref<cue::ListNode> m_list = nullptr;
    Ref<cue::LoadingCircle> m_loadingCircle = nullptr;
};
