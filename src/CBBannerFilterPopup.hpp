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
    // toggles filter i (0 = All) and syncs the toggles with the shop's filters
    void onFilterToggled(int index);

    std::vector<Toggler*> m_filterToggles;
    std::vector<Toggler*> m_sortToggles;
    Toggler* m_supporterOnlyToggle = nullptr;
};
