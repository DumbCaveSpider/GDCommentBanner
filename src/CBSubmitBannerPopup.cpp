#include "CBSubmitBannerPopup.hpp"
#include "CBShopLayer.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <argon/argon.hpp>
#include "Geode/ui/General.hpp"
#include "Geode/utils/general.hpp"
#include "include/CBConstant.hpp"
#include "include/CBLocalBanner.hpp"
#include "CBLocalBannersPopup.hpp"
#include <Geode/utils/file.hpp>
#include <Geode/ui/NineSlice.hpp>
#include <algorithm>
#include <unordered_set>

using namespace geode::prelude;

CBSubmitBannerPopup* CBSubmitBannerPopup::create(bool isLocal) {
    auto ret = new CBSubmitBannerPopup();
    if (ret && ret->init(isLocal)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBSubmitBannerPopup::init(bool isLocal) {
    if (!Popup::init(380.f, 260.f)) return false;

    m_isLocal = isLocal;
    this->setTitle(m_isLocal ? "Add Local Banner" : "Submit Comment Banner");

    if (!m_isLocal) {
        addSideArt(m_mainLayer, SideArt::Top, SideArtStyle::PopupBlue, false);
        addSideArt(m_mainLayer, SideArt::BottomRight, SideArtStyle::PopupBlue, false);
    } else {
        addSideArt(m_mainLayer, SideArt::All, SideArtStyle::PopupBlue, false);
    }

    // Pick File Button
    auto pickFileBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Select Image", "goldFont.fnt", "GJ_button_04.png", .8f),
        this,
        menu_selector(CBSubmitBannerPopup::onPickFile));

    m_previewBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Preview", "goldFont.fnt", "GJ_button_04.png", .8f),
        this,
        menu_selector(CBSubmitBannerPopup::onPreview));
    m_previewBtn->setVisible(false);

    auto topMenu = CCMenu::create();
    topMenu->setContentSize({300.f, 40.f});
    topMenu->setLayout(RowLayout::create()->setGap(5.f));
    topMenu->addChild(pickFileBtn);
    topMenu->addChild(m_previewBtn);
    topMenu->updateLayout();

    m_buttonMenu->addChildAtPosition(topMenu, Anchor::Top, {0.f, -50.f}, {0.5, 0.5});

    m_fileNameLabel = CCLabelBMFont::create("No file selected (1500x150) (Static or Animated)", "chatFont.fnt");
    m_fileNameLabel->limitLabelWidth(m_mainLayer->getContentWidth() - 20.f, 0.8f, 0.1f);
    m_fileNameLabel->setColor({200, 200, 200});
    m_mainLayer->addChildAtPosition(m_fileNameLabel, Anchor::Top, {0.f, -75.f}, false);

    // Name
    m_nameInput = geode::TextInput::create(300.f, "Banner Name");
    m_nameInput->setCommonFilter(CommonFilter::Name);
    m_buttonMenu->addChildAtPosition(m_nameInput, Anchor::Center, {0.f, 30.f});

    // Description
    m_descInput = geode::TextInput::create(300.f, "Description");
    m_descInput->setCommonFilter(CommonFilter::Any);
    m_buttonMenu->addChildAtPosition(m_descInput, Anchor::Center, {0.f, -10.f});

    // Price & Limited row
    auto row1 = CCMenu::create();
    row1->setContentSize({300.f, 30.f});
    row1->setLayout(RowLayout::create()->setGap(10.f)->setAutoScale(false));

    m_priceInput = geode::TextInput::create(100.f, "Price");
    m_priceInput->setCommonFilter(CommonFilter::Int);
    row1->addChild(m_priceInput);

    m_amountInput = geode::TextInput::create(100.f, "Amount");
    m_amountInput->setCommonFilter(CommonFilter::Int);
    m_amountInput->setVisible(false);

    row1->addChild(m_amountInput);
    row1->updateLayout();

    m_buttonMenu->addChildAtPosition(row1, Anchor::Center, {0.f, -50.f}, {0.5, 0.5});

    auto costRow = CCMenu::create();
    costRow->setContentSize({300.f, 20.f});
    costRow->setLayout(RowLayout::create()->setGap(5.f)->setAutoScale(false));

    auto amethystSpr = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr);
    amethystSpr->setScale(0.6f);
    costRow->addChild(amethystSpr);

    bool hasCreatedBanner = Mod::get()->getSavedValue<bool>("has_created_banner", false);

    auto costLabel = CCLabelBMFont::create(
        hasCreatedBanner ? "Submission Cost: 15,000 amethysts" : "Submission Cost: Free on first banner",
        "bigFont.fnt");
    costLabel->setScale(0.35f);
    if (!hasCreatedBanner) {
        costLabel->setColor({100, 255, 100});  // Make it green if free
    }
    costRow->addChild(costLabel);

    costRow->updateLayout();
    m_mainLayer->addChildAtPosition(costRow, Anchor::Center, {0.f, -80.f}, {0.5, 0.5});

    auto limitedMenu = CCMenu::create();
    limitedMenu->setAnchorPoint({0.f, 0.f});
    limitedMenu->setContentSize({60.f, 40.f});
    limitedMenu->setLayout(RowLayout::create()->setGap(0.f)->setAutoScale(false));

    m_limitedToggler = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(CBSubmitBannerPopup::onToggleLimited), 0.7f);
    auto limitedLabel = CCLabelBMFont::create("Limited", "bigFont.fnt");
    limitedLabel->limitLabelWidth(limitedMenu->getContentWidth() - 10.f, 0.4f, 0.2f);

    limitedMenu->addChild(m_limitedToggler);
    limitedMenu->addChild(limitedLabel);
    limitedMenu->updateLayout();

    m_mainLayer->addChildAtPosition(limitedMenu, Anchor::BottomLeft, {5.f, 10.f});

    if (m_isLocal) {
        m_descInput->setVisible(false);
        row1->setVisible(false);
        costRow->setVisible(false);
        limitedMenu->setVisible(false);
        m_nameInput->setPositionY(m_nameInput->getPositionY() - 30);
    }

    // Submit Button
    auto submitBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Submit", "goldFont.fnt", "GJ_button_01.png", .8f),
        this,
        menu_selector(CBSubmitBannerPopup::onSubmit));
    m_buttonMenu->addChildAtPosition(submitBtn, Anchor::Bottom, {0.f, 25.f}, false);

    return true;
}

