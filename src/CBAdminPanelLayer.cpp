#include "CBAdminPanelLayer.hpp"
#include <Geode/utils/web.hpp>
#include <cue/Colors.hpp>
#include <dankmeme.argon/argon.hpp>
#include "Geode/ui/BasedButtonSprite.hpp"
#include "Geode/ui/General.hpp"
#include "Geode/ui/Layout.hpp"
#include "include/CBConstant.hpp"
#include <Geode/ui/Button.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/binding/SetTextPopup.hpp>
#include "CBManageUserPopup.hpp"
#include "CBUserCell.hpp"
#include "CBBannerCell.hpp"
#include "CBPreviewBanner.hpp"
#include "CBProfileBannerPopup.hpp"
#include <Geode/binding/SetTextPopup.hpp>
#include "CBAdminBannerManagePopup.hpp"
#include <Geode/ui/Scrollbar.hpp>
#include <Geode/ui/MDPopup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include "CBPendingBannerReasonPopup.hpp"

using namespace geode::prelude;

CBAdminPanelLayer* CBAdminPanelLayer::create() {
    auto ret = new CBAdminPanelLayer();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBAdminPanelLayer::init() {
    if (!Popup::init(420.f, 280.f, "GJ_square02.png")) return false;

    this->setTitle("Comment Banners Admin Panel");
    addSideArt(m_mainLayer, SideArt::Top, SideArtStyle::PopupBlue);

    m_list = cue::ListNode::create({380.f, 190.f}, cue::DarkBrown, cue::ListBorderStyle::CommentsBlue);
    if (m_list) {
        m_mainLayer->addChildAtPosition(m_list, Anchor::Center, {0.f, -25.f}, false);

        auto scrollbar = Scrollbar::create(m_list->getScrollLayer());
        m_mainLayer->addChildAtPosition(scrollbar, Anchor::Center, {380.f / 2 + 5.f, -25.f}, false);
    }

    auto listBg = NineSlice::create("square02_001.png");
    if (listBg) {
        listBg->setPosition(m_list->getPosition());
        listBg->setContentSize(m_list->getContentSize() + CCSize(5.f, 10.f));
        listBg->setOpacity(100);
        m_mainLayer->addChild(listBg, -1);
    }

    auto tabMenu = CCMenu::create();
    tabMenu->setContentSize({380.f, 30.f});
    tabMenu->setLayout(RowLayout::create()->setGap(10.f));

    auto pendingBtn = Button::createWithNode(
        ButtonSprite::create("Pending", "goldFont.fnt", "GJ_button_01.png", .8f),
        [this](Button* sender) { this->onTabPending(sender); });
    tabMenu->addChild(pendingBtn);

    auto usersBtn = Button::createWithNode(
        ButtonSprite::create("Users", "goldFont.fnt", "GJ_button_01.png", .8f),
        [this](Button* sender) { this->onTabUsers(sender); });
    tabMenu->addChild(usersBtn);

    auto bannersBtn = Button::createWithNode(
        ButtonSprite::create("Banners", "goldFont.fnt", "GJ_button_01.png", .8f),
        [this](Button* sender) { this->onTabBanners(sender); });
    tabMenu->addChild(bannersBtn);

    m_mainLayer->addChildAtPosition(tabMenu, Anchor::Top, {0.f, -50.f}, false);
    tabMenu->updateLayout();

    auto filterMenu = CCMenu::create();
    m_filterBtn = Button::createWithSpriteFrameName("gj_findBtn_001.png", [this](Button*) {
            auto popup = SetTextPopup::create("", (m_currentTab == Tab::Users) ? "AccountID or Username" : "Banner Name, User, or ID", 32, (m_currentTab == Tab::Users) ? "Search Users" : "Search Banners", "Search", true, 60, false, 0, 0);
            popup->m_delegate = this;
            popup->show();
        });
    m_filterBtn->setVisible(false);
    filterMenu->addChild(m_filterBtn);
    m_mainLayer->addChildAtPosition(filterMenu, Anchor::TopRight, {-25.f, -25.f});

    m_paginationMenu = CCMenu::create();
    m_paginationMenu->setLayout(RowLayout::create()->setGap(425.f));
    m_pageLabel = Label::create("Page 1 of 1 (Total: 0)", "goldFont.fnt");
    m_pageLabel->setLimitLabelWidth(120.f, 0.4f, 0.1f);
    m_mainLayer->addChildAtPosition(m_pageLabel, Anchor::Bottom, {0.f, 12.f}, false);

    {
        m_prevBtn = Button::createWithSpriteFrameName("GJ_arrow_01_001.png", [this](Button* sender) { this->onPrevPage(sender); });
        m_prevBtn->setPosition({-70.f, 0.f});
        m_paginationMenu->addChild(m_prevBtn);
    }

    {
        m_nextBtn = Button::createWithSpriteFrameName("GJ_arrow_01_001.png", [this](Button* sender) { this->onNextPage(sender); });
        static_cast<CCSprite*>(m_nextBtn->getDisplayNode())->setFlipX(true);
        m_nextBtn->setPosition({70.f, 0.f});
        m_paginationMenu->addChild(m_nextBtn);
    }
    m_mainLayer->addChildAtPosition(m_paginationMenu, Anchor::Center, {0, -25.f}, false);
    m_paginationMenu->updateLayout();

    fetchPendingBanners();

    return true;
}

void CBAdminPanelLayer::onTabPending(CCObject*) {
    if (m_currentTab == Tab::Pending) return;
    m_currentTab = Tab::Pending;
    m_filterBtn->setVisible(false);
    fetchPendingBanners();
}

void CBAdminPanelLayer::onTabUsers(CCObject*) {
    if (m_currentTab == Tab::Users) return;
    m_currentTab = Tab::Users;
    m_filterBtn->setVisible(true);
    fetchUsers();
}

void CBAdminPanelLayer::onTabBanners(CCObject*) {
    if (m_currentTab == Tab::Banners) return;
    m_currentTab = Tab::Banners;
    m_filterBtn->setVisible(true);
    fetchBanners();
}

void CBAdminPanelLayer::processPendingBanner(int id, bool approve, const std::string& reason) {
    auto accountId = argon::getGameAccountData().accountId;
    std::string actionStr = approve ? "Approving banner..." : "Rejecting banner...";
    std::string endpoint = approve ? "/approveBanner" : "/rejectBanner";

    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, actionStr);
    popup->show();
    Ref<CBAdminPanelLayer> retainedSelf = this;
    std::string authToken = this->m_authToken;
    arc::spawn([retainedSelf, id, accountId, authToken, endpoint, reason, approve, popup]() -> arc::Future<> {
        auto req = utils::web::WebRequest();
        auto body = matjson::makeObject({{"accountId", accountId},
            {"argonToken", authToken},
            {"id", id},
            {"reason", reason}});
        auto res = co_await req.bodyJSON(body).post(fmt::format("{}{}", comment::baseUrl, endpoint));
        bool ok = res.ok();
        queueInMainThread([retainedSelf, popup, ok, approve] {
            if (popup && popup->getParent()) {
                if (ok) {
                    popup->showSuccessMessage(approve ? "Banner approved!" : "Banner rejected!");
                } else {
                    popup->showFailMessage(approve ? "Failed to approve banner." : "Failed to reject banner.");
                }
            }
            if (ok && retainedSelf && retainedSelf->getParent()) {
                retainedSelf->fetchPendingBanners();
            }
        });
        co_return;
    });
}

