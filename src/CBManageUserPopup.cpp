#include "CBManageUserPopup.hpp"
#include <argon/argon.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/binding/ProfilePage.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/LazySprite.hpp>
#include <Geode/ui/Button.hpp>
#include "Geode/cocos/label_nodes/CCLabelBMFont.h"
#include "Geode/utils/general.hpp"
#include "ccTypes.h"

using namespace geode::prelude;

CBManageUserPopup* CBManageUserPopup::create(matjson::Value const& userData) {
    auto ret = new CBManageUserPopup();
    if (ret && ret->init(userData)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBManageUserPopup::init(matjson::Value const& userData) {
    if (!Popup::init(520.f, 280.f, "GJ_square02.png")) return false;

    m_userData = userData;
    m_targetAccountId = m_userData["accountId"].asInt().unwrapOr(0);
    m_username = m_userData["username"].asString().unwrapOr("Unknown");
    m_isStaff = m_userData["is_staff"].asBool().unwrapOr(false);
    m_amethyst = m_userData["amethyst"].asInt().unwrapOr(0);

    this->setTitle(fmt::format("Manage {}", m_username));
    addSideArt(m_mainLayer, SideArt::TopLeft, SideArtStyle::PopupBlue);
    addSideArt(m_mainLayer, SideArt::BottomLeft, SideArtStyle::PopupBlue);

    // Left Side (Controls)
    auto controlsMenu = CCMenu::create();
    controlsMenu->setContentSize({180.f, 240.f});
    controlsMenu->setPosition({110.f, 140.f});
    controlsMenu->setLayout(ColumnLayout::create()->setGap(8.f)->setAxisAlignment(AxisAlignment::Center));

    // Staff Toggle
    auto staffRow = CCMenu::create();
    staffRow->setLayout(RowLayout::create()->setGap(10.f)->setAutoScale(false));

    m_staffToggler = CCMenuItemToggler::createWithStandardSprites(
        this, menu_selector(CBManageUserPopup::onToggleStaff), 1.f);
    m_staffToggler->toggle(m_isStaff);

    bool isAdmin = Mod::get()->getSavedValue<bool>("is_admin", false);
    if (!isAdmin) {
        staffRow->setVisible(false);
    }

    staffRow->addChild(m_staffToggler);
    controlsMenu->addChild(staffRow);

    auto staffLabel = CCLabelBMFont::create("Is Staff", "bigFont.fnt");
    staffLabel->limitLabelWidth(150.f, 0.6f, 0.4f);
    staffRow->addChild(staffLabel);

    staffRow->updateLayout();

    // Amethyst Input
    auto amyIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr);
    amyIcon->setScale(0.8f);

    auto amyRow = CCMenu::create();
    amyRow->setLayout(RowLayout::create()->setGap(5.f));
    amyRow->addChild(amyIcon);

    m_amethystInput = TextInput::create(80.f, "Amethyst");
    m_amethystInput->setFilter("0123456789");
    m_amethystInput->setString(std::to_string(m_amethyst));
    amyRow->addChild(m_amethystInput);

    auto amyBtn = geode::Button::createWithNode(
        ButtonSprite::create("Save", "goldFont.fnt", "GJ_button_01.png", 0.6f),
        [this](geode::Button* s) { this->onUpdateAmethyst(s); });
    amyRow->addChild(amyBtn);
    amyRow->updateLayout();
    controlsMenu->addChild(amyRow);
    // Ban Input
    auto banRow = CCMenu::create();
    banRow->setLayout(RowLayout::create()->setGap(5.f));

    m_banHoursInput = TextInput::create(135.f, "Temp Ban (Hrs)");
    m_banHoursInput->setFilter("-0123456789");  // Allow negative? Or 0 to unban.
    banRow->addChild(m_banHoursInput);

    auto banBtn = geode::Button::createWithNode(
        ButtonSprite::create("Ban", "goldFont.fnt", "GJ_button_06.png", 0.6f),
        [this](geode::Button* s) { this->onTempBan(s); });
    banRow->addChild(banBtn);
    banRow->updateLayout();
    controlsMenu->addChild(banRow);

    controlsMenu->updateLayout();
    m_mainLayer->addChild(controlsMenu);

    // Right Side (Banners)
    m_bannersList = cue::ListNode::create({290.f, 210.f}, {0, 0, 0, 0}, cue::ListBorderStyle::CommentsBlue);
    m_mainLayer->addChildAtPosition(m_bannersList, Anchor::Center, {100.f, -15.f}, false);

    fetchUserBanners();

    return true;
}

void CBManageUserPopup::onUpdateAmethyst(CCObject*) {
    auto amyRes = numFromString<int>(m_amethystInput->getString());
    if (amyRes.isErr()) {
        return;
    }
    int amy = amyRes.unwrap();

    auto accountData = argon::getGameAccountData();
    auto adminId = accountData.accountId;

    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Updating amethyst...");
    popup->show();

    Ref<CBManageUserPopup> retainedSelf = this;
    arc::spawn([retainedSelf, adminId, accountData, amy, popup]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            geode::queueInMainThread([popup] { popup->showFailMessage("Authentication failed."); });
            co_return;
        }

        auto req = geode::utils::web::WebRequest();
        req.header("Content-Type", "application/x-www-form-urlencoded");
        std::string body = fmt::format("accountId={}&argonToken={}&targetAccountId={}&amethyst={}",
            adminId,
            authResult,
            retainedSelf->m_targetAccountId,
            amy);

        auto res = co_await req.bodyString(body).post(fmt::format("{}/admin/updateUserAmethyst", comment::baseUrl));
        bool ok = res.ok();

        geode::queueInMainThread([popup, ok] {
            if (ok) {
                popup->showSuccessMessage("Amethyst updated!");
            } else {
                popup->showFailMessage("Failed to update amethyst.");
            }
        });
        co_return;
    });
}

