#pragma once

#include <Geode/Geode.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/TextInput.hpp>

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
    Favorite,
};

enum class CBBannerCellType {
    Shop,     // shop / profile / local banners
    Pending,  // approve, reject, preview
    Admin,    // preview, feature, manage
    Manage,   // name/description/price inputs, save, delete
};

struct CBBannerCellOptions {
    CBBannerCellType type = CBBannerCellType::Shop;
    bool disableCommentPreview = false;
    CBAdminPanelLayer* adminPanel = nullptr;
    matjson::Value adminData;
    std::function<void(int bannerId, bool featured)> onFeaturedChanged;
    std::function<void(int bannerId, std::string name, std::string description, int price)> onSave;
    std::function<void(int bannerId)> onDelete;
    bool canDelete = true;
};

class CBBannerCell : public cocos2d::CCLayer {
public:
    // show the plain banner image instead of the comment cell mockup
    static CBBannerCell* create(const CBBannerItem& banner, float width, bool disableCommentPreview = false);
    static CBBannerCell* create(const CBBannerItem& banner, float width, CBBannerCellOptions options);
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
    void addManageButtons();
    CCMenu* createAdminMenu();
    void addAdminButton(CCMenu* menu, Button* button);
    void toggleFeatured(Button* button);
    void setFeatured(Button* button, bool featured);

    void showPurchaseConfirm();
    void applyBanner();
    void unequipBanner();
    void purchaseBanner();

    CBBannerItem m_banner;
    CBBannerCellOptions m_options;
    TextInput* m_nameInput = nullptr;
    TextInput* m_descInput = nullptr;
    TextInput* m_priceInput = nullptr;
};
