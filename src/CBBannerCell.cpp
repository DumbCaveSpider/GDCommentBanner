#include "CBBannerCell.hpp"
#include "CBPurchaseItemPopup.hpp"
#include "CBViewItemPopup.hpp"
#include "CBProfileBannerPopup.hpp"
#include "Geode/ui/Layout.hpp"
#include "ccTypes.h"
#include "include/CBConstant.hpp"
#include "include/CBCommentPreview.hpp"
#include "CBShopLayer.hpp"
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/utils/web.hpp>
#include <dankmeme.argon/argon.hpp>
#include "include/CBLocalBanner.hpp"
#include "CBLocalBannersPopup.hpp"
#include <Geode/ui/MDPopup.hpp>
#include "CBPreviewBanner.hpp"
#include "CBAdminPanelLayer.hpp"
#include "CBAdminBannerManagePopup.hpp"
#include "CBPendingBannerReasonPopup.hpp"

struct EquipRequest {
    int AccountID;
    std::string ArgonToken;
    int BannerID;
};

static constexpr auto kPreviewID = "cb-comment-preview";
static constexpr auto kPreviewCompactKey = "preview-compact";
static constexpr auto kPreviewOverlayKey = "preview-overlay";
static constexpr float kPreviewMaxHeight = 54.f;

// shows the compact or non-compact mockup depending on the saved preference
// (and shows/hides the comment drawn over the banner)
static void applyPreviewMode(CCNode* container) {
    bool compact = Mod::get()->getSavedValue<bool>(kPreviewCompactKey, false);
    bool overlay = Mod::get()->getSavedValue<bool>(kPreviewOverlayKey, true);
    if (auto normalPreview = container->getChildByTag(0)) {
        normalPreview->setVisible(!compact);
        comment::setCommentPreviewOverlay(normalPreview, overlay);
    }
    if (auto compactPreview = container->getChildByTag(1)) {
        compactPreview->setVisible(compact);
        comment::setCommentPreviewOverlay(compactPreview, overlay);
    }
}

static void applyPreviewModeRecursive(CCNode* node) {
    if (node->getID() == kPreviewID) {
        applyPreviewMode(node);
        return;
    }
    for (auto child : node->getChildrenExt()) {
        applyPreviewModeRecursive(child);
    }
}

static CCNode* createCellPreview(std::string const& imageUrl, CCSize const& maxSize, std::string const& nonCompactImageUrl) {
    auto container = CCNode::create();
    container->setID(kPreviewID);
    container->setAnchorPoint({0.5f, 0.5f});

    CCSize containerSize = CCSizeZero;
    for (bool compact : {false, true}) {
        auto preview = comment::createCommentPreview(imageUrl, compact, comment::kCommentCellWidth, nonCompactImageUrl);
        preview->setScale(std::min(maxSize.width / preview->getContentWidth(), maxSize.height / preview->getContentHeight()));
        preview->setTag(compact ? 1 : 0);
        container->addChild(preview);
        containerSize.width = std::max(containerSize.width, preview->getScaledContentWidth());
        containerSize.height = std::max(containerSize.height, preview->getScaledContentHeight());
    }
    container->setContentSize(containerSize);
    for (auto child : container->getChildrenExt()) {
        child->setPosition(containerSize / 2.f);
    }
    applyPreviewMode(container);
    return container;
}

Button* CBBannerCell::createBannerIcon(CBBannerIcon icon) {
    char const* frame = "GJ_sRecentIcon_001.png";
    char const* title = "Limited";
    char const* text =
        "This banner is <cp>Limited</c>: only a set number of copies can be bought. "
        "Once it's <cr>sold out</c>, nobody else can buy it.";
    switch (icon) {
        case CBBannerIcon::SupporterOnly:
            frame = "particle_230_001.png";
            title = "Supporter Only";
            text = "This banner is <cy>Supporter Only</c>: only <cp>Supporters</c> can buy and use it.";
            break;
        case CBBannerIcon::Featured:
            frame = "GJ_sStarsIcon_001.png";
            title = "Featured";
            text = "This banner was <cy>featured</c> by the <cg>staff team</c> for being high quality.";
            break;
        case CBBannerIcon::Limited:
            break;
        case CBBannerIcon::Favorite:
            frame = "particle_199_001.png";
            title = "Favorite";
            text =
                "This banner is in your <cy>favorites</c>. Use the <cg>Favorite</c> filter in the shop to find it, "
                "or change it from the banner's <cl>Info</c>.";
            break;
    }

    auto button = Button::createWithSpriteFrameName(frame, [title, text](Button*) {
        FLAlertLayer::create(title, text, "OK")->show();
    });
    // all icons 12 units tall (the star/limited icons at 0.8 scale)
    float height = button->getContentHeight();
    button->setScale(height > 0.f ? 12.f / height : 0.8f);
    return button;
}

