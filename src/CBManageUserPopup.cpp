#include "CBManageUserPopup.hpp"
#include "CBBannerCell.hpp"
#include <dankmeme.argon/argon.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/binding/ProfilePage.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/Label.hpp>
#include "Geode/utils/general.hpp"
#include "ccTypes.h"
#include <Geode/ui/Scrollbar.hpp>

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

    m_staffToggler = Toggler::createWithStandardSprites(1.f, [this](Toggler* sender, bool) { this->onToggleStaff(sender); });
    m_staffToggler->setToggled(m_isStaff);

    bool isAdmin = Mod::get()->getSavedValue<bool>("is_admin", false);
    if (!isAdmin) {
        staffRow->setVisible(false);
    }

    staffRow->addChild(m_staffToggler);
    controlsMenu->addChild(staffRow);

    auto staffLabel = Label::create("Is Staff", "bigFont.fnt");
    staffLabel->setLimitLabelWidth(150.f, 0.6f, 0.4f);
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

    auto amyBtn = Button::createWithNode(
        ButtonSprite::create("Save", "goldFont.fnt", "GJ_button_01.png", 0.6f),
        [this](Button* s) { this->onUpdateAmethyst(s); });
    amyRow->addChild(amyBtn);
    amyRow->updateLayout();
    controlsMenu->addChild(amyRow);
    // Ban Input
    auto banRow = CCMenu::create();
    banRow->setLayout(RowLayout::create()->setGap(5.f));

    m_banHoursInput = TextInput::create(135.f, "Temp Ban (Hrs)");
    m_banHoursInput->setFilter("-0123456789");  // Allow negative? Or 0 to unban.
    banRow->addChild(m_banHoursInput);

    auto banBtn = Button::createWithNode(
        ButtonSprite::create("Ban", "goldFont.fnt", "GJ_button_06.png", 0.6f),
        [this](Button* s) { this->onTempBan(s); });
    banRow->addChild(banBtn);
    banRow->updateLayout();
    controlsMenu->addChild(banRow);

    controlsMenu->updateLayout();
    m_mainLayer->addChild(controlsMenu);

    // Right Side (Banners)
    m_bannersList = cue::ListNode::create({290.f, 210.f}, {0, 0, 0, 0}, cue::ListBorderStyle::CommentsBlue);
    m_mainLayer->addChildAtPosition(m_bannersList, Anchor::Center, {100.f, -15.f}, false);

    auto scrollbar = Scrollbar::create(m_bannersList->getScrollLayer());
    m_mainLayer->addChildAtPosition(scrollbar, Anchor::Center, {100.f + 290.f / 2 + 5.f, -15.f}, false);

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
            queueInMainThread([popup] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage("Authentication failed.");
                }
            });
            co_return;
        }

        auto req = utils::web::WebRequest();
        auto body = matjson::makeObject({
            {"accountId", adminId},
            {"argonToken", authResult},
            {"targetAccountId", retainedSelf->m_targetAccountId},
            {"amethyst", amy}
        });

        auto res = co_await req.bodyJSON(body).post(fmt::format("{}/admin/updateUserAmethyst", comment::baseUrl));
        bool ok = res.ok();

        queueInMainThread([popup, ok] {
            if (popup && popup->getParent()) {
                if (ok) {
                    popup->showSuccessMessage("Amethyst updated!");
                } else {
                    popup->showFailMessage("Failed to update amethyst.");
                }
            }
        });
        co_return;
    });
}

