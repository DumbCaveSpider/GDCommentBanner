#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/binding/UploadPopupDelegate.hpp>
#include <Geode/ui/LazySprite.hpp>

using namespace geode::prelude;

struct CBBannerItem {
    std::string imageUrl;
    std::string name;
    std::string description;
    std::string username;
    int price = 0;
    int id = 0;
    int accountId = 0;
    bool owns = false;
    bool equipped = false;
    bool isLimited = false;
    int amount = 0;
    int totalBought = 0;
};

class CBBannerCell : public cocos2d::CCLayer, public UploadPopupDelegate {
public:
    static CBBannerCell* create(const CBBannerItem& banner, float width);

private:
    void showPurchaseConfirm();
    void applyBanner();
    void purchaseBanner();
    void onClosePopup(UploadActionPopup* popup) override;

    CBBannerItem m_banner;
};
