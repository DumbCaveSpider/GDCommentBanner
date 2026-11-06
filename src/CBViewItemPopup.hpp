#pragma once

#include <Geode/Geode.hpp>
#include "CBBannerCell.hpp"
#include <Geode/ui/Button.hpp>
#include <Geode/ui/Toggler.hpp>

using namespace geode::prelude;

class CBViewItemPopup : public Popup {
public:
    static CBViewItemPopup* create(const CBBannerItem& banner);

private:
    bool init(const CBBannerItem& banner);
    // saves the favorite on the server, puts the toggle back if that fails
    void setFavorite(Toggler* toggler, bool favorite);

    CBBannerItem m_banner;
};