void CBManageUserPopup::onToggleStaff(CCObject* sender) {
    Ref<Toggler> toggler = static_cast<Toggler*>(sender);

    auto accountData = argon::getGameAccountData();
    auto adminId = accountData.accountId;

    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Toggling staff...");
    popup->show();

    Ref<CBManageUserPopup> retainedSelf = this;
    arc::spawn([retainedSelf, adminId, accountData, popup, toggler]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            queueInMainThread([popup, toggler] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage("Authentication failed.");
                }
                if (toggler) {
                    toggler->toggle();  // revert, without firing the callback
                }
            });
            co_return;
        }

        auto req = utils::web::WebRequest();
        auto body = matjson::makeObject({
            {"accountId", adminId},
            {"argonToken", authResult},
            {"targetAccountId", retainedSelf->m_targetAccountId}
        });

        auto res = co_await req.bodyJSON(body).post(fmt::format("{}/admin/toggleStaff", comment::baseUrl));
        bool ok = res.ok();

        queueInMainThread([retainedSelf, popup, ok, toggler] {
            if (popup && popup->getParent()) {
                if (ok) {
                    popup->showSuccessMessage("Staff status toggled!");
                } else {
                    popup->showFailMessage("Failed to toggle staff status.");
                }
            }
            if (ok) {
                if (retainedSelf) retainedSelf->m_isStaff = !retainedSelf->m_isStaff;
            } else {
                if (toggler) toggler->toggle();  // revert, without firing the callback
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
            queueInMainThread([popup] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage("Authentication failed.");
                }
            });
            co_return;
        }

        auto req = utils::web::WebRequest();
        auto body = matjson::makeObject({
            {"accountId", adminId},
            {"argonToken", authResult},
            {"targetAccountId", retainedSelf->m_targetAccountId},
            {"hours", hours}
        });

        auto res = co_await req.bodyJSON(body).post(fmt::format("{}/admin/tempBan", comment::baseUrl));
        bool ok = res.ok();

        queueInMainThread([popup, ok] {
            if (popup && popup->getParent()) {
                if (ok) {
                    popup->showSuccessMessage("User temp banned!");
                } else {
                    popup->showFailMessage("Failed to ban user.");
                }
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
            queueInMainThread([retainedSelf] { retainedSelf->m_loadingCircle->fadeOut(); });
            co_return;
        }

        auto req = utils::web::WebRequest();
        auto url = fmt::format("{}/admin/userBanners?adminAccountId={}&argonToken={}&accountId={}",
            comment::baseUrl,
            adminId,
            authResult,
            retainedSelf->m_targetAccountId);

        auto res = co_await req.get(url);
        if (!res.ok()) {
            queueInMainThread([retainedSelf] { retainedSelf->m_loadingCircle->fadeOut(); });
            co_return;
        }

        auto jsonRes = res.json();
        if (jsonRes.isErr()) {
            queueInMainThread([retainedSelf] { retainedSelf->m_loadingCircle->fadeOut(); });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isArray()) {
            queueInMainThread([retainedSelf] { retainedSelf->m_loadingCircle->fadeOut(); });
            co_return;
        }

        queueInMainThread([retainedSelf, json = std::move(json)] {
            retainedSelf->m_loadingCircle->fadeOut();
            retainedSelf->m_bannersList->clear();

            if (json.size() == 0) {
                auto emptyLabel = Label::create("No banners.", "goldFont.fnt");
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
    // plain pointer: this popup owns the list, a Ref here would keep it alive forever
    auto self = this;
    auto cell = CBBannerCell::create(CBBannerItem::fromJson(banner), m_bannersList->getContentWidth(), {
        .type = CBBannerCellType::Manage,
        .disableCommentPreview = true,
        .onSave = [self](int bannerId, std::string name, std::string description, int price) {
            self->updateBannerDetails(bannerId, name, description, price);
        },
        .onDelete = [self](int bannerId) { self->deleteBanner(bannerId); },
        .canDelete = Mod::get()->getSavedValue<bool>("is_admin", false),
    });
    if (!cell) return;

    m_bannersList->setCellColor(ccBLACK);
    m_bannersList->addCell(cell);
}

void CBManageUserPopup::deleteBanner(int bannerId) {
    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Deleting Banner...");
    popup->show();

    Ref<CBManageUserPopup> retainedSelf = this;
    auto accountData = argon::getGameAccountData();
    arc::spawn([retainedSelf, bannerId, popup, accountData]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            queueInMainThread([popup] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage("Authentication failed.");
                }
            });
            co_return;
        }

        auto req = utils::web::WebRequest();
        auto body = matjson::makeObject({
            {"accountId", accountData.accountId},
            {"argonToken", authResult},
            {"bannerId", bannerId}
        });

        auto res = co_await req.bodyJSON(body).post(fmt::format("{}/admin/deleteBanner", comment::baseUrl));
        bool ok = res.ok();

        queueInMainThread([retainedSelf, popup, ok] {
            if (popup && popup->getParent()) {
                if (ok) {
                    popup->showSuccessMessage("Banner deleted!");
                } else {
                    popup->showFailMessage("Failed to delete banner.");
                }
            }
            if (ok && retainedSelf && retainedSelf->getParent()) {
                retainedSelf->fetchUserBanners();
            }
        });
        co_return;
    });
}

void CBManageUserPopup::updateBannerDetails(int bannerId, std::string name, std::string description, int price) {
    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Updating Banner...");
    popup->show();

    Ref<CBManageUserPopup> retainedSelf = this;
    auto accountData = argon::getGameAccountData();
    arc::spawn([retainedSelf, bannerId, name, description, price, popup, accountData]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            queueInMainThread([popup] {
                if (popup && popup->getParent()) {
                    popup->showFailMessage("Authentication failed.");
                }
            });
            co_return;
        }

        auto req = utils::web::WebRequest();
        auto body = matjson::makeObject({
            {"accountId", accountData.accountId},
            {"argonToken", authResult},
            {"bannerId", bannerId},
            {"price", price},
            {"name", name},
            {"description", description}
        });

        auto res = co_await req.bodyJSON(body).post(fmt::format("{}/admin/updateBannerDetails", comment::baseUrl));
        bool ok = res.ok();

        queueInMainThread([retainedSelf, popup, ok] {
            if (popup && popup->getParent()) {
                if (ok) {
                    popup->showSuccessMessage("Banner updated!");
                } else {
                    popup->showFailMessage("Failed to update banner.");
                }
            }
        });
        co_return;
    });
}
