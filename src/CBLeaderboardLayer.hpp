#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/BasedButton.hpp>
#include <cue/ListNode.hpp>
#include <cue/LoadingCircle.hpp>

using namespace geode::prelude;

enum class LeaderboardTab {
    Earners = 1,
    Sales = 2,
};

class CBLeaderboardLayer : public CCLayer {
public:
    static CBLeaderboardLayer* create();
    static cocos2d::CCScene* scene();

private:
    bool init() override;
    void keyBackClicked() override;
    void onBack(CCObject* sender);
    void fetchLeaderboard();
    void createTabs();
    void updateTabSelection();
    void onTabSelected(CCObject* sender);

    Ref<CCSprite> m_background;
    Ref<cue::ListNode> m_list = nullptr;
    Ref<cue::LoadingCircle> m_loadingCircle = nullptr;
    std::vector<Ref<TabButton>> m_tabs;
    LeaderboardTab m_currentTab = LeaderboardTab::Earners;
};
