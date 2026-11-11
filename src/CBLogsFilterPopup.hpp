#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Toggler.hpp>
#include "include/CBConstant.hpp"
#include <set>

using namespace geode::prelude;

struct CBLogsFilter {
    std::set<int> categories;  // empty = all types
    bool oldestFirst = false;

    bool isDefault() const { return categories.empty() && !oldestFirst; }
};

class CBLogsFilterPopup : public Popup {
public:
    static CBLogsFilterPopup* create(CBLogsFilter filter, std::function<void(CBLogsFilter const&)> onChange);

private:
    bool init(CBLogsFilter filter, std::function<void(CBLogsFilter const&)> onChange);
    void apply();

    CBLogsFilter m_filter;
    std::function<void(CBLogsFilter const&)> m_onChange;
    std::vector<Toggler*> m_typeToggles;
    std::vector<Toggler*> m_orderToggles;
};