void CBManageUserPopup::onToggleStaff(CCObject* sender) {
    auto toggler = static_cast<CCMenuItemToggler*>(sender);

    auto accountData = argon::getGameAccountData();
    auto adminId = accountData.accountId;

    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Toggling staff...");
    popup->show();

    Ref<CBManageUserPopup> retainedSelf = this;
    arc::spawn([retainedSelf, adminId, accountData, popup, toggler]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            geode::queueInMainThread([popup, toggler] {
                popup->showFailMessage("Authentication failed.");
                toggler->toggle(!toggler->isToggled());  // revert
            });
            co_return;
        }

        auto req = geode::utils::web::WebRequest();
        req.header("Content-Type", "application/x-www-form-urlencoded");
        std::string body = fmt::format("accountId={}&argonToken={}&targetAccountId={}",
            adminId,
            authResult,
            retainedSelf->m_targetAccountId);

        auto res = co_await req.bodyString(body).post(fmt::format("{}/admin/toggleStaff", comment::baseUrl));
        bool ok = res.ok();

        geode::queueInMainThread([retainedSelf, popup, ok, toggler] {
            if (ok) {
                popup->showSuccessMessage("Staff status toggled!");
                retainedSelf->m_isStaff = !retainedSelf->m_isStaff;
            } else {
                popup->showFailMessage("Failed to toggle staff status.");
                toggler->toggle(!toggler->isToggled());  // revert
            }
        });
        co_return;
    });
}

void CBManageUserPopup::onTempBan(CCObject*) {
    auto hoursRes = numFromString<int>(m_banHoursInput->getString());
    if (hoursRes.isErr()) {
        return;
    }
    int hours = hoursRes.unwrap();

    auto accountData = argon::getGameAccountData();
    auto adminId = accountData.accountId;

    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Banning user...");
    popup->show();

    Ref<CBManageUserPopup> retainedSelf = this;
    arc::spawn([retainedSelf, adminId, accountData, hours, popup]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            geode::queueInMainThread([popup] { popup->showFailMessage("Authentication failed."); });
            co_return;
        }

        auto req = geode::utils::web::WebRequest();
        req.header("Content-Type", "application/x-www-form-urlencoded");
        std::string body = fmt::format("accountId={}&argonToken={}&targetAccountId={}&hours={}",
            adminId,
            authResult,
            retainedSelf->m_targetAccountId,
            hours);

        auto res = co_await req.bodyString(body).post(fmt::format("{}/admin/tempBan", comment::baseUrl));
        bool ok = res.ok();

        geode::queueInMainThread([popup, ok] {
            if (ok) {
                popup->showSuccessMessage("User temp banned!");
            } else {
                popup->showFailMessage("Failed to ban user.");
            }
        });
        co_return;
    });
}