void CBAdminPanelLayer::fetchPendingBanners() {
    // drop the previous tab's cells while the new data loads
    m_list->clear();
    if (m_loadingCircle) m_loadingCircle->removeFromParent();
    m_loadingCircle = cue::LoadingCircle::create(true);
    m_list->addChildAtPosition(m_loadingCircle, Anchor::Center, CCPointZero, false);
    m_loadingCircle->fadeIn();

    if (auto empty = m_list->getChildByID("empty-label")) {
        empty->removeFromParent();
    }

    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;

    Ref<CBAdminPanelLayer> retainedSelf = this;
    arc::spawn([retainedSelf, accountId, accountData]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Authentication failed.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto authToken = std::move(authResult);
        auto req = utils::web::WebRequest();
        auto url = fmt::format("{}/pendingBanners?accountId={}&argonToken={}", comment::baseUrl, accountId, authToken);

        auto response = co_await req.get(url);
        if (!response.ok()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Failed to fetch pending banners.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto jsonRes = response.json();
        if (jsonRes.isErr()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isArray()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        queueInMainThread([retainedSelf, json = std::move(json), accountId, authToken] {
            retainedSelf->m_loadingCircle->fadeOut();
            if (retainedSelf->m_currentTab != Tab::Pending) return;
            retainedSelf->m_currentData = json;
            retainedSelf->m_authToken = authToken;
            retainedSelf->m_page = 0;
            retainedSelf->renderPage();
            retainedSelf->m_list->updateLayout();
        });

        co_return;
    });
}

void CBAdminPanelLayer::fetchUsers(std::string query) {
    // drop the previous tab's cells while the new data loads
    m_list->clear();
    if (m_loadingCircle) m_loadingCircle->removeFromParent();
    m_loadingCircle = cue::LoadingCircle::create(true);

    m_list->addChildAtPosition(m_loadingCircle, Anchor::Center, CCPointZero, false);
    m_loadingCircle->fadeIn();

    if (auto empty = m_list->getChildByID("empty-label")) {
        empty->removeFromParent();
    }

    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;

    Ref<CBAdminPanelLayer> retainedSelf = this;
    arc::spawn([retainedSelf, accountId, accountData, query]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Authentication failed.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto authToken = std::move(authResult);
        auto req = utils::web::WebRequest();

        std::string encodedQuery = query;
        size_t pos = 0;
        while ((pos = encodedQuery.find(" ", pos)) != std::string::npos) {
            encodedQuery.replace(pos, 1, "%20");
            pos += 3;
        }
        auto url = fmt::format("{}/admin/searchUsers?accountId={}&argonToken={}&query={}", comment::baseUrl, accountId, authToken, encodedQuery);

        auto response = co_await req.get(url);
        if (!response.ok()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Failed to fetch users.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto jsonRes = response.json();
        if (jsonRes.isErr()) {
            queueInMainThread([retainedSelf] { retainedSelf->m_loadingCircle->fadeOut(); });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isArray()) {
            queueInMainThread([retainedSelf] { retainedSelf->m_loadingCircle->fadeOut(); });
            co_return;
        }

        queueInMainThread([retainedSelf, json = std::move(json), accountId, authToken] {
            retainedSelf->m_loadingCircle->fadeOut();
            if (retainedSelf->m_currentTab != Tab::Users) return;
            retainedSelf->m_currentData = json;
            retainedSelf->m_authToken = authToken;
            retainedSelf->m_page = 0;
            retainedSelf->renderPage();
            retainedSelf->m_list->updateLayout();
        });

        co_return;
    });
}

void CBAdminPanelLayer::fetchBanners(std::string query) {
    // drop the previous tab's cells while the new data loads
    m_list->clear();
    if (m_loadingCircle) m_loadingCircle->removeFromParent();
    m_loadingCircle = cue::LoadingCircle::create(true);
    m_list->addChildAtPosition(m_loadingCircle, Anchor::Center, CCPointZero, false);
    m_loadingCircle->fadeIn();

    if (auto empty = m_list->getChildByID("empty-label")) {
        empty->removeFromParent();
    }

    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;

    Ref<CBAdminPanelLayer> retainedSelf = this;
    arc::spawn([retainedSelf, accountId, accountData, query]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Authentication failed.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto authToken = std::move(authResult);
        auto req = utils::web::WebRequest();

        std::string encodedQuery = query;
        size_t pos = 0;
        while ((pos = encodedQuery.find(" ", pos)) != std::string::npos) {
            encodedQuery.replace(pos, 1, "%20");
            pos += 3;
        }
        auto url = fmt::format("{}/admin/searchBanners?accountId={}&argonToken={}&query={}", comment::baseUrl, accountId, authToken, encodedQuery);

        auto response = co_await req.get(url);
        if (!response.ok()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Failed to fetch banners.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto jsonRes = response.json();
        if (jsonRes.isErr()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isArray()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        queueInMainThread([retainedSelf, json = std::move(json), accountId, authToken] {
            retainedSelf->m_loadingCircle->fadeOut();
            if (retainedSelf->m_currentTab != Tab::Banners) return;
            retainedSelf->m_currentData = json;
            retainedSelf->m_authToken = authToken;
            retainedSelf->m_page = 0;
            retainedSelf->renderPage();
            retainedSelf->m_list->updateLayout();
        });

        co_return;
    });
}

void CBAdminPanelLayer::updatePagination() {
    int itemsPerPage = (m_currentTab == Tab::Users) ? 30 : 20;
    int totalItems = 0;
    if (m_currentData.isArray()) {
        totalItems = m_currentData.asArray().unwrap().size();
    }
    int totalPages = std::max(1, (totalItems + itemsPerPage - 1) / itemsPerPage);

    if (m_pageLabel) {
        m_pageLabel->setString(fmt::format("Page {} of {} (Total: {})", m_page + 1, totalPages, totalItems).c_str());
        m_pageLabel->setLimitLabelWidth(120.f, 0.4f, 0.1f);
    }

    if (m_prevBtn) {
        m_prevBtn->setVisible(m_page > 0);
    }
    if (m_nextBtn) {
        m_nextBtn->setVisible(m_page < totalPages - 1);
    }
}

void CBAdminPanelLayer::setTextPopupClosed(SetTextPopup* popup, std::string text) {
    if (m_currentTab == Tab::Users) {
        this->fetchUsers(text);
    } else if (m_currentTab == Tab::Banners) {
        this->fetchBanners(text);
    }
}

void CBAdminPanelLayer::onNextPage(CCObject*) {
    m_page++;
    renderPage();
}

void CBAdminPanelLayer::onPrevPage(CCObject*) {
    if (m_page > 0) m_page--;
    renderPage();
}

void CBAdminPanelLayer::renderPage() {
    if (!m_currentData.isArray()) return;
    auto jsonArray = m_currentData.asArray().unwrap();

    m_list->clear();
    if (auto empty = m_list->getChildByID("empty-label")) {
        empty->removeFromParent();
    }

    if (jsonArray.size() == 0) {
        std::string emptyStr = "No items found.";
        if (m_currentTab == Tab::Pending)
            emptyStr = "No pending banners.";
        else if (m_currentTab == Tab::Banners)
            emptyStr = "No banners found.";
        else if (m_currentTab == Tab::Users)
            emptyStr = "No users found.";

        auto emptyLabel = Label::create(emptyStr.c_str(), "goldFont.fnt");
        emptyLabel->setScale(0.8f);
        emptyLabel->setID("empty-label");
        m_list->addChildAtPosition(emptyLabel, Anchor::Center);
        updatePagination();
        return;
    }

    int itemsPerPage = (m_currentTab == Tab::Users) ? 30 : 20;
    size_t start = m_page * itemsPerPage;
    size_t end = std::min(start + itemsPerPage, jsonArray.size());

    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;

    if (m_currentTab == Tab::Pending) {
        m_list->setCellColor(ccColor4B{0, 0, 0, 0});
        for (size_t i = start; i < end; ++i) {
            auto cell = CBBannerCell::create(CBBannerItem::fromJson(jsonArray[i]), 380.f, {
                .type = CBBannerCellType::Pending,
                .disableCommentPreview = true,
                .adminPanel = this,
            });
            if (cell) m_list->addCell(cell);
        }

    } else if (m_currentTab == Tab::Users) {
        m_list->setCellColor(ccColor4B{0, 0, 0, 0});
        for (size_t i = start; i < end; ++i) {
            auto item = jsonArray[i];
            auto cell = CBUserCell::create(CBUserData::fromJson(item), 380.f, {
                                                                                  .showRole = true,
                                                                                  .onManage = [item]() {
                                                                                      if (auto popup = CBManageUserPopup::create(item)) {
                                                                                          popup->show();
                                                                                      }
                                                                                  },
                                                                              });
            if (cell) {
                m_list->addCell(cell);
            }
        }

    } else if (m_currentTab == Tab::Banners) {
        m_list->setCellColor(ccColor4B{0, 0, 0, 0});
        // plain pointer: the panel owns these cells, a Ref here would keep the panel alive forever
        auto self = this;
        for (size_t i = start; i < end; ++i) {
            auto item = jsonArray[i];
            auto cell = CBBannerCell::create(CBBannerItem::fromJson(item), 380.f, {
                .type = CBBannerCellType::Admin,
                .disableCommentPreview = true,
                .adminData = item,
                .onFeaturedChanged = [self](int bannerId, bool featured) {
                    // keep the cached list in sync so paging doesn't show the old state
                    for (auto& entry : self->m_currentData) {
                        if (entry["id"].asInt().unwrapOr(0) == bannerId) {
                            entry["isFeatured"] = featured;
                        }
                    }
                },
            });
            if (cell) m_list->addCell(cell);
        }
    }

    m_list->updateLayout();
    updatePagination();
}