void CBBannerCell::decoratePopupTitle(CCNode* mainLayer, Label* title, CBBannerItem const& banner) {
    // icons right to left, ending just before the title
    float x = -title->getScaledContentWidth() / 2.f - 5.f;
    auto addIcon = [&](CBBannerIcon type) {
        auto icon = createBannerIcon(type);
        icon->setAnchorPoint({1.f, 0.5f});
        mainLayer->addChildAtPosition(icon, Anchor::Top, {x, -20.f}, false);
        x -= icon->getScaledContentWidth() + 4.f;
    };
    if (banner.isLimited) addIcon(CBBannerIcon::Limited);
    if (banner.isFeatured) addIcon(CBBannerIcon::Featured);
    if (banner.isSupporterOnly) addIcon(CBBannerIcon::SupporterOnly);

    auto pulse = [&](ccColor3B a, ccColor3B b) {
        title->runAction(CCRepeatForever::create(CCSequence::create(
            CCTintTo::create(1.f, a.r, a.g, a.b),
            CCTintTo::create(1.f, b.r, b.g, b.b),
            nullptr)));
    };
    if (banner.isSupporterOnly) {
        pulse({0, 220, 255}, {255, 255, 255});
    } else if (banner.isFeatured && banner.isLimited) {
        pulse({255, 255, 50}, {255, 150, 255});
    } else if (banner.isFeatured) {
        pulse({255, 255, 50}, {255, 255, 255});
    } else if (banner.isLimited) {
        pulse({255, 150, 255}, {255, 255, 255});
    }
}

bool CBBannerCell::isPreviewCompact() {
    return Mod::get()->getSavedValue<bool>(kPreviewCompactKey, false);
}

void CBBannerCell::setPreviewCompact(bool compact) {
    Mod::get()->setSavedValue(kPreviewCompactKey, compact);
    if (auto scene = CCDirector::sharedDirector()->getRunningScene()) applyPreviewModeRecursive(scene);
}

bool CBBannerCell::isPreviewOverlay() {
    return Mod::get()->getSavedValue<bool>(kPreviewOverlayKey, true);
}

void CBBannerCell::setPreviewOverlay(bool overlay) {
    Mod::get()->setSavedValue(kPreviewOverlayKey, overlay);
    if (auto scene = CCDirector::sharedDirector()->getRunningScene()) applyPreviewModeRecursive(scene);
}

CCNode* CBBannerCell::createCommentPreviewButton(std::string const& imageUrl, CCSize const& maxSize, std::string const& nonCompactImageUrl) {
    return Button::createWithNode(createCellPreview(imageUrl, maxSize, nonCompactImageUrl), [](Button* sender) {
        // switch between compact and non-compact, applied to every visible preview
        auto compact = Mod::get()->getSavedValue<bool>(kPreviewCompactKey, false);
        Mod::get()->setSavedValue(kPreviewCompactKey, !compact);
        if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
            applyPreviewModeRecursive(scene);
        }
    });
}

CBBannerItem CBBannerItem::fromJson(matjson::Value const& json) {
    CBBannerItem item;
    item.id = static_cast<int>(json["id"].asInt().unwrapOr(0));
    item.accountId = static_cast<int>(json["accountId"].asInt().unwrapOr(0));
    item.name = json["name"].asString().unwrapOr("Unknown");
    item.description = json["description"].asString().unwrapOr("");
    item.username = json["username"].asString().unwrapOr("");
    item.imageUrl = json["imageUrl"].asString().unwrapOr("");
    item.nonCompactImageUrl = json["nonCompactImageUrl"].asString().unwrapOr("");
    item.price = static_cast<int>(json["price"].asInt().unwrapOr(0));
    item.owns = json["owns"].asBool().unwrapOr(false);
    item.isLimited = json["isLimited"].asBool().unwrapOr(false);
    item.isFeatured = json["isFeatured"].asBool().unwrapOr(false);
    item.isSupporterOnly = json["isSupporterOnly"].asBool().unwrapOr(false);
    item.amount = static_cast<int>(json["amount"].asInt().unwrapOr(0));
    item.totalBought = static_cast<int>(json["totalBought"].asInt().unwrapOr(0));
    item.equippedCount = static_cast<int>(json["equippedCount"].asInt().unwrapOr(0));
    item.creatorIsSupporter = json["creatorIsSupporter"].asBool().unwrapOr(false);
    item.isResubmission = json["isResubmission"].asBool().unwrapOr(false);
    item.isFavorite = json["isFavorite"].asBool().unwrapOr(false);
    item.createdAt = json["createdAt"].asString().unwrapOr("");
    item.editedAt = json["editedAt"].asString().unwrapOr("");
    item.approvedAt = json["approvedAt"].asString().unwrapOr("");
    return item;
}

CBBannerCell* CBBannerCell::create(const CBBannerItem& banner, float width, bool disableCommentPreview) {
    return create(banner, width, CBBannerCellOptions{.disableCommentPreview = disableCommentPreview});
}

