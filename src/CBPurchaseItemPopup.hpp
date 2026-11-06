#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/Toggler.hpp>
#include "CBBannerCell.hpp"
#include <Geode/ui/Button.hpp>

using namespace geode::prelude;

class CBPurchaseItemPopup : public Popup {
public:
    static CBPurchaseItemPopup* create(const CBBannerItem& banner);

private:
    bool init(const CBBannerItem& banner);
    void buyBanner();

    CBBannerItem m_banner;
    Button* m_buyButton = nullptr;
};
