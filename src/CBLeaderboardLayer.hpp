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

    CCSprite* m_background;
    cue::ListNode* m_list = nullptr;
    cue::LoadingCircle* m_loadingCircle = nullptr;
};