CBBannerCell* CBBannerCell::create(const CBBannerItem& banner, float width, CBBannerCellOptions options) {
    static constexpr auto cellHeight = 96.f;
    auto cellBg = new CBBannerCell();
    cellBg->m_banner = banner;
    cellBg->m_options = std::move(options);
    if (!cellBg->init()) {
        delete cellBg;
        return nullptr;
    }
    cellBg->setContentSize({width, cellHeight});
    cellBg->autorelease();

    auto type = cellBg->m_options.type;
    bool isAdminCell = type != CBBannerCellType::Shop;
    bool isManage = type == CBBannerCellType::Manage;

    // @geode-ignore(unknown-resource)
    if (auto background = NineSlice::createWithSpriteFrameName("geode.loader/tab-bg.png")) {
        background->setContentSize({width - 5, cellHeight});
        background->setPosition({width / 2.f, cellHeight / 2.f});
        ccColor3B bgColor = {255, 255, 255};
        if (type == CBBannerCellType::Pending) {
            if (banner.creatorIsSupporter) {
                bgColor = {0, 220, 255};  // Cyan, submitted by a supporter
            }
        } else if (banner.equipped) {
            bgColor = {255, 165, 0};  // Orange
        } else if (banner.isSupporterOnly) {
            bgColor = {0, 220, 255};  // Cyan, supporter only
        } else if (!isAdminCell && (banner.owns || banner.isLocal)) {
            bgColor = {0, 200, 0};  // Green
        }
        background->setColor(bgColor);
        if (banner.isFeatured) {
            background->runAction(CCRepeatForever::create(CCSequence::create(
                CCTintTo::create(1.f, 255, 255, 50),
                CCTintTo::create(1.f, bgColor.r, bgColor.g, bgColor.b),
                nullptr)));
        }
        cellBg->addChild(background);
    }

    if (cellBg->m_options.disableCommentPreview) {
        auto sprite = comment::createBannerNode(banner.imageUrl, {width - 16.f, 104.f});
        sprite->setPosition({width / 2.f, cellHeight - 30.f});
        cellBg->addChild(sprite);
    } else if (auto previewButton = createCommentPreviewButton(banner.imageUrl, {width - 32.f, kPreviewMaxHeight}, banner.nonCompactImageUrl)) {
        previewButton->setPosition({width / 2.f, cellHeight - 27.f});
        cellBg->addChild(previewButton);
    }

    Label* nameLabel = nullptr;
    if (!banner.name.empty()) {
        nameLabel = Label::create(banner.name.c_str(), "bigFont.fnt");
        if (nameLabel) {
            nameLabel->setAnchorPoint({0.f, 0.5f});
            float nameX = 10.f;
            // tappable icons before the name, each explains what it means
            auto addIcon = [&](CBBannerIcon type) {
                auto icon = createBannerIcon(type);
                icon->setAnchorPoint({0.f, 0.5f});
                icon->setPosition({nameX, 23.f});
                cellBg->addChild(icon);
                nameX += icon->getScaledContentWidth() + 4.f;
            };
            // the manage cell has the name input there instead
            if (!isManage) {
                if (banner.isSupporterOnly) addIcon(CBBannerIcon::SupporterOnly);
                if (banner.isFeatured) addIcon(CBBannerIcon::Featured);
                if (banner.isLimited) addIcon(CBBannerIcon::Limited);
                if (banner.isFavorite) addIcon(CBBannerIcon::Favorite);
            }

            if (banner.isSupporterOnly) {
                nameLabel->runAction(CCRepeatForever::create(CCSequence::create(
                    CCTintTo::create(1.f, 0, 220, 255),
                    CCTintTo::create(1.f, 255, 255, 255),
                    nullptr)));
            } else if (banner.isFeatured && banner.isLimited) {
                nameLabel->runAction(CCRepeatForever::create(CCSequence::create(
                    CCTintTo::create(1.f, 255, 255, 50),
                    CCTintTo::create(1.f, 255, 150, 255),
                    nullptr)));
            } else if (banner.isFeatured) {
                nameLabel->runAction(CCRepeatForever::create(CCSequence::create(
                    CCTintTo::create(1.f, 255, 255, 50),
                    CCTintTo::create(1.f, 255, 255, 255),
                    nullptr)));
            } else if (banner.isLimited) {
                nameLabel->runAction(CCRepeatForever::create(CCSequence::create(
                    CCTintTo::create(1.f, 255, 150, 255),
                    CCTintTo::create(1.f, 255, 255, 255),
                    nullptr)));
            }
            nameLabel->setPosition({nameX, 25.f});
            nameLabel->setLimitLabelWidth(100.f, 0.5f, 0.2f);
            cellBg->addChild(nameLabel);

            if (isManage) {
                // editable instead: same spot, after the icons
                nameLabel->setVisible(false);
                cellBg->m_nameInput = TextInput::create(220.f, "Name", "bigFont.fnt");
                cellBg->m_nameInput->setCommonFilter(CommonFilter::Name);
                cellBg->m_nameInput->setString(banner.name);
                cellBg->m_nameInput->setScale(0.5f);
                cellBg->m_nameInput->setAnchorPoint({0.f, 0.5f});
                cellBg->m_nameInput->setPosition({nameX, 25.f});
                cellBg->addChild(cellBg->m_nameInput, 1);
            }
        }
    }

    if (isManage) {
        // the description instead of "By user" (the popup is already about that user)
        cellBg->m_descInput = TextInput::create(400.f, "Description", "chatFont.fnt");
        cellBg->m_descInput->setCommonFilter(CommonFilter::Any);
        cellBg->m_descInput->setString(banner.description);
        cellBg->m_descInput->setScale(0.4f);
        cellBg->m_descInput->setAnchorPoint({0.f, 0.5f});
        cellBg->m_descInput->setPosition({10.f, 10.f});
        cellBg->addChild(cellBg->m_descInput, 1);
    } else if (!banner.isLocal && !banner.username.empty()) {
        if (auto usernameLabel = Button::createWithLabel(fmt::format("By {}", banner.username).c_str(), "goldFont.fnt", [banner](Button* sender) {
                CBProfileBannerPopup::create(banner.accountId, banner.username)->show();
            })) {
            usernameLabel->setAnchorPoint({0.f, 0.5f});
            usernameLabel->setPosition({10.f, 10.f});
            usernameLabel->setScale(0.4f);
            cellBg->addChild(usernameLabel);
        }
    }

    // price, then (admin cells) the extra details in the same row
    float detailX = nameLabel ? nameLabel->getPositionX() + nameLabel->getScaledContentWidth() + 10.f : 20.f;
    if (cellBg->m_nameInput) {
        detailX = cellBg->m_nameInput->getPositionX() + cellBg->m_nameInput->getScaledContentWidth() + 6.f;
    }
    if (isManage) {
        // editable price with the amethyst after it
        cellBg->m_priceInput = TextInput::create(110.f, "Price", "bigFont.fnt");
        cellBg->m_priceInput->setCommonFilter(CommonFilter::Int);
        cellBg->m_priceInput->setString(numToString(banner.price));
        cellBg->m_priceInput->setScale(0.5f);
        cellBg->m_priceInput->setAnchorPoint({0.f, 0.5f});
        cellBg->m_priceInput->setPosition({detailX, 25.f});
        cellBg->addChild(cellBg->m_priceInput, 1);
        detailX += cellBg->m_priceInput->getScaledContentWidth() + 3.f;

        if (auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr)) {
            amethystIcon->setScale(0.5f);
            amethystIcon->setAnchorPoint({0.f, 0.5f});
            amethystIcon->setPosition({detailX, 25.f});
            cellBg->addChild(amethystIcon);
            detailX += amethystIcon->getScaledContentWidth() + 8.f;
        }
    } else if (!banner.isLocal) {
        if (auto price = Label::create(fmt::format("{}", GameToolbox::pointsToString(banner.price)).c_str(), "bigFont.fnt")) {
            price->setAnchorPoint({0.f, 0.5f});
            price->setScale(0.5f);
            price->setPosition({0.f, 0.f});

            auto priceNode = CCNode::create();
            priceNode->setPosition({detailX, 25.f});
            priceNode->addChild(price);
            detailX += price->getScaledContentWidth();

            if (auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr)) {
                amethystIcon->setScale(0.5f);
                auto priceWidth = price->getContentSize().width * price->getScale();
                amethystIcon->setPosition({priceWidth + 4.f, 0.f});
                amethystIcon->setAnchorPoint({0.f, 0.5f});
                priceNode->addChild(amethystIcon);
                detailX += 4.f + amethystIcon->getScaledContentWidth();
            }

            cellBg->addChild(priceNode);
            detailX += 8.f;
        }
    }

    if (isAdminCell && !isManage) {
        // an approved banner with a new image, rejecting restores the old one
        if (type == CBBannerCellType::Pending && banner.isResubmission) {
            if (auto resubLabel = Label::create("Image Change", "goldFont.fnt")) {
                resubLabel->setAnchorPoint({0.f, 0.5f});
                resubLabel->setScale(0.4f);
                resubLabel->setColor({0, 220, 255});
                resubLabel->setPosition({detailX, 25.f});
                cellBg->addChild(resubLabel);
                detailX += resubLabel->getScaledContentWidth() + 8.f;
            }
        }
        // limited stock (pending: what was requested)
        if (banner.isLimited) {
            if (auto amountLabel = Label::create(fmt::format("Amount: {}", banner.amount).c_str(), "goldFont.fnt")) {
                amountLabel->setAnchorPoint({0.f, 0.5f});
                amountLabel->setLimitLabelWidth(60.f, 0.4f, 0.2f);
                amountLabel->setPosition({detailX, 25.f});
                cellBg->addChild(amountLabel);
                detailX += amountLabel->getScaledContentWidth() + 8.f;
            }
        }
        // full banner details (description, preview, stats)
        if (auto descBtn = Button::createWithSpriteFrameName("GJ_infoIcon_001.png", [banner](Button*) {
                if (auto popup = CBViewItemPopup::create(banner)) {
                    popup->show();
                }
            })) {
            descBtn->setScale(0.5f);
            descBtn->setAnchorPoint({0.f, 0.5f});
            descBtn->setPosition({detailX, 25.f});
            cellBg->addChild(descBtn);
        }
    }

    switch (type) {
        case CBBannerCellType::Shop: cellBg->addShopButtons(); break;
        case CBBannerCellType::Pending: cellBg->addPendingButtons(); break;
        case CBBannerCellType::Admin: cellBg->addAdminButtons(); break;
        case CBBannerCellType::Manage: cellBg->addManageButtons(); break;
    }

    return cellBg;
}

