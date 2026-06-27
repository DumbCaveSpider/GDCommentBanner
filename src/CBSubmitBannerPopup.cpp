#include "CBSubmitBannerPopup.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <argon/argon.hpp>
#include "Geode/ui/General.hpp"
#include "include/CBConstant.hpp"
#include <fstream>
#include <vector>

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

    this->setTitle("Submit Banner");
    addSideArt(m_mainLayer, SideArt::Top, SideArtStyle::PopupBlue, false);
    addSideArt(m_mainLayer, SideArt::BottomRight, SideArtStyle::PopupBlue, false);

    // Pick File Button
    auto pickFileBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Select Image", "goldFont.fnt", "GJ_button_04.png", .8f),
        this,
        menu_selector(CBSubmitBannerPopup::onPickFile));
    m_buttonMenu->addChildAtPosition(pickFileBtn, Anchor::Top, {0.f, -50.f}, false);

    m_fileNameLabel = CCLabelBMFont::create("No file selected (1500x150)", "chatFont.fnt");
    m_fileNameLabel->limitLabelWidth(m_mainLayer->getContentWidth() - 20.f, 0.8f, 0.4f);
    m_fileNameLabel->setColor({200, 200, 200});
    m_mainLayer->addChildAtPosition(m_fileNameLabel, Anchor::Top, {0.f, -75.f}, false);

    // Name
    m_nameInput = geode::TextInput::create(300.f, "Banner Name");
    m_buttonMenu->addChildAtPosition(m_nameInput, Anchor::Center, {0.f, 30.f});

    // Description
    m_descInput = geode::TextInput::create(300.f, "Description");
    m_buttonMenu->addChildAtPosition(m_descInput, Anchor::Center, {0.f, -10.f});

    // Price & Limited row
    auto row1 = CCMenu::create();
    row1->setContentSize({300.f, 30.f});
    row1->setLayout(RowLayout::create()->setGap(10.f)->setAutoScale(false));

    m_priceInput = geode::TextInput::create(100.f, "Price");
    m_priceInput->setFilter("0123456789");
    row1->addChild(m_priceInput);

    m_amountInput = geode::TextInput::create(100.f, "Amount");
    m_amountInput->setCommonFilter(CommonFilter::Int);
    m_amountInput->setVisible(false);

    row1->addChild(m_amountInput);
    row1->updateLayout();

    m_buttonMenu->addChildAtPosition(row1, Anchor::Center, {0.f, -60.f}, {0.5, 0.5});

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
                        .description = "Images",
                        .files = {"*.png", "*.jpg", "*.jpeg", "*.gif"}}}});
        if (result.isOk()) {
            if (auto path = result.unwrap()) {
                retainedSelf->m_selectedFilePath = path->string();
                if (retainedSelf->m_fileNameLabel) {
                    retainedSelf->m_fileNameLabel->setString(path->filename().string().c_str());
                    retainedSelf->m_fileNameLabel->setColor({100, 255, 100});
                }
            }
        }
    });
}

void CBSubmitBannerPopup::onSubmit(CCObject*) {
    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Submitting banner...");
    popup->show();

    if (m_selectedFilePath.empty()) {
        popup->showFailMessage("Please select an image file.");
        return;
    }
    if (m_nameInput->getString().empty() || m_descInput->getString().empty() || m_priceInput->getString().empty()) {
        popup->showFailMessage("Please fill out all required fields.");
        return;
    }
    bool isLimited = m_limitedToggler->isToggled();
    if (isLimited && m_amountInput->getString().empty()) {
        popup->showFailMessage("Please specify an amount for the limited banner.");
        return;
    }

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

        // Read file
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            geode::queueInMainThread([popup] {
                popup->showFailMessage("Failed to read image file.");
            });
            co_return;
        }
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size)) {
            geode::queueInMainThread([popup] {
                popup->showFailMessage("Failed to read image file.");
            });
            co_return;
        }

        std::string boundary = "----GeodeBoundary123456789";
        std::string body;

        auto appendField = [&](const std::string& field, const std::string& val) {
            body += "--" + boundary + "\r\n";
            body += "Content-Disposition: form-data; name=\"" + field + "\"\r\n\r\n";
            body += val + "\r\n";
        };

        appendField("accountId", std::to_string(accountId));
        appendField("argonToken", authToken);
        appendField("name", name);
        appendField("description", desc);
        appendField("price", price);
        appendField("limited", isLimited ? "true" : "false");
        if (isLimited) {
            appendField("amount", amount);
        }

        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"image\"; filename=\"image.png\"\r\n";
        body += "Content-Type: application/octet-stream\r\n\r\n";
        body.append(buffer.begin(), buffer.end());
        body += "\r\n--" + boundary + "--\r\n";

        auto req = geode::utils::web::WebRequest();
        req.header("Content-Type", "multipart/form-data; boundary=" + boundary);

        auto response = co_await req.bodyString(body).post(fmt::format("{}/submitBanner", comment::baseUrl));
        if (!response.ok()) {
            geode::queueInMainThread([popup, response] {
                popup->showFailMessage(comment::getResponseMessage(response, "Failed to submit banner."));
            });
            co_return;
        }

        geode::queueInMainThread([popup, retainedSelf] {
            popup->showSuccessMessage("Banner submitted successfully! Waiting for staff approval.");
            retainedSelf->onClose(nullptr);
        });

        co_return;
    });
}
