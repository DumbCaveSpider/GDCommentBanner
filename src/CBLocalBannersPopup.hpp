#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <cue/ListNode.hpp>

using namespace geode::prelude;

class CBLocalBannersPopup : public geode::Popup {
public:
    static CBLocalBannersPopup* create();
    static CBLocalBannersPopup* getInstance();
    void fetchBanners();

private:
    bool init() override;
    ~CBLocalBannersPopup() override;
    void onAddBanner(CCObject* sender);

    static CBLocalBannersPopup* s_instance;
    cue::ListNode* m_list = nullptr;
    geode::Label* m_noBannersLabel = nullptr;
};