void CBBannerCell::addShopButtons() {
    auto banner = m_banner;
    if (banner.isLocal) {
        if (auto equipButton = Button::createWithNode(ButtonSprite::create(banner.equipped ? "Unequip" : "Equip", 100.f, true, "goldFont.fnt", banner.equipped ? "GJ_button_06.png" : "GJ_button_02.png", .0f, 1.f), [banner](Button* sender) {
                if (banner.equipped) {
                    comment::local::unequipLocalBanner();
                } else {
                    comment::local::equipLocalBanner(banner.id);
                }
                if (auto popup = CBLocalBannersPopup::getInstance()) {
                    popup->fetchBanners();
                } else if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
                    if (auto popup = scene->getChildByType<CBLocalBannersPopup>(0)) {
                        popup->fetchBanners();
                    }
                }
                if (auto shop = CBShopLayer::getInstance()) {
                    shop->refreshBanners();
                }
            })) {
            equipButton->setScale(0.6f);
            this->addChildAtPosition(equipButton, Anchor::BottomRight, {-50.f, 15.f}, false);
        }

        if (auto deleteButton = Button::createWithNode(ButtonSprite::create("Delete", 100.f, true, "goldFont.fnt", "GJ_button_06.png", .0f, 1.f), [banner](Button* sender) {
                createQuickPopup("Delete Local Banner", "Are you sure you want to <cr>delete</c> this local banner from your device?", "Cancel", "Delete", [banner](FLAlertLayer*, bool btn2) {
                    if (!btn2) return;
                    comment::local::deleteLocalBanner(banner.id);
                    if (auto popup = CBLocalBannersPopup::getInstance()) {
                        popup->fetchBanners();
                    } else if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
                        if (auto popup = scene->getChildByType<CBLocalBannersPopup>(0)) {
                            popup->fetchBanners();
                        }
                    }
                    if (auto shop = CBShopLayer::getInstance()) {
                        shop->refreshBanners();
                    }
                });
            })) {
            deleteButton->setScale(0.6f);
            this->addChildAtPosition(deleteButton, Anchor::BottomRight, {-50.f, 35.f}, false);
        }
    } else {
        if (auto buyButton = Button::createWithNode(ButtonSprite::create(banner.equipped ? "Unequip" : (banner.owns ? "Equip" : "Buy"), 100.f, true, "goldFont.fnt", banner.equipped ? "GJ_button_06.png" : (banner.owns ? "GJ_button_02.png" : "GJ_button_01.png"), .0f, 1.f), [this, banner](Button* sender) {
                if (banner.equipped) {
                    this->unequipBanner();
                    return;
                }
                if (banner.owns) {
                    this->applyBanner();
                    return;
                }

                if (banner.isSupporterOnly && !comment::isUserSupporter()) {
                    FLAlertLayer::create(
                        "Supporter Only",
                        "You need to be a <cp>Supporter</c> to buy and use this banner.",
                        "OK")
                        ->show();
                    return;
                }

                auto cost = banner.price;
                auto current = Mod::get()->getSavedValue<int>("amethyst", 0);
                if (current < cost) {
                    createQuickPopup(
                        "Not enough Amethyst",
                        fmt::format("You need <cp>{} Amethyst</c> to <cg>buy this banner</c>.", cost - current),
                        "OK",
                        nullptr,
                        300.f,
                        [](FLAlertLayer* layer, bool btn2) {},
                        true,
                        true);
                    return;
                }
                if (auto popup = CBPurchaseItemPopup::create(this->m_banner)) {
                    popup->show();
                }
            })) {
            buyButton->setScale(0.6f);
            this->addChildAtPosition(buyButton, Anchor::BottomRight, {-50.f, 15.f}, false);
        }

        if (auto infoBtn = Button::createWithNode(ButtonSprite::create("Info", 100.f, true, "goldFont.fnt", "GJ_button_01.png", .0f, 1.f), [banner](Button* sender) {
                if (auto popup = CBViewItemPopup::create(banner)) {
                    popup->show();
                }
            })) {
            infoBtn->setScale(0.6f);
            this->addChildAtPosition(infoBtn, Anchor::BottomRight, {-50.f, 35.f}, false);
        }
    }
}

