#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Toggler.hpp>

using namespace geode::prelude;

class CBBannerFilterPopup : public Popup {
public:
    static CBBannerFilterPopup* create();

private:
    bool init();
    void onFilterToggled(int index);

    std::vector<Toggler*> m_filterToggles;
    std::vector<Toggler*> m_sortToggles;
    Toggler* m_supporterOnlyToggle = nullptr;
};