void CBManageUserPopup::fetchUserBanners() {
    if (m_loadingCircle) m_loadingCircle->removeFromParent();
    m_loadingCircle = cue::LoadingCircle::create(true);
    m_bannersList->addChildAtPosition(m_loadingCircle, Anchor::Center, CCPointZero, false);
    m_loadingCircle->fadeIn();

    auto accountData = argon::getGameAccountData();
    auto adminId = accountData.accountId;

    Ref<CBManageUserPopup> retainedSelf = this;
    arc::spawn([retainedSelf, adminId, accountData]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            geode::queueInMainThread([retainedSelf] { retainedSelf->m_loadingCircle->fadeOut(); });
            co_return;
        }

        auto req = geode::utils::web::WebRequest();
        auto url = fmt::format("{}/admin/userBanners?adminAccountId={}&argonToken={}&accountId={}",
            comment::baseUrl,
            adminId,
            authResult,
            retainedSelf->m_targetAccountId);

        auto res = co_await req.get(url);
        if (!res.ok()) {
            geode::queueInMainThread([retainedSelf] { retainedSelf->m_loadingCircle->fadeOut(); });
            co_return;
        }

        auto jsonRes = res.json();
        if (jsonRes.isErr()) {
            geode::queueInMainThread([retainedSelf] { retainedSelf->m_loadingCircle->fadeOut(); });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isArray()) {
            geode::queueInMainThread([retainedSelf] { retainedSelf->m_loadingCircle->fadeOut(); });
            co_return;
        }

        geode::queueInMainThread([retainedSelf, json = std::move(json)] {
            retainedSelf->m_loadingCircle->fadeOut();
            retainedSelf->m_bannersList->clear();

            if (json.size() == 0) {
                auto emptyLabel = CCLabelBMFont::create("No banners.", "goldFont.fnt");
                emptyLabel->setScale(0.6f);
                retainedSelf->m_bannersList->addChildAtPosition(emptyLabel, Anchor::Center);
                return;
            }

            for (size_t i = 0; i < json.size(); ++i) {
                retainedSelf->createBannerCell(json[i]);
            }
            retainedSelf->m_bannersList->updateLayout();
        });
        co_return;
    });
}

void CBManageUserPopup::createBannerCell(matjson::Value const& banner) {
    auto cell = CCLayer::create();
    cell->setContentSize({290.f, 40.f});

    std::string name = banner["name"].asString().unwrapOr("Unknown");
    std::string description = banner["description"].asString().unwrapOr("");
    std::string imageUrl = banner["imageUrl"].asString().unwrapOr("");
    if (!imageUrl.empty()) {
        imageUrl = fmt::format("{}", imageUrl);
    }

    auto sprite = LazySprite::create({345.f, 40.f}, true);
    sprite->setAutoResize(true);
    sprite->setPosition({145.f, 20.f});
    if (!imageUrl.empty()) {
        sprite->loadFromUrl(imageUrl, LazySprite::Format::kFmtWebp, false);
    }
    cell->addChild(sprite, -2);

    auto bg = CCLayerGradient::create({0, 0, 0, 255}, {0, 0, 0, 0}, {1, 0});
    bg->setContentSize(cell->getContentSize());
    bg->setAnchorPoint({0, 0});
    cell->addChild(bg, -1);

    auto label = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
    label->limitLabelWidth(140.f, 0.5f, 0.2f);
    label->setAnchorPoint({0, 0.5f});
    label->setPosition({10.f, 26.f});
    cell->addChild(label);

    auto descLabel = CCLabelBMFont::create(description.c_str(), "chatFont.fnt", 140.f);
    descLabel->setAnchorPoint({0, 0.5f});
    descLabel->setPosition({10.f, 13.f});
    descLabel->limitLabelWidth(140.f, 0.3f, 0.2f);
    cell->addChild(descLabel);

    int bannerId = banner["id"].asInt().unwrapOr(0);
    int price = banner["price"].asInt().unwrapOr(0);

    auto bannerMenu = CCMenu::create();
    bannerMenu->setPosition({0, 0});

    auto amyIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr);
    amyIcon->setScale(0.5f);
    amyIcon->setAnchorPoint({1.f, 0.5f});
    amyIcon->setPosition({168.f, 20.f});
    bannerMenu->addChild(amyIcon);

    auto priceInput = TextInput::create(40.f, "Price");
    priceInput->setCommonFilter(CommonFilter::Int);
    priceInput->setString(numToString(price));
    priceInput->setAnchorPoint({0, 0.5f});
    priceInput->setPosition({170.f, 20.f});
    bannerMenu->addChild(priceInput);

    auto savePriceSpr = CircleButtonSprite::createWithSpriteFrameName("GJ_completesIcon_001.png", 0.7f, CircleBaseColor::Green, CircleBaseSize::Small);
    savePriceSpr->setScale(0.7f);
    auto savePriceBtn = geode::Button::createWithNode(
        savePriceSpr,
        [this, bannerId, priceInput](geode::Button*) {
            this->updateBannerPrice(bannerId, priceInput);
        });
    savePriceBtn->setAnchorPoint({0.5f, 0.5f});
    savePriceBtn->setPosition({235.f, 20.f});
    bannerMenu->addChild(savePriceBtn);

    auto deleteBtn = geode::Button::createWithNode(
        CCSprite::createWithSpriteFrameName("GJ_resetBtn_001.png"),
        [this, bannerId](geode::Button*) {
            geode::createQuickPopup("Delete Banner", "Are you sure you want to <cr>delete this banner</c>?", "Cancel", "Delete", [this, bannerId](FLAlertLayer*, bool btn2) {
                if (btn2) {
                    this->deleteBanner(bannerId);
                }
            });
        });
    deleteBtn->setAnchorPoint({0.5f, 0.5f});
    deleteBtn->setPosition({270.f, 20.f});

    bool isAdmin = Mod::get()->getSavedValue<bool>("is_admin", false);
    if (!isAdmin) {
        deleteBtn->setEnabled(false);
        deleteBtn->setColor({150, 150, 150});
    }

    bannerMenu->addChild(deleteBtn);

    cell->addChild(bannerMenu);

    m_bannersList->setCellColor(ccColor4B{0, 0, 0, 0});
    m_bannersList->addCell(cell);
}