CCMenu* CBBannerCell::createAdminMenu() {
    // bottom right, same spot as the shop cell's buttons
    auto menu = CCMenu::create();
    menu->ignoreAnchorPointForPosition(false);
    menu->setAnchorPoint({1.f, 0.5f});
    menu->setContentSize({140.f, 30.f});
    menu->setLayout(RowLayout::create()
            ->setAxisAlignment(AxisAlignment::End)
            ->setGap(5.f)
            ->setAutoScale(false));
    this->addChildAtPosition(menu, Anchor::BottomRight, {-12.f, 25.f}, false);
    return menu;
}

void CBBannerCell::addAdminButton(CCMenu* menu, Button* button) {
    // every admin button the same height, whatever sprite it uses
    constexpr float kButtonHeight = 28.f;
    float height = button->getContentHeight();
    if (height > 0.f) button->setScale(kButtonHeight / height);
    menu->addChild(button);
    menu->updateLayout();
}

void CBBannerCell::addPendingButtons() {
    auto menu = createAdminMenu();
    int id = m_banner.id;
    // plain pointer: the panel owns this cell, a Ref here would keep the panel alive forever
    auto adminPanel = m_options.adminPanel;

    addAdminButton(menu, Button::createWithNode(CircleButtonSprite::createWithSpriteFrameName("GJ_completesIcon_001.png", 0.8f, CircleBaseColor::Green, CircleBaseSize::Small), [id, adminPanel](Button*) {
        if (auto popup = CBPendingBannerReasonPopup::create(id, false, adminPanel)) {
            popup->show();
        }
    }));

    addAdminButton(menu, Button::createWithNode(CircleButtonSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png", 0.8f, CircleBaseColor::Red, CircleBaseSize::Small), [id, adminPanel](Button*) {
        if (auto popup = CBPendingBannerReasonPopup::create(id, true, adminPanel)) {
            popup->show();
        }
    }));

    // preview on a comment cell (both images when it has a non-compact one)
    auto imageUrl = m_banner.imageUrl;
    auto nonCompactImageUrl = m_banner.nonCompactImageUrl;
    addAdminButton(menu, Button::createWithSpriteFrameName("dtrackBtn_search_001.png", [imageUrl, nonCompactImageUrl](Button*) {
        if (auto preview = CBPreviewBanner::create(imageUrl, nonCompactImageUrl)) {
            preview->show();
        }
    }));
}