void CBSubmitBannerPopup::onToggleLimited(CCObject* sender) {
    bool toggled = !static_cast<CCMenuItemToggler*>(sender)->isToggled();
    if (m_amountInput) {
        m_amountInput->setVisible(toggled);
        if (auto parent = m_amountInput->getParent()) {
            parent->updateLayout();
        }
    }
}

void CBSubmitBannerPopup::onPickFile(CCObject*) {
    Ref<CBSubmitBannerPopup> retainedSelf = this;
    arc::spawn([retainedSelf]() -> arc::Future<> {
        std::unordered_set<std::string> allowedFiles = retainedSelf->m_isLocal
                                                           ? std::unordered_set<std::string>{"*.png", "*.webp", "*.gif"}
                                                           : std::unordered_set<std::string>{"*.png", "*.webp", "*.jpg", "*.jpeg", "*.gif"};

        auto result = co_await geode::utils::file::pick(
            geode::utils::file::PickMode::OpenFile,
            geode::utils::file::FilePickOptions{
                .defaultPath = std::nullopt,
                .filters = {
                    geode::utils::file::FilePickOptions::Filter{
                        .description = "Image Files",
                        .files = allowedFiles}}});

        auto notify = [&](std::string message) {
            geode::Loader::get()->queueInMainThread([message = std::move(message)]() {
                geode::Notification::create(message.c_str(), geode::NotificationIcon::Warning)->show();
            });
        };

        if (result.isOk()) {
            auto pathOpt = result.unwrap();
            if (pathOpt.has_value()) {
                auto path = pathOpt.value();

                auto extension = path.extension().string();
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                if (retainedSelf->m_isLocal) {
                    if (extension != ".png" && extension != ".webp" && extension != ".gif") {
                        notify("Please select a PNG, GIF or WEBP file");
                        co_return;
                    }
                } else {
                    if (extension != ".png" && extension != ".webp" && extension != ".jpg" && extension != ".jpeg" && extension != ".gif") {
                        notify("Please select a PNG, JPEG, GIF or WEBP file");
                        co_return;
                    }
                }

                if (extension != ".gif") {
                    CCImage image;
                    if (!image.initWithImageFile(path.string().c_str())) {
                        notify("Unable to decode image locally");
                        co_return;
                    }

                    if (image.getWidth() != 1500 || image.getHeight() != 150) {
                        notify("Image must be 1500x150, got " + numToString(image.getWidth()) + "x" + numToString(image.getHeight()));
                        co_return;
                    }
                }

                geode::Loader::get()->queueInMainThread([retainedSelf, p = path.string(), filename = path.filename().string()]() {
                    retainedSelf->m_selectedFilePath = p;
                    if (retainedSelf->m_fileNameLabel) {
                        retainedSelf->m_fileNameLabel->setString(filename.c_str());
                        retainedSelf->m_fileNameLabel->setColor({100, 255, 100});
                    }
                    if (retainedSelf->m_previewBtn) {
                        retainedSelf->m_previewBtn->setVisible(true);
                        if (auto menu = retainedSelf->m_previewBtn->getParent()) {
                            static_cast<CCMenu*>(menu)->updateLayout();
                        }
                    }
                });
            }
        }
    });
}

