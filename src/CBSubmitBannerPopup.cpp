#include "CBSubmitBannerPopup.hpp"
#include "CBPreviewBanner.hpp"
#include "CBShopLayer.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <dankmeme.argon/argon.hpp>
#include "Geode/ui/General.hpp"
#include "Geode/utils/general.hpp"
#include "ccTypes.h"
#include "include/CBConstant.hpp"
#include "include/CBLocalBanner.hpp"
#include "CBLocalBannersPopup.hpp"
#include <Geode/utils/file.hpp>
#include <Geode/ui/NineSlice.hpp>
#include <algorithm>
#include <filesystem>
#include <unordered_set>

using namespace geode::prelude;

CBSubmitBannerPopup* CBSubmitBannerPopup::create(
    bool isLocal,
    bool isEdit,
    CBEditBannerData const& editData,
    std::function<void()> onEditSuccess) {
    auto ret = new CBSubmitBannerPopup();
    if (ret && ret->init(isLocal, isEdit, editData, std::move(onEditSuccess))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBSubmitBannerPopup::init(bool isLocal, bool isEdit, CBEditBannerData const& editData, std::function<void()> onEditSuccess) {
    if (!Popup::init(400.f, 280.f)) return false;

    m_isLocal = isLocal;
    m_isEdit = isEdit && !isLocal;
    m_editData = editData;
    m_onEditSuccess = std::move(onEditSuccess);

    if (m_isEdit) {
        this->setTitle("Change Banner Image");
    } else {
        this->setTitle(m_isLocal ? "Add Local Banner" : "Submit Comment Banner");
    }

    addSideArt(m_mainLayer, SideArt::Top, SideArtStyle::PopupBlue, false);

    // supporters can also upload a separate image for non-compact comment cells
    m_canUploadNonCompact = !m_isLocal && comment::isUserSupporter();

    // Pick File Button(s)
    auto pickFileBtn = Button::createWithNode(ButtonSprite::create(m_canUploadNonCompact ? "Submit Compact" : "Select Image", "goldFont.fnt", "GJ_button_04.png", .8f), [this](Button* sender) { this->onPickFile(sender); });
    pickFileBtn->setTag(0);

    Button* pickNonCompactBtn = nullptr;
    if (m_canUploadNonCompact) {
        pickNonCompactBtn = Button::createWithNode(ButtonSprite::create("Submit Non-Compact", "goldFont.fnt", "GJ_button_04.png", .8f), [this](Button* sender) { this->onPickFile(sender); });
        pickNonCompactBtn->setTag(1);
    }

    // Preview, top right corner of the popup (shown once there's an image to preview)
    m_previewBtn = Button::createWithSpriteFrameName("dtrackBtn_search_001.png", [this](Button* sender) { this->onPreview(sender); });
    m_previewBtn->setVisible(false);
    m_buttonMenu->addChildAtPosition(m_previewBtn, Anchor::TopRight, {-25.f, -25.f});

    auto topMenu = CCMenu::create();
    topMenu->setContentSize({300.f, 40.f});
    topMenu->setLayout(RowLayout::create()->setGap(5.f));
    topMenu->addChild(pickFileBtn);
    if (pickNonCompactBtn) topMenu->addChild(pickNonCompactBtn);
    topMenu->updateLayout();

    m_buttonMenu->addChildAtPosition(topMenu, Anchor::Top, {0.f, -50.f}, {0.5, 0.5});

    m_fileNameLabel = Label::create("", "chatFont.fnt");
    m_mainLayer->addChildAtPosition(m_fileNameLabel, Anchor::Top, {0.f, -75.f}, false);
    this->updateFileLabel();

    // Name
    m_nameInput = TextInput::create(300.f, "Banner Name");
    m_nameInput->setLabel("Banner Name");
    m_nameInput->setCommonFilter(CommonFilter::Name);
    m_buttonMenu->addChildAtPosition(m_nameInput, Anchor::Center, {0.f, 30.f});

    // Description
    m_descInput = TextInput::create(300.f, "Description");
    m_descInput->setLabel("Description");
    m_descInput->setCommonFilter(CommonFilter::Any);
    m_buttonMenu->addChildAtPosition(m_descInput, Anchor::Center, {0.f, -15.f});

    // Price & Limited row
    auto row1 = CCMenu::create();
    row1->setContentSize({300.f, 30.f});
    row1->setLayout(RowLayout::create()->setGap(10.f)->setAutoScale(false));

    m_priceInput = TextInput::create(100.f, "Price");
    m_priceInput->setLabel("Price");
    m_priceInput->setCommonFilter(CommonFilter::Int);
    row1->addChild(m_priceInput);

    m_amountInput = TextInput::create(100.f, "Amount");
    m_amountInput->setLabel("Amount");
    m_amountInput->setCommonFilter(CommonFilter::Int);
    m_amountInput->setVisible(false);

    row1->addChild(m_amountInput);
    row1->updateLayout();

    m_buttonMenu->addChildAtPosition(row1, Anchor::Center, {0.f, -60.f}, {0.5, 0.5});

    auto costRow = CCMenu::create();
    costRow->setContentSize({300.f, 20.f});
    costRow->setLayout(RowLayout::create()->setGap(5.f)->setAutoScale(false)->setAxisAlignment(AxisAlignment::End));

    auto amethystSpr = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr);
    amethystSpr->setScale(0.6f);
    costRow->addChild(amethystSpr);

    bool hasCreatedBanner = Mod::get()->getSavedValue<bool>("has_created_banner", false);

    auto costLabel = Label::create(
        hasCreatedBanner ? "Cost: 15,000" : "Cost: Free",
        "bigFont.fnt");
    costLabel->setScale(0.35f);
    if (!hasCreatedBanner) {
        costLabel->setColor({100, 255, 100});  // Make it green if free
    }
    costRow->addChild(costLabel);

    costRow->updateLayout();
    m_mainLayer->addChildAtPosition(costRow, Anchor::BottomRight, {-10.f, 10.f}, {1.0, 0.0});

    // Supporter Only + Limited toggles, stacked in the bottom left corner
    auto togglesMenu = CCMenu::create();
    togglesMenu->setAnchorPoint({0.f, 0.f});
    togglesMenu->setContentSize({100.f, 0.f});
    togglesMenu->setLayout(ColumnLayout::create()
            ->setGap(0.f)
            ->setAxisReverse(true)  // first row on top
            ->setAutoScale(false)
            ->setAutoGrowAxis(0.f)
            ->setCrossAxisAlignment(AxisAlignment::Start)
            ->setCrossAxisLineAlignment(AxisAlignment::Start));

    // each row is its own menu so the toggler still receives touches
    auto createToggleRow = [](Toggler* toggler, char const* text, ccColor3B color) {
        auto row = CCMenu::create();
        row->setAnchorPoint({0.f, 0.5f});
        row->setContentSize({100.f, 30.f});
        row->setLayout(RowLayout::create()->setGap(5.f)->setAutoScale(false)->setAxisAlignment(AxisAlignment::Start));

        auto label = Label::create(text, "bigFont.fnt");
        label->setLimitLabelWidth(row->getContentWidth() - 30.f, 0.4f, 0.2f);
        label->setColor(color);

        row->addChild(toggler);
        row->addChild(label);
        row->updateLayout();
        return row;
    };

    // Supporter Only, above Limited (supporters only)
    if (comment::isUserSupporter() && !m_isLocal) {
        m_supporterOnlyToggler = Toggler::createWithStandardSprites(0.7f);
        togglesMenu->addChild(createToggleRow(m_supporterOnlyToggler, "Supporter", {255, 150, 255}));
    }

    m_limitedToggler = Toggler::createWithStandardSprites(0.7f, [this](Toggler* sender, bool) { this->onToggleLimited(sender); });
    togglesMenu->addChild(createToggleRow(m_limitedToggler, "Limited", {255, 255, 255}));

    togglesMenu->updateLayout();
    m_mainLayer->addChildAtPosition(togglesMenu, Anchor::BottomLeft, {10.f, 10.f}, false);

    if (m_isLocal) {
        m_descInput->setVisible(false);
        row1->setVisible(false);
        costRow->setVisible(false);
        togglesMenu->setVisible(false);
        m_nameInput->setPositionY(m_nameInput->getPositionY() - 30);
    }

    if (m_isEdit) {
        // price and submission cost don't apply when changing the image
        m_nameInput->setString(m_editData.name);
        m_descInput->setString(m_editData.description);
        costRow->setVisible(false);
        m_priceInput->removeFromParent();
        m_priceInput = nullptr;

        m_limitedToggler->setToggled(m_editData.isLimited);
        m_amountInput->setVisible(m_editData.isLimited);
        if (m_editData.isLimited) {
            m_amountInput->setString(numToString(m_editData.amount));
        }
        row1->updateLayout();

        if (m_supporterOnlyToggler) {
            m_supporterOnlyToggler->setToggled(m_editData.isSupporterOnly);
        }

        // the current images can be previewed before picking new ones
        m_previewBtn->setVisible(true);
    }

    // Submit Button
    auto submitBtn = Button::createWithNode(ButtonSprite::create(m_isEdit ? "Update" : "Submit", "goldFont.fnt", "GJ_button_01.png", .8f), [this](Button* sender) { this->onSubmit(sender); });
    m_buttonMenu->addChildAtPosition(submitBtn, Anchor::Bottom, {0.f, 25.f}, false);

    return true;
}

void CBSubmitBannerPopup::onToggleLimited(CCObject* sender) {
    // called after the toggle, so this is already the new state
    bool toggled = static_cast<Toggler*>(sender)->isToggled();
    if (m_amountInput) {
        m_amountInput->setVisible(toggled);
        if (auto parent = m_amountInput->getParent()) {
            parent->updateLayout();
        }
    }
}

void CBSubmitBannerPopup::updateFileLabel() {
    if (!m_fileNameLabel) return;

    auto fileName = [](std::string const& path) {
        return std::filesystem::path(path).filename().string();
    };

    std::string text;
    bool hasAny = !m_selectedFilePath.empty() || !m_selectedNonCompactPath.empty();
    if (!m_canUploadNonCompact) {
        text = m_selectedFilePath.empty() ? "No file selected (1500x150) (Static or Animated)" : fileName(m_selectedFilePath);
    } else {
        std::string compact = m_selectedFilePath.empty()
                                  ? (m_isEdit ? "unchanged" : "none (1500x150)")
                                  : fileName(m_selectedFilePath);
        std::string nonCompact = m_selectedNonCompactPath.empty()
                                     ? (m_isEdit ? "unchanged" : "optional (1500x350)")
                                     : fileName(m_selectedNonCompactPath);
        text = fmt::format("Compact: {}  |  Non-Compact: {}", compact, nonCompact);
    }

    m_fileNameLabel->setString(text.c_str());
    m_fileNameLabel->setLimitLabelWidth(m_mainLayer->getContentWidth() - 20.f, 0.8f, 0.1f);
    m_fileNameLabel->setColor(hasAny ? ccGREEN : ccWHITE);
}

void CBSubmitBannerPopup::onPickFile(CCObject* sender) {
    Ref<CBSubmitBannerPopup> retainedSelf = this;
    bool nonCompact = sender && sender->getTag() == 1 && m_canUploadNonCompact;
    int requiredHeight = nonCompact ? 350 : 150;
    arc::spawn([retainedSelf, nonCompact, requiredHeight]() -> arc::Future<> {
        std::unordered_set<std::string> allowedFiles = retainedSelf->m_isLocal
                                                           ? std::unordered_set<std::string>{"*.png", "*.webp", "*.gif"}
                                                           : std::unordered_set<std::string>{"*.png", "*.webp", "*.jpg", "*.jpeg", "*.gif"};

        auto result = co_await utils::file::pick(
            utils::file::PickMode::OpenFile,
            utils::file::FilePickOptions{
                .defaultPath = std::nullopt,
                .filters = {
                    utils::file::FilePickOptions::Filter{
                        .description = "Image Files",
                        .files = allowedFiles}}});

        auto notify = [&](std::string message) {
            Loader::get()->queueInMainThread([message = std::move(message)]() {
                Notification::create(message.c_str(), NotificationIcon::Warning)->show();
            });
        };

        if (result.isOk()) {
            auto pathOpt = result.unwrap();
            if (pathOpt.has_value()) {
                auto path = pathOpt.value();

                std::string extension(path.extension().value_or(""));
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                if (retainedSelf->m_isLocal) {
                    if (extension != "png" && extension != "webp" && extension != "gif") {
                        notify("Please select a PNG, GIF or WEBP file");
                        co_return;
                    }
                } else {
                    if (extension != "png" && extension != "webp" && extension != "jpg" && extension != "jpeg" && extension != "gif") {
                        notify("Please select a PNG, JPEG, GIF or WEBP file");
                        co_return;
                    }
                }

                if (extension != "gif") {
                    CCImage image;
                    if (!image.initWithImageFile(path.string().c_str())) {
                        notify("Unable to decode image locally");
                        co_return;
                    }

                    if (image.getWidth() != 1500 || image.getHeight() != requiredHeight) {
                        notify(fmt::format("{} image must be 1500x{}, got {}x{}",
                            nonCompact ? "Non-compact" : "Compact",
                            requiredHeight,
                            image.getWidth(),
                            image.getHeight()));
                        co_return;
                    }
                }

                Loader::get()->queueInMainThread([retainedSelf, nonCompact, p = path.string()]() {
                    if (nonCompact) {
                        retainedSelf->m_selectedNonCompactPath = p;
                    } else {
                        retainedSelf->m_selectedFilePath = p;
                    }
                    retainedSelf->updateFileLabel();
                    if (retainedSelf->m_previewBtn) {
                        retainedSelf->m_previewBtn->setVisible(true);
                    }
                });
            }
        }
    });
}

void CBSubmitBannerPopup::onSubmit(CCObject*) {
    if (m_isLocal) {
        if (m_selectedFilePath.empty()) {
            Notification::create("Please select an image file.", NotificationIcon::Error)->show();
            return;
        }
        if (m_nameInput->getString().empty()) {
            Notification::create("Please enter a banner name.", NotificationIcon::Error)->show();
            return;
        }

        if (comment::local::addLocalBanner(m_nameInput->getString(), m_selectedFilePath)) {
            Notification::create("Local banner added successfully!", NotificationIcon::Success)->show();
            if (auto popup = CBLocalBannersPopup::getInstance()) {
                popup->fetchBanners();
            } else if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
                if (auto popup = scene->getChildByType<CBLocalBannersPopup>(0)) {
                    popup->fetchBanners();
                }
            }
            this->onClose(nullptr);
        } else {
            Notification::create("Failed to save local banner.", NotificationIcon::Error)->show();
        }
        return;
    }

    // a new banner always needs the compact image, an edit needs at least one new image
    if (m_isEdit ? (m_selectedFilePath.empty() && m_selectedNonCompactPath.empty()) : m_selectedFilePath.empty()) {
        Notification::create("Please select an image file.", NotificationIcon::Error)->show();
        return;
    }

    if (m_isEdit) {
        if (m_nameInput->getString().empty() || m_descInput->getString().empty()) {
            Notification::create("Please fill out all required fields.", NotificationIcon::Error)->show();
            return;
        }
        if (m_limitedToggler->isToggled()) {
            int amount = numFromString<int>(m_amountInput->getString()).unwrapOr(0);
            if (amount <= 0) {
                Notification::create("Please specify an amount for the limited banner.", NotificationIcon::Error)->show();
                return;
            }
            if (amount < m_editData.totalBought) {
                Notification::create("Amount cannot be less than total bought.", NotificationIcon::Error)->show();
                return;
            }
        }
        Ref<CBSubmitBannerPopup> retainedSelf = this;
        createQuickPopup("Change Banner Image", "Are you sure you want to <cy>replace the image</c> of this banner?\nThe banner goes back to <co>pending</c> until staff <cg>review</c> the new image, and <cr>everyone who has it equipped will have it unequipped</c>. If the new image is rejected, the current one is restored.", "Cancel", "Update", [retainedSelf](FLAlertLayer*, bool btn2) {
            if (btn2) retainedSelf->submitEdit();
        });
        return;
    }

    if (m_nameInput->getString().empty() || m_descInput->getString().empty() || m_priceInput->getString().empty()) {
        Notification::create("Please fill out all required fields.", NotificationIcon::Error)->show();
        return;
    }
    bool isLimited = m_limitedToggler->isToggled();
    if (isLimited && m_amountInput->getString().empty()) {
        Notification::create("Please specify an amount for the limited banner.", NotificationIcon::Error)->show();
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
        Notification::create(fmt::format("Not enough amethysts! You need {} more.", needed), NotificationIcon::Error)->show();
        return;
    }

    std::string popupTitle = "Submit Banner";
    std::string popupMsg = "Are you sure you want to submit this banner? It will be <co>pending</c> until staff <cg>review</c> it. Changing its image later sends it back for review.";
    if (priceVal > 100000) {
        popupTitle = "Increased Submission Fee";
        popupMsg = fmt::format("Because this banner's price is higher than <cp>100,000 amethysts</c>, there is an increased submission fee of <cy>{} amethysts</c>.\nAre you sure you want to proceed? Changing its image later sends it back for review.", GameToolbox::pointsToString(submissionFee));
    }

    createQuickPopup(popupTitle.c_str(), popupMsg, "Cancel", "Submit", [this, isLimited, submissionFee](FLAlertLayer*, bool btn2) {
        if (!btn2) return;

        Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Submitting banner...");
        popup->show();

        auto name = m_nameInput->getString();
        auto desc = m_descInput->getString();
        auto price = m_priceInput->getString();
        auto amount = m_amountInput->getString();
        auto filePath = m_selectedFilePath;
        auto nonCompactPath = m_selectedNonCompactPath;
        bool isSupporterOnly = m_supporterOnlyToggler && m_supporterOnlyToggler->isToggled();

        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;

        if (accountId <= 0) {
            if (popup && popup->getParent()) {
                popup->showFailMessage("Invalid account ID.");
            }
            return;
        }

        Ref<CBSubmitBannerPopup> retainedSelf = this;
        arc::spawn([retainedSelf, accountId, accountData, name, desc, price, amount, isLimited, isSupporterOnly, filePath, nonCompactPath, popup, submissionFee]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                queueInMainThread([popup] {
                    if (popup && popup->getParent()) {
                        popup->showFailMessage("Authentication failed.");
                    }
                });
                co_return;
            }

            auto authToken = std::move(authResult);

            utils::web::MultipartForm form;
            form.param("accountId", numToString(accountId));
            form.param("argonToken", authToken);
            form.param("name", name);
            form.param("description", desc);
            form.param("price", price);
            form.param("limited", isLimited ? "true" : "false");
            if (isLimited) {
                form.param("amount", amount);
            }
            form.param("supporterOnly", isSupporterOnly ? "true" : "false");

            bool attached = static_cast<bool>(form.file("image", filePath, comment::imageMimeType(filePath)));
            if (attached && !nonCompactPath.empty()) {
                attached = static_cast<bool>(form.file("nonCompactImage", nonCompactPath, comment::imageMimeType(nonCompactPath)));
            }
            if (!attached) {
                queueInMainThread([popup] {
                    if (popup && popup->getParent()) {
                        popup->showFailMessage("Failed to attach image file.");
                    }
                });
                co_return;
            }

            auto req = utils::web::WebRequest();
            req.bodyMultipart(std::move(form));

            auto response = co_await req.post(fmt::format("{}/submitBanner", comment::baseUrl));
            if (!response.ok()) {
                queueInMainThread([popup, response] {
                    if (popup && popup->getParent()) {
                        popup->showFailMessage(comment::getResponseMessage(response, "Failed to submit banner."));
                    }
                });
                co_return;
            }

            queueInMainThread([popup, retainedSelf, submissionFee] {
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

void CBSubmitBannerPopup::submitEdit() {
    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;
    if (accountId <= 0) {
        Notification::create("Invalid account ID.", NotificationIcon::Error)->show();
        return;
    }

    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Updating banner image...");
    popup->show();

    auto name = m_nameInput->getString();
    auto desc = m_descInput->getString();
    auto filePath = m_selectedFilePath;
    auto nonCompactPath = m_selectedNonCompactPath;
    int bannerId = m_editData.id;
    bool isLimited = m_limitedToggler->isToggled();
    auto amount = m_amountInput->getString();
    std::optional<bool> isSupporterOnly;
    if (m_supporterOnlyToggler) isSupporterOnly = m_supporterOnlyToggler->isToggled();

    Ref<CBSubmitBannerPopup> retainedSelf = this;
    arc::spawn([retainedSelf, accountId, accountData, bannerId, name, desc, isLimited, amount, isSupporterOnly, filePath, nonCompactPath, popup]() -> arc::Future<> {
        auto authToken = co_await comment::argonToken(accountData);
        if (authToken.empty()) {
            queueInMainThread([popup] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage("Authentication failed.");
                }
            });
            co_return;
        }

        utils::web::MultipartForm form;
        form.param("accountId", numToString(accountId));
        form.param("argonToken", authToken);
        form.param("bannerId", numToString(bannerId));
        form.param("name", name);
        form.param("description", desc);
        form.param("limited", isLimited ? "true" : "false");
        if (isLimited) {
            form.param("amount", amount);
        }
        if (isSupporterOnly) {
            form.param("supporterOnly", *isSupporterOnly ? "true" : "false");
        }
        bool attached = true;
        if (!filePath.empty()) {
            attached = static_cast<bool>(form.file("image", filePath, comment::imageMimeType(filePath)));
        }
        if (attached && !nonCompactPath.empty()) {
            attached = static_cast<bool>(form.file("nonCompactImage", nonCompactPath, comment::imageMimeType(nonCompactPath)));
        }
        if (!attached) {
            queueInMainThread([popup] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage("Failed to attach image file.");
                }
            });
            co_return;
        }

        auto req = utils::web::WebRequest();
        req.bodyMultipart(std::move(form));
        auto response = co_await req.post(fmt::format("{}/editBanner", comment::baseUrl));
        if (!response.ok()) {
            queueInMainThread([popup, response] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage(comment::getResponseMessage(response, "Failed to update banner image."));
                }
            });
            co_return;
        }

        // e.g. "the new image was sent for review"
        std::string message = "Banner image updated!";
        if (auto json = response.json(); json.isOk()) {
            message = json.unwrap()["message"].asString().unwrapOr(message);
        }

        queueInMainThread([popup, retainedSelf, bannerId, message] {
            // the server unequipped this banner from everyone, including us
            comment::invalidateBannerCache();
            if (auto shop = CBShopLayer::getInstance()) {
                if (shop->getEquippedBannerId() == bannerId) {
                    shop->setEquippedBannerId(-1);
                }
                shop->refreshBanners();
            }
            if (popup && popup->getParent()) {
                popup->showSuccessMessage(message);
            }
            if (retainedSelf->m_onEditSuccess) retainedSelf->m_onEditSuccess();
            if (retainedSelf->getParent()) {
                retainedSelf->onClose(nullptr);
            }
        });
        co_return;
    });
}

void CBSubmitBannerPopup::onPreview(CCObject*) {
    // when editing, images that weren't replaced fall back to the banner's current ones
    std::string compactImage = m_selectedFilePath;
    std::string nonCompactImage = m_selectedNonCompactPath;
    if (m_isEdit) {
        if (compactImage.empty()) compactImage = m_editData.imageUrl;
        if (nonCompactImage.empty()) nonCompactImage = m_editData.nonCompactImageUrl;
    }

    if (auto preview = CBPreviewBanner::create(compactImage, nonCompactImage)) {
        preview->show();
    }
}