void CBBannerCell::addAdminButtons() {
    auto menu = createAdminMenu();

    // preview of both comment modes, only when the banner has both images
    if (!m_banner.nonCompactImageUrl.empty()) {
        auto imageUrl = m_banner.imageUrl;
        auto nonCompactImageUrl = m_banner.nonCompactImageUrl;
        addAdminButton(menu, Button::createWithSpriteFrameName("dtrackBtn_search_001.png", [imageUrl, nonCompactImageUrl](Button*) {
            if (auto preview = CBPreviewBanner::create(imageUrl, nonCompactImageUrl)) {
                preview->show();
            }
        }));
    }

    // feature toggle: an empty star unfeatures, a full star features
    addAdminButton(menu, Button::createWithNode(CircleButtonSprite::createWithSpriteFrameName(m_banner.isFeatured ? "GJ_noStarsIcon_001.png" : "GJ_starsIcon_001.png", 1.0f, CircleBaseColor::Green, CircleBaseSize::Small), [this](Button* sender) { this->toggleFeatured(sender); }));

    auto adminData = m_options.adminData;
    addAdminButton(menu, Button::createWithSpriteFrameName("sideBtn_options_001.png", [adminData](Button*) {
        if (auto popup = CBAdminBannerManagePopup::create(adminData)) {
            popup->show();
        }
    }));
}

void CBBannerCell::addManageButtons() {
    auto menu = createAdminMenu();
    int id = m_banner.id;

    // save name / description / price
    addAdminButton(menu, Button::createWithNode(
        CircleButtonSprite::createWithSpriteFrameName("GJ_completesIcon_001.png", 0.8f, CircleBaseColor::Green, CircleBaseSize::Small),
        [this, id](Button*) {
            if (!m_options.onSave) return;
            m_options.onSave(id, m_nameInput->getString(), m_descInput->getString(),
                numFromString<int>(m_priceInput->getString()).unwrapOr(m_banner.price));
        }));

    // delete (admins only, greyed out for staff)
    auto deleteBtn = Button::createWithNode(
        CircleButtonSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png", 0.8f, CircleBaseColor::Red, CircleBaseSize::Small),
        [this, id](Button*) {
            if (!m_options.onDelete) return;
            auto onDelete = m_options.onDelete;
            createQuickPopup("Delete Banner", fmt::format("Are you sure you want to <cr>delete</c> <cy>{}</c>?", m_banner.name), "Cancel", "Delete",
                [onDelete, id](FLAlertLayer*, bool confirmed) {
                    if (confirmed) onDelete(id);
                });
        });
    if (!m_options.canDelete) {
        deleteBtn->setEnabled(false);
        deleteBtn->setCascadeColorEnabled(true);
        deleteBtn->setColor({120, 120, 120});
    }
    addAdminButton(menu, deleteBtn);
}

