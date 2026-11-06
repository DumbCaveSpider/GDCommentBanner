#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/ui/Button.hpp>

using namespace geode::prelude;

class CBAdminPanelLayer;

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
    bool isFeatured = false;
    bool isSupporterOnly = false;
    std::string nonCompactImageUrl;  // optional, shown in non-compact comment cells
    int amount = 0;
    int totalBought = 0;
    int equippedCount = 0;
    bool isLocal = false;
    bool creatorIsSupporter = false;  // pending banners: the submitter is a supporter
    bool isResubmission = false;      // approved banner whose new image is waiting for review
    bool isFavorite = false;          // the viewer marked it as a favorite (shop lists only)
    // "YYYY-MM-DD hh:mm:ss", empty when unknown (banners from before these were tracked)
    std::string createdAt;
    std::string editedAt;
    std::string approvedAt;

    // reads a banner as returned by the server (getBanners, pendingBanners, admin lists)
    static CBBannerItem fromJson(matjson::Value const& json);
};

// the small icons next to a banner's name
enum class CBBannerIcon {
    SupporterOnly,
    Featured,
    Limited,
    Favorite,  // in the viewer's favorites
};

enum class CBBannerCellType {
    Shop,     // shop / profile / local banners: buy, equip, info (or equip/delete for local)
    Pending,  // admin panel, Pending tab: approve, reject, preview
    Admin,    // admin panel, Banners tab: preview, feature, manage
};

struct CBBannerCellOptions {
    CBBannerCellType type = CBBannerCellType::Shop;
    // show the plain banner image instead of the comment cell mockup
    bool disableCommentPreview = false;
    // Pending: the approve/reject popups report back to the admin panel
    CBAdminPanelLayer* adminPanel = nullptr;
    // Admin: raw banner json, passed to CBAdminBannerManagePopup
    matjson::Value adminData;
    // Admin: called after the banner was featured/unfeatured
    std::function<void(int bannerId, bool featured)> onFeaturedChanged;
};

class CBBannerCell : public cocos2d::CCLayer {
public:
    // disableCommentPreview: show the plain banner image instead of the comment cell mockup
    static CBBannerCell* create(const CBBannerItem& banner, float width, bool disableCommentPreview = false);
    static CBBannerCell* create(const CBBannerItem& banner, float width, CBBannerCellOptions options);
    // Comment cell mockup of the banner that fits inside maxSize, tap it to switch compact/non-compact
    // tappable icon, shows what it means when clicked
    static Button* createBannerIcon(CBBannerIcon icon);
    // the banner's icons left of a popup title and the matching title tint
    static void decoratePopupTitle(CCNode* mainLayer, Label* title, CBBannerItem const& banner);

    // preview settings, saved and applied to every comment preview on screen
    static bool isPreviewCompact();
    static void setPreviewCompact(bool compact);
    static bool isPreviewOverlay();
    static void setPreviewOverlay(bool overlay);

    static cocos2d::CCNode* createCommentPreviewButton(std::string const& imageUrl, cocos2d::CCSize const& maxSize, std::string const& nonCompactImageUrl = "");

private:
    void addShopButtons();
    void addPendingButtons();
    void addAdminButtons();
    // the right-hand button row of the admin cell types, every button sized alike
    CCMenu* createAdminMenu();
    void addAdminButton(CCMenu* menu, Button* button);
    void toggleFeatured(Button* button);

    void showPurchaseConfirm();
    void applyBanner();
    void unequipBanner();
    void purchaseBanner();

    CBBannerItem m_banner;
    CBBannerCellOptions m_options;
};
