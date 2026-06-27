#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <cue/ListNode.hpp>
#include <cue/LoadingCircle.hpp>

using namespace geode::prelude;

class CBYourBannersPopup : public geode::Popup {
public:
    static CBYourBannersPopup* create();

private:
    bool init();
    void fetchBanners();

    cue::ListNode* m_list = nullptr;
    cue::LoadingCircle* m_loadingCircle = nullptr;
};