void CBManageUserPopup::deleteBanner(int bannerId) {
    auto popup = UploadActionPopup::create(nullptr, "Deleting Banner...");
    popup->show();

    Ref<CBManageUserPopup> retainedSelf = this;
    auto accountData = argon::getGameAccountData();
    arc::spawn([retainedSelf, bannerId, popup, accountData]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            geode::queueInMainThread([popup] { popup->showFailMessage("Authentication failed."); });
            co_return;
        }

        auto req = geode::utils::web::WebRequest();
        req.header("Content-Type", "application/x-www-form-urlencoded");
        std::string body = fmt::format("accountId={}&argonToken={}&bannerId={}",
            accountData.accountId,
            authResult,
            bannerId);

        auto res = co_await req.bodyString(body).post(fmt::format("{}/admin/deleteBanner", comment::baseUrl));
        bool ok = res.ok();

        geode::queueInMainThread([retainedSelf, popup, ok] {
            if (ok) {
                popup->showSuccessMessage("Banner deleted!");
                retainedSelf->fetchUserBanners();
            } else {
                popup->showFailMessage("Failed to delete banner.");
            }
        });
        co_return;
    });
}

void CBManageUserPopup::updateBannerPrice(int bannerId, geode::TextInput* input) {
    std::string priceStr = input->getString();
    if (priceStr.empty()) return;

    auto priceRes = numFromString<int>(priceStr);
    if (priceRes.isErr()) {
        return;
    }
    int price = priceRes.unwrap();

    auto popup = UploadActionPopup::create(nullptr, "Updating Price...");
    popup->show();

    Ref<CBManageUserPopup> retainedSelf = this;
    auto accountData = argon::getGameAccountData();
    arc::spawn([retainedSelf, bannerId, price, popup, accountData]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            geode::queueInMainThread([popup] { popup->showFailMessage("Authentication failed."); });
            co_return;
        }

        auto req = geode::utils::web::WebRequest();
        req.header("Content-Type", "application/x-www-form-urlencoded");
        std::string body = fmt::format("accountId={}&argonToken={}&bannerId={}&price={}",
            accountData.accountId,
            authResult,
            bannerId,
            price);

        auto res = co_await req.bodyString(body).post(fmt::format("{}/admin/updateBannerPrice", comment::baseUrl));
        bool ok = res.ok();

        geode::queueInMainThread([retainedSelf, popup, ok] {
            if (ok) {
                popup->showSuccessMessage("Price updated!");
            } else {
                popup->showFailMessage("Failed to update price.");
            }
        });
        co_return;
    });
}