void CBBannerCell::toggleFeatured(Button* button) {
    bool newState = !m_banner.isFeatured;
    Ref<CBBannerCell> self = this;
    Ref<Button> btn = button;
    createQuickPopup(
        newState ? "Feature Banner" : "Unfeature Banner",
        newState
            ? fmt::format("Are you sure you want to <cy>feature</c> <cg>{}</c>?\nIt gets the <cy>featured</c> star and shows up in the <cl>Featured</c> filter.", m_banner.name)
            : fmt::format("Are you sure you want to <cr>unfeature</c> <cg>{}</c>?", m_banner.name),
        "Cancel", newState ? "Feature" : "Unfeature",
        [self, btn, newState](FLAlertLayer*, bool confirmed) {
            if (confirmed) self->setFeatured(btn, newState);
        });
}

void CBBannerCell::setFeatured(Button* button, bool newState) {
    auto accountData = argon::getGameAccountData();

    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, newState ? "Featuring banner..." : "Unfeaturing banner...");
    popup->show();

    Ref<CBBannerCell> self = this;
    Ref<Button> btn = button;
    int bannerId = m_banner.id;
    arc::spawn([self, btn, bannerId, newState, popup, accountData]() -> arc::Future<> {
        auto authToken = co_await comment::argonToken(accountData);
        if (authToken.empty()) {
            queueInMainThread([popup] {
                if (popup && popup->getParent()) popup->showFailMessage("Authentication failed.");
            });
            co_return;
        }

        auto body = matjson::makeObject({{"accountId", accountData.accountId},
            {"argonToken", authToken},
            {"bannerId", bannerId},
            {"featured", newState}});
        auto response = co_await utils::web::WebRequest().bodyJSON(body).post(fmt::format("{}/admin/setFeatured", comment::baseUrl));
        if (!response.ok()) {
            auto error = comment::getResponseMessage(response, "Failed to update banner.");
            queueInMainThread([popup, error] {
                if (popup && popup->getParent()) popup->showFailMessage(error);
            });
            co_return;
        }

        queueInMainThread([self, btn, bannerId, newState, popup] {
            self->m_banner.isFeatured = newState;
            self->m_options.adminData["isFeatured"] = newState;
            float scale = btn->getScale();
            btn->setDisplayNode(CircleButtonSprite::createWithSpriteFrameName(
                newState ? "GJ_noStarsIcon_001.png" : "GJ_starsIcon_001.png", 1.0f, CircleBaseColor::Green, CircleBaseSize::Small));
            btn->setScale(scale);
            if (self->m_options.onFeaturedChanged) self->m_options.onFeaturedChanged(bannerId, newState);
            if (popup && popup->getParent()) {
                popup->showSuccessMessage(newState ? "Banner featured!" : "Banner unfeatured.");
            }
        });
        co_return;
    });
}

void CBBannerCell::showPurchaseConfirm() {
    Ref<CBBannerCell> retainedSelf = this;
    createQuickPopup(
        "Confirm Purchase",
        fmt::format("Buy banner #{} for {} amethyst?", m_banner.id, m_banner.price),
        "Cancel",
        "Buy",
        300.f,
        [retainedSelf](FLAlertLayer* layer, bool btn2) {
            if (btn2) {
                retainedSelf->purchaseBanner();
            }
        },
        true,
        false);
}

void CBBannerCell::applyBanner() {
    Ref<CBBannerCell> retainedSelf = this;
    queueInMainThread([retainedSelf]() {
        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;
        Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Equipping banner...");
        if (popup) {
            popup->show();
        }

        arc::spawn([retainedSelf, accountId, accountData, popup]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                log::warn("argon failed");
                co_return;
            }

            auto authToken = std::move(authResult);

            EquipRequest reqBody{
                accountId,
                std::move(authToken),
                retainedSelf->m_banner.id,
            };

            arc::spawn([retainedSelf, reqBody = std::move(reqBody), popup]() -> arc::Future<> {
                auto request = utils::web::WebRequest();
                auto body = matjson::makeObject({{"accountId", reqBody.AccountID},
                    {"argonToken", reqBody.ArgonToken},
                    {"bannerId", reqBody.BannerID}});
                auto response = co_await request.bodyJSON(body).post(fmt::format("{}/equipBanner", comment::baseUrl));

                if (!response.ok()) {
                    log::warn("equipBanner failed: {}", response.errorMessage());
                    queueInMainThread([popup, error = response.errorMessage()] {
                        if (popup && popup->getParent()) {
                            popup->showFailMessage(fmt::format("Equip failed: {}", error));
                        }
                    });
                    co_return;
                }

                queueInMainThread([retainedSelf, popup] {
                    if (popup && popup->getParent()) {
                        popup->showSuccessMessage("Banner equipped successfully");
                    }
                    Mod::get()->setSavedValue("equipped-banner", retainedSelf->m_banner.id);
                    int myAccountId = GJAccountManager::sharedState()->m_accountID;
                    if (myAccountId <= 0) myAccountId = argon::getGameAccountData().accountId;
                    comment::updateBannerCache(myAccountId, true, retainedSelf->m_banner.imageUrl, retainedSelf->m_banner.nonCompactImageUrl);
                    if (auto shop = CBShopLayer::getInstance()) {
                        shop->setEquippedBannerId(retainedSelf->m_banner.id);
                        shop->refreshBanners();
                    }
                    if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
                        if (auto popup = scene->getChildByType<CBProfileBannerPopup>(0)) {
                            popup->fetchBanners();
                        }
                    }
                });
                log::debug("banner {} equipped successfully", retainedSelf->m_banner.id);
                co_return;
            });
            co_return;
        });
    });
}

