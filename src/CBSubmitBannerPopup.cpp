#include "CBSubmitBannerPopup.hpp"
#include "CBShopLayer.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <argon/argon.hpp>
#include "Geode/ui/General.hpp"
#include "Geode/utils/general.hpp"
#include "include/CBConstant.hpp"
#include <Geode/utils/file.hpp>
#include <algorithm>

using namespace geode::prelude;

CBSubmitBannerPopup* CBSubmitBannerPopup::create() {
    auto ret = new CBSubmitBannerPopup();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBSubmitBannerPopup::init() {
    if (!Popup::init(380.f, 260.f)) return false;

    this->setTitle("Submit Comment Banner");
    addSideArt(m_mainLayer, SideArt::Top, SideArtStyle::PopupBlue, false);
    addSideArt(m_mainLayer, SideArt::BottomRight, SideArtStyle::PopupBlue, false);

    // Pick File Button
    auto pickFileBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Select Image", "goldFont.fnt", "GJ_button_04.png", .8f),
        this,
        menu_selector(CBSubmitBannerPopup::onPickFile));
    m_buttonMenu->addChildAtPosition(pickFileBtn, Anchor::Top, {0.f, -50.f}, false);

    m_fileNameLabel = CCLabelBMFont::create("No file selected (1500x150) (Static or Animated)", "chatFont.fnt");
    m_fileNameLabel->limitLabelWidth(m_mainLayer->getContentWidth() - 20.f, 0.8f, 0.4f);
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
        if (auto row = m_amountInput->getParent()) {
            static_cast<CCMenu*>(row)->updateLayout();
        }
    }
}

void CBSubmitBannerPopup::onPickFile(CCObject*) {
    Ref<CBSubmitBannerPopup> retainedSelf = this;
    arc::spawn([retainedSelf]() -> arc::Future<> {
        auto result = co_await geode::utils::file::pick(
            geode::utils::file::PickMode::OpenFile,
            geode::utils::file::FilePickOptions{
                .defaultPath = std::nullopt,
                .filters = {
                    geode::utils::file::FilePickOptions::Filter{
                        .description = "Image Files",
                        .files = {"*.png", "*.webp", "*.jpg", "*.jpeg", "*.gif"}}}});

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
                if (extension != ".png" && extension != ".webp" && extension != ".jpg" && extension != ".jpeg" && extension != ".gif") {
                    notify("Please select a PNG, JPEG, GIF or WEBP file");
                    co_return;
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
                });
            }
        }
    });
}

void CBSubmitBannerPopup::onSubmit(CCObject*) {
    int currentAmethysts = Mod::get()->getSavedValue<int>("amethyst", 0);
    bool hasCreatedBanner = Mod::get()->getSavedValue<bool>("has_created_banner", false);
    if (currentAmethysts < 15000 && hasCreatedBanner) {
        int needed = 15000 - currentAmethysts;
        geode::Notification::create(fmt::format("Not enough amethysts! You need {} more.", needed), geode::NotificationIcon::Error)->show();
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

    geode::createQuickPopup("Submit Banner", "Are you sure you want to submit this banner? You <cr>cannot change it</c> after it's uploaded.", "Cancel", "Submit", [this, isLimited](FLAlertLayer*, bool btn2) {
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
            popup->showFailMessage("Invalid account ID.");
            return;
        }

        Ref<CBSubmitBannerPopup> retainedSelf = this;
        arc::spawn([retainedSelf, accountId, accountData, name, desc, price, amount, isLimited, filePath, popup]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                geode::queueInMainThread([popup] {
                    popup->showFailMessage("Authentication failed.");
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
                    popup->showFailMessage("Failed to attach image file.");
                });
                co_return;
            }

            auto req = geode::utils::web::WebRequest();
            req.bodyMultipart(std::move(form));

            auto response = co_await req.post(fmt::format("{}/submitBanner", comment::baseUrl));
            if (!response.ok()) {
                geode::queueInMainThread([popup, response] {
                    popup->showFailMessage(comment::getResponseMessage(response, "Failed to submit banner."));
                });
                co_return;
            }

            geode::queueInMainThread([popup, retainedSelf] {
                bool hasCreatedBanner = Mod::get()->getSavedValue<bool>("has_created_banner", false);
                if (!hasCreatedBanner) {
                    Mod::get()->setSavedValue("has_created_banner", true);
                } else {
                    int current = Mod::get()->getSavedValue<int>("amethyst", 0);
                    int newAmethyst = std::max(0, current - 15000);
                    Mod::get()->setSavedValue("amethyst", newAmethyst);
                    if (auto shopLayer = CBShopLayer::getInstance()) {
                        shopLayer->updateAmethystLabel(newAmethyst);
                    }
                }

                popup->showSuccessMessage("Banner submitted successfully!");
                retainedSelf->onClose(nullptr);
            });

            co_return;
        });
    });
}
