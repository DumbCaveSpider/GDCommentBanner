#include "CBSupporterPopup.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <dankmeme.argon/argon.hpp>
#include "Geode/ui/General.hpp"
#include <Geode/ui/MDPopup.hpp>
#include <Geode/ui/Button.hpp>
#include "include/CBConstant.hpp"

CBSupporterPopup* CBSupporterPopup::create() {
    auto ret = new CBSupporterPopup();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBSupporterPopup::init() {
    if (!Popup::init(320.f, 200.f, "GJ_square04.png")) return false;

    this->setTitle("Link Supporter Membership");
    addSideArt(m_mainLayer, SideArt::All, SideArtStyle::PopupBlue, false);

    auto infoBtn = Button::createWithSpriteFrameName("GJ_infoIcon_001.png", [](Button*) {
        MDPopup::create(
            "Supporter Benefits",
            "Supporting <cp>Comment Banners</c> on <cl>Ko-fi</c> helps pay for the servers, and gives you:\n\n"
            "- <cy>Supporter Only</c> banners: submit banners only supporters can buy, and filter the shop for them.\n"
            "- <cg>Change your banner's image</c> after it's approved (it's reviewed again before going live).\n"
            "- A separate <cy>non-compact</c> image (1500x350) for your banners, shown in non-compact comments.\n"
            "- <cr>Delete</c> your own banners (buyers are refunded).\n"
            "- Upload images up to <cg>5MB</c> instead of 500KiB, for higher quality banners.\n\n"
            "Supporter perks last while your <cl>Ko-fi</c> membership is active. "
            "Link it here with the <cy>email</c> or <cy>Discord username</c> you used on Ko-fi.",
            "OK")
            ->show();
    });
    infoBtn->setScale(0.8f);
    m_mainLayer->addChildAtPosition(infoBtn, Anchor::TopRight, {-20.f, -20.f}, false);
    infoBtn->setZOrder(2);

    auto infoLabel = Label::create("Enter the email or Discord username you used on Ko-fi.", "chatFont.fnt");
    infoLabel->setLimitLabelWidth(m_mainLayer->getContentWidth() - 30.f, 0.7f, 0.1f);
    m_mainLayer->addChildAtPosition(infoLabel, Anchor::Top, {0.f, -40.f}, false);

    // Email / Discord toggle
    auto tabMenu = CCMenu::create();
    tabMenu->setContentSize({260.f, 30.f});
    tabMenu->setLayout(RowLayout::create()->setGap(8.f));

    m_emailTabSprite = ButtonSprite::create("Email", "goldFont.fnt", "GJ_button_01.png", .75f);
    tabMenu->addChild(Button::createWithNode(m_emailTabSprite, [this](Button* sender) { this->onSelectEmail(sender); }));

    m_discordTabSprite = ButtonSprite::create("Discord", "goldFont.fnt", "GJ_button_04.png", .75f);
    tabMenu->addChild(Button::createWithNode(m_discordTabSprite, [this](Button* sender) { this->onSelectDiscord(sender); }));

    tabMenu->updateLayout();
    m_buttonMenu->addChildAtPosition(tabMenu, Anchor::Center, {0.f, 25.f}, {0.5f, 0.5f});

    // Inputs, only the selected one is visible
    m_emailInput = TextInput::create(260.f, "Ko-fi Email");
    m_emailInput->setCommonFilter(CommonFilter::Any);
    m_mainLayer->addChildAtPosition(m_emailInput, Anchor::Center, {0.f, -15.f}, false);

    m_discordInput = TextInput::create(260.f, "Discord Username");
    m_discordInput->setCommonFilter(CommonFilter::Any);
    m_mainLayer->addChildAtPosition(m_discordInput, Anchor::Center, {0.f, -15.f}, false);

    // Submit + Get Supporter buttons at the bottom
    auto bottomMenu = CCMenu::create();
    bottomMenu->setContentSize({280.f, 40.f});
    bottomMenu->setLayout(RowLayout::create()->setGap(10.f));

    auto submitBtn = Button::createWithNode(ButtonSprite::create("Submit", "goldFont.fnt", "GJ_button_01.png", .8f), [this](Button* sender) { this->onSubmit(sender); });
    bottomMenu->addChild(submitBtn);

    auto supporterBtn = Button::createWithNode(
        ButtonSprite::create("Get Supporter", "goldFont.fnt", "GJ_button_03.png", .8f),
        [](Button*) {
            utils::web::openLinkInBrowser("https://ko-fi.com/arcticwoof");
            Notification::create("Opened Ko-fi in your browser.", NotificationIcon::Info)->show();
        });
    bottomMenu->addChild(supporterBtn);

    bottomMenu->updateLayout();
    m_mainLayer->addChildAtPosition(bottomMenu, Anchor::Bottom, {0.f, 25.f}, {0.5f, 0.5f});

    // label to indicate if the user is already a supporter
    m_statusLabel = Label::create("", "bigFont.fnt");
    m_mainLayer->addChildAtPosition(m_statusLabel, Anchor::Bottom, {0.f, 55.f}, false);
    updateStatusLabel(comment::isUserSupporter());

    setUseDiscord(false);
    return true;
}

void CBSupporterPopup::updateStatusLabel(bool isSupporter) {
    m_statusLabel->setString(isSupporter ? "You are a Supporter!" : "You are not a Supporter yet");
    m_statusLabel->setColor(isSupporter ? ccGREEN : ccORANGE);
    m_statusLabel->setLimitLabelWidth(m_mainLayer->getContentWidth() - 40.f, 0.45f, 0.1f);
}

void CBSupporterPopup::onSelectEmail(CCObject*) {
    setUseDiscord(false);
}

void CBSupporterPopup::onSelectDiscord(CCObject*) {
    setUseDiscord(true);
}

void CBSupporterPopup::setUseDiscord(bool useDiscord) {
    m_useDiscord = useDiscord;
    m_emailTabSprite->updateBGImage(useDiscord ? "GJ_button_04.png" : "GJ_button_02.png");
    m_discordTabSprite->updateBGImage(useDiscord ? "GJ_button_02.png" : "GJ_button_04.png");
    m_emailInput->setVisible(!useDiscord);
    m_discordInput->setVisible(useDiscord);
    m_emailInput->defocus();
    m_discordInput->defocus();
}

void CBSupporterPopup::onSubmit(CCObject*) {
    auto value = std::string(utils::string::trim(m_useDiscord ? m_discordInput->getString() : m_emailInput->getString()));
    if (value.empty()) {
        Notification::create(m_useDiscord ? "Please enter your Discord username." : "Please enter your email.", NotificationIcon::Error)->show();
        return;
    }

    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;
    if (accountId <= 0) {
        Notification::create("Invalid account ID.", NotificationIcon::Error)->show();
        return;
    }

    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Linking membership...");
    popup->show();

    Ref<CBSupporterPopup> retainedSelf = this;
    std::string type = m_useDiscord ? "discord" : "email";

    arc::spawn([retainedSelf, accountId, accountData, type, value, popup]() -> arc::Future<> {
        auto authToken = co_await comment::argonToken(accountData);
        if (authToken.empty()) {
            queueInMainThread([popup] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage("Authentication failed.");
                }
            });
            co_return;
        }

        auto body = matjson::makeObject({{"accountId", accountId},
            {"argonToken", authToken},
            {"type", type},
            {"value", value}});

        auto req = web::WebRequest();
        auto response = co_await req.bodyJSON(body).post(fmt::format("{}/linkMembership", comment::baseUrl));
        if (!response.ok()) {
            std::string errorStr = comment::getResponseMessage(response, "Failed to link membership.");
            queueInMainThread([popup, errorStr] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage(errorStr);
                }
            });
            co_return;
        }

        bool isSupporter = false;
        std::string message = "Membership linked!";
        if (auto json = response.json(); json.isOk()) {
            auto data = json.unwrap();
            isSupporter = data["is_supporter"].asBool().unwrapOr(false);
            message = data["message"].asString().unwrapOr(message);
        }

        queueInMainThread([retainedSelf, popup, isSupporter, message] {
            Mod::get()->setSavedValue("is_supporter", isSupporter);
            retainedSelf->updateStatusLabel(isSupporter);
            if (popup && popup->getParent()) {
                if (isSupporter) {
                    popup->showSuccessMessage(message);
                } else {
                    popup->showFailMessage(message);
                }
            }
            if (isSupporter && retainedSelf->getParent()) {
                retainedSelf->onClose(nullptr);
            }
        });
        co_return;
    });
}
