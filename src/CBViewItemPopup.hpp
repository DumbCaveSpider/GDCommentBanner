#pragma once

#include <Geode/Geode.hpp>
#include "CBBannerCell.hpp"
#include <Geode/ui/Button.hpp>

using namespace geode::prelude;

class CBViewItemPopup : public geode::Popup {
public:
    static CBViewItemPopup* create(const CBBannerItem& banner);

private:
    bool init(const CBBannerItem& banner);

    CBBannerItem m_banner;
};