void CBSubmitBannerPopup::onSubmit(CCObject*) {
    if (m_isLocal) {
        if (m_selectedFilePath.empty()) {
            geode::Notification::create("Please select an image file.", geode::NotificationIcon::Error)->show();
            return;
        }
        if (m_nameInput->getString().empty()) {
            geode::Notification::create("Please enter a banner name.", geode::NotificationIcon::Error)->show();
            return;
        }

        if (comment::local::addLocalBanner(m_nameInput->getString(), m_selectedFilePath)) {
            geode::Notification::create("Local banner added successfully!", geode::NotificationIcon::Success)->show();
            if (auto popup = CBLocalBannersPopup::getInstance()) {
                popup->fetchBanners();
            } else if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
                if (auto popup = scene->getChildByType<CBLocalBannersPopup>(0)) {
                    popup->fetchBanners();
                }
            }
            this->onClose(nullptr);
        } else {
            geode::Notification::create("Failed to save local banner.", geode::NotificationIcon::Error)->show();
        }
        return;
    }

    if (m_selectedFilePath.empty()) {
        geode::Notification::create("Please select an image file.", geode::NotificationIcon::Error)->show();
        return;
    }
    if (m_nameInput->getString().empty() || m_descInput->getString().empty() || m_priceInput->getString().empty()) {
        geode::Notification::create("Please fill out all required fields.", geode::NotificationIcon::Error)->show();
        return;
    }
    bool isLimited = m_limitedToggler->isToggled();
    if (isLimited && m_amountInput->getString().empty()) {
        geode::Notification::create("Please specify an amount for the limited banner.", geode::NotificationIcon::Error)->show();
        return;
    }

    int priceVal = numFromString<int>(m_priceInput->getString()).unwrapOr(0);
    int multiplier = ((priceVal - 1) / 100000) + 1;
    if (multiplier < 1) multiplier = 1;
    int submissionFee = 15000 * multiplier;
    bool hasCreatedBanner = Mod::get()->getSavedValue<bool>("has_created_banner", false);
    if (!hasCreatedBanner) {
        submissionFee = std::max(0, submissionFee - 15000);
    }

    int currentAmethysts = Mod::get()->getSavedValue<int>("amethyst", 0);
    if (currentAmethysts < submissionFee) {
        int needed = submissionFee - currentAmethysts;
        geode::Notification::create(fmt::format("Not enough amethysts! You need {} more.", needed), geode::NotificationIcon::Error)->show();
        return;
    }

    std::string popupTitle = "Submit Banner";
    std::string popupMsg = "Are you sure you want to submit this banner? You <cr>cannot change it</c> after it's uploaded.";
    if (priceVal > 100000) {
        popupTitle = "Increased Submission Fee";
        popupMsg = fmt::format("Because this banner's price is higher than <cp>100,000 amethysts</c>, there is an increased submission fee of <cy>{} amethysts</c>.\nAre you sure you want to proceed? You <cr>cannot change it</c> after it's uploaded.", GameToolbox::pointsToString(submissionFee));
    }

    geode::createQuickPopup(popupTitle.c_str(), popupMsg, "Cancel", "Submit", [this, isLimited, submissionFee](FLAlertLayer*, bool btn2) {
        if (!btn2) return;

        Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Submitting banner...");
        popup->show();

        auto name = m_nameInput->getString();
        auto desc = m_descInput->getString();
        auto price = m_priceInput->getString();
        auto amount = m_amountInput->getString();
        auto filePath = m_selectedFilePath;

        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;

        if (accountId <= 0) {
            if (popup && popup->getParent()) {
                popup->showFailMessage("Invalid account ID.");
            }
            return;
        }

        Ref<CBSubmitBannerPopup> retainedSelf = this;
        arc::spawn([retainedSelf, accountId, accountData, name, desc, price, amount, isLimited, filePath, popup, submissionFee]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                geode::queueInMainThread([popup] {
                    if (popup && popup->getParent()) {
                        popup->showFailMessage("Authentication failed.");
                    }
                });
                co_return;
            }

            auto authToken = std::move(authResult);

            geode::utils::web::MultipartForm form;
            form.param("accountId", numToString(accountId));
            form.param("argonToken", authToken);
            form.param("name", name);
            form.param("description", desc);
            form.param("price", price);
            form.param("limited", isLimited ? "true" : "false");
            if (isLimited) {
                form.param("amount", amount);
            }

            std::string mimeType = "image/png";
            std::string ext = filePath.substr(filePath.find_last_of(".") + 1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == "webp") {
                mimeType = "image/webp";
            } else if (ext == "jpg" || ext == "jpeg") {
                mimeType = "image/jpeg";
            } else if (ext == "gif") {
                mimeType = "image/gif";
            }
            auto res = form.file("image", filePath, mimeType);
            if (!res) {
                geode::queueInMainThread([popup] {
                    if (popup && popup->getParent()) {
                        popup->showFailMessage("Failed to attach image file.");
                    }
                });
                co_return;
            }

            auto req = geode::utils::web::WebRequest();
            req.bodyMultipart(std::move(form));

            auto response = co_await req.post(fmt::format("{}/submitBanner", comment::baseUrl));
            if (!response.ok()) {
                geode::queueInMainThread([popup, response] {
                    if (popup && popup->getParent()) {
                        popup->showFailMessage(comment::getResponseMessage(response, "Failed to submit banner."));
                    }
                });
                co_return;
            }

            geode::queueInMainThread([popup, retainedSelf, submissionFee] {
                bool hasCreatedBanner = Mod::get()->getSavedValue<bool>("has_created_banner", false);
                if (!hasCreatedBanner) {
                    Mod::get()->setSavedValue("has_created_banner", true);
                }
                if (submissionFee > 0) {
                    int current = Mod::get()->getSavedValue<int>("amethyst", 0);
                    int newAmethyst = std::max(0, current - submissionFee);
                    Mod::get()->setSavedValue("amethyst", newAmethyst);
                    if (auto shopLayer = CBShopLayer::getInstance()) {
                        shopLayer->updateAmethystLabel(newAmethyst);
                    }
                }

                if (popup && popup->getParent()) {
                    popup->showSuccessMessage("Banner submitted successfully!");
                }
                if (retainedSelf && retainedSelf->getParent()) {
                    retainedSelf->onClose(nullptr);
                }
            });

            co_return;
        });
    });
}