void CBBannerCell::unequipBanner() {
    Ref<CBBannerCell> retainedSelf = this;
    queueInMainThread([retainedSelf]() {
        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;
        Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Unequipping banner...");
        if (popup) {
            popup->show();
        }

        arc::spawn([retainedSelf, accountId, accountData, popup]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                log::warn("argon failed");
                co_return;
            }

            auto authToken = std::move(authResult);

            auto request = utils::web::WebRequest();
            auto body = matjson::makeObject({{"accountId", accountId},
                {"argonToken", authToken}});
            auto response = co_await request.bodyJSON(body).post(fmt::format("{}/unequipBanner", comment::baseUrl));

            if (!response.ok()) {
                log::warn("unequipBanner failed: {}", response.errorMessage());
                queueInMainThread([popup, error = response.errorMessage()] {
                    if (popup && popup->getParent()) {
                        popup->showFailMessage(fmt::format("Unequip failed: {}", error));
                    }
                });
                co_return;
            }

            queueInMainThread([popup] {
                if (popup && popup->getParent()) {
                    popup->showSuccessMessage("Banner unequipped successfully");
                }
                Mod::get()->setSavedValue("equipped-banner", -1);
                int myAccountId = GJAccountManager::sharedState()->m_accountID;
                if (myAccountId <= 0) myAccountId = argon::getGameAccountData().accountId;
                comment::updateBannerCache(myAccountId, false, "");
                if (auto shop = CBShopLayer::getInstance()) {
                    shop->setEquippedBannerId(-1);
                    shop->refreshBanners();
                }
                if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
                    if (auto popup = scene->getChildByType<CBProfileBannerPopup>(0)) {
                        popup->fetchBanners();
                    }
                }
            });
            log::debug("banner {} unequipped successfully", retainedSelf->m_banner.id);
            co_return;
        });
    });
}

void CBBannerCell::purchaseBanner() {
    Ref<CBBannerCell> retainedSelf = this;
    queueInMainThread([retainedSelf]() {
        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;
        Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Equipping banner...");
        if (popup) {
            popup->show();
        }

        arc::spawn([retainedSelf, accountId, accountData, popup]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                log::warn("argon failed");
                co_return;
            }

            auto authToken = std::move(authResult);
            auto current = Mod::get()->getSavedValue<int>("amethyst", 0);
            if (!retainedSelf->m_banner.owns && current < retainedSelf->m_banner.price) {
                co_return;
            }

            EquipRequest reqBody{
                accountId,
                std::move(authToken),
                retainedSelf->m_banner.id,
            };

            arc::spawn([retainedSelf, reqBody = std::move(reqBody), current, popup]() -> arc::Future<> {
                auto request = utils::web::WebRequest();
                auto body = matjson::makeObject({{"accountId", reqBody.AccountID},
                    {"argonToken", reqBody.ArgonToken},
                    {"bannerId", reqBody.BannerID}});
                auto response = co_await request.bodyJSON(body).post(fmt::format("{}/equipBanner", comment::baseUrl));

                if (!response.ok()) {
                    log::warn("equipBanner failed: {}", response.errorMessage());
                    queueInMainThread([popup, error = response.errorMessage()] {
                        if (popup && popup->getParent()) {
                            popup->showFailMessage(fmt::format("Equip failed: {}", error));
                        }
                    });
                    co_return;
                }

                if (!retainedSelf->m_banner.owns) {
                    Mod::get()->setSavedValue("amethyst", current - retainedSelf->m_banner.price);
                }
                queueInMainThread([retainedSelf, popup] {
                    if (popup && popup->getParent()) {
                        popup->showSuccessMessage("Banner equipped successfully");
                    }
                    if (auto shop = CBShopLayer::getInstance()) {
                        shop->setEquippedBannerId(retainedSelf->m_banner.id);
                        shop->refreshBanners();
                    }
                });
                log::debug("banner {} equipped successfully", retainedSelf->m_banner.id);
                co_return;
            });
            co_return;
        });
    });
}