void CBSubmitBannerPopup::onPreview(CCObject*) {
    if (m_selectedFilePath.empty()) return;

    auto blockLayer = CCBlockLayer::create();
    blockLayer->setZOrder(5);

    auto menu = CCMenu::create();
    menu->setZOrder(1);

    auto closeBtn = CCMenuItemSpriteExtra::create(
        CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png"),
        this,
        menu_selector(CBSubmitBannerPopup::onClosePreview));
    closeBtn->setUserObject(blockLayer);
    menu->addChildAtPosition(closeBtn, Anchor::TopLeft, {25.f, -25.f});
    blockLayer->addChild(menu);

    auto winSize = CCDirector::sharedDirector()->getWinSize();

    auto createPreviewNode = [](std::string const& filePath, CCSize const& size) -> CCNode* {
        auto clip = CCClippingNode::create();
        clip->setContentSize(size);
        clip->setAnchorPoint({0.5f, 0.5f});
        //clip->setAlphaThreshold(0.01f);

        auto stencil = CCLayerColor::create(ccColor4B{255, 255, 255, 255}, size.width, size.height);
        clip->setStencil(stencil);

        auto bg = NineSlice::create("square02_small.png");
        bg->setContentSize(size);
        bg->setPosition(size / 2.f);
        bg->setOpacity(100);
        clip->addChild(bg);

        auto image = comment::createBannerNode(filePath, size);
        image->setPosition(size / 2.f);
        if (auto lazy = static_cast<LazySprite*>(image)) {
            lazy->setAutoResize(false);
            auto applyScale = [lazy, size]() {
                if (!lazy->getTexture()) return;
                auto texSize = lazy->getTexture()->getContentSize();
                if (texSize.width <= 0 || texSize.height <= 0) return;
                float scale = std::max(size.width / texSize.width, size.height / texSize.height);
                lazy->setScale(scale);
            };
            if (lazy->isLoaded()) {
                applyScale();
            }
            lazy->setLoadCallback([applyScale](auto) {
                applyScale();
            });
        }
        clip->addChild(image);

        return clip;
    };

    auto container = CCNode::create();
    container->setContentSize({340.f, 160.f});
    container->setAnchorPoint({0.5f, 0.5f});
    container->setPosition(winSize / 2);
    container->setLayout(ColumnLayout::create()
            ->setGap(8.f)
            ->setAutoScale(false)
            ->setAxisAlignment(AxisAlignment::Center));

    auto nonCompactPreview = createPreviewNode(m_selectedFilePath, {340.f, 80.f});
    container->addChild(nonCompactPreview);

    auto nonCompactLabel = CCLabelBMFont::create("Non-Compact Mode", "goldFont.fnt");
    nonCompactLabel->setScale(0.45f);
    container->addChild(nonCompactLabel);

    auto compactPreview = createPreviewNode(m_selectedFilePath, {340.f, 36.f});
    container->addChild(compactPreview);

    auto compactLabel = CCLabelBMFont::create("Compact Mode", "goldFont.fnt");
    compactLabel->setScale(0.45f);
    container->addChild(compactLabel);

    container->updateLayout();
    blockLayer->addChild(container);

    this->addChild(blockLayer);
}

void CBSubmitBannerPopup::onClosePreview(CCObject* sender) {
    if (auto node = static_cast<CCNode*>(static_cast<CCNode*>(sender)->getUserObject())) {
        node->removeFromParent();
    }
}
