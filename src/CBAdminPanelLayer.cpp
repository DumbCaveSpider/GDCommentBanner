#include "CBAdminPanelLayer.hpp"
#include <Geode/utils/web.hpp>
#include <argon/argon.hpp>
#include "Geode/ui/BasedButtonSprite.hpp"
#include "Geode/ui/General.hpp"
#include "Geode/ui/Layout.hpp"
#include "include/CBConstant.hpp"
#include <Geode/ui/Button.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/binding/SetTextPopup.hpp>
#include "CBManageUserPopup.hpp"
#include <Geode/binding/SetTextPopup.hpp>
#include "CBAdminBannerManagePopup.hpp"
#include <Geode/ui/Scrollbar.hpp>

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

    m_list = cue::ListNode::create({380.f, 190.f}, {0, 0, 0, 0}, cue::ListBorderStyle::CommentsBlue);
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

    auto pendingBtn = geode::Button::createWithNode(
        ButtonSprite::create("Pending", "goldFont.fnt", "GJ_button_01.png", .8f),
        [this](geode::Button* sender) { this->onTabPending(sender); });
    tabMenu->addChild(pendingBtn);

    auto usersBtn = geode::Button::createWithNode(
        ButtonSprite::create("Users", "goldFont.fnt", "GJ_button_01.png", .8f),
        [this](geode::Button* sender) { this->onTabUsers(sender); });
    tabMenu->addChild(usersBtn);

    auto bannersBtn = geode::Button::createWithNode(
        ButtonSprite::create("Banners", "goldFont.fnt", "GJ_button_01.png", .8f),
        [this](geode::Button* sender) { this->onTabBanners(sender); });
    tabMenu->addChild(bannersBtn);

    m_mainLayer->addChildAtPosition(tabMenu, Anchor::Top, {0.f, -50.f}, false);
    tabMenu->updateLayout();

    auto filterMenu = CCMenu::create();
    m_filterBtn = geode::Button::createWithNode(
        CCSprite::createWithSpriteFrameName("gj_findBtn_001.png"),
        [this](geode::Button*) {
            auto popup = SetTextPopup::create("", (m_currentTab == Tab::Users) ? "AccountID or Username" : "Banner Name, User, or ID", 32, (m_currentTab == Tab::Users) ? "Search Users" : "Search Banners", "Search", true, 60);
            popup->m_delegate = this;
            popup->show();
        });
    m_filterBtn->setVisible(false);
    filterMenu->addChild(m_filterBtn);
    m_mainLayer->addChildAtPosition(filterMenu, Anchor::TopRight, {-25.f, -25.f});

    m_paginationMenu = CCMenu::create();
    m_paginationMenu->setLayout(RowLayout::create()->setGap(425.f));
    m_pageLabel = CCLabelBMFont::create("Page 1 of 1 (Total: 0)", "goldFont.fnt");
    m_pageLabel->limitLabelWidth(120.f, 0.4f, 0.1f);
    m_mainLayer->addChildAtPosition(m_pageLabel, Anchor::Bottom, {0.f, 12.f}, false);

    if (auto prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
        m_prevBtn = CCMenuItemSpriteExtra::create(prevSpr, this, menu_selector(CBAdminPanelLayer::onPrevPage));
        m_prevBtn->setPosition({-70.f, 0.f});
        m_paginationMenu->addChild(m_prevBtn);
    }

    if (auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
        nextSpr->setFlipX(true);
        m_nextBtn = CCMenuItemSpriteExtra::create(nextSpr, this, menu_selector(CBAdminPanelLayer::onNextPage));
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

void CBAdminPanelLayer::fetchPendingBanners() {
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
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Authentication failed.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto authToken = std::move(authResult);
        auto req = geode::utils::web::WebRequest();
        auto url = fmt::format("{}/pendingBanners?accountId={}&argonToken={}", comment::baseUrl, accountId, authToken);

        auto response = co_await req.get(url);
        if (!response.ok()) {
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Failed to fetch pending banners.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto jsonRes = response.json();
        if (jsonRes.isErr()) {
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isArray()) {
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        geode::queueInMainThread([retainedSelf, json = std::move(json), accountId, authToken] {
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
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Authentication failed.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto authToken = std::move(authResult);
        auto req = geode::utils::web::WebRequest();

        std::string encodedQuery = query;
        size_t pos = 0;
        while ((pos = encodedQuery.find(" ", pos)) != std::string::npos) {
            encodedQuery.replace(pos, 1, "%20");
            pos += 3;
        }
        auto url = fmt::format("{}/admin/searchUsers?accountId={}&argonToken={}&query={}", comment::baseUrl, accountId, authToken, encodedQuery);

        auto response = co_await req.get(url);
        if (!response.ok()) {
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Failed to fetch users.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto jsonRes = response.json();
        if (jsonRes.isErr()) {
            geode::queueInMainThread([retainedSelf] { retainedSelf->m_loadingCircle->fadeOut(); });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isArray()) {
            geode::queueInMainThread([retainedSelf] { retainedSelf->m_loadingCircle->fadeOut(); });
            co_return;
        }

        geode::queueInMainThread([retainedSelf, json = std::move(json), accountId, authToken] {
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
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Authentication failed.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto authToken = std::move(authResult);
        auto req = geode::utils::web::WebRequest();

        std::string encodedQuery = query;
        size_t pos = 0;
        while ((pos = encodedQuery.find(" ", pos)) != std::string::npos) {
            encodedQuery.replace(pos, 1, "%20");
            pos += 3;
        }
        auto url = fmt::format("{}/admin/searchBanners?accountId={}&argonToken={}&query={}", comment::baseUrl, accountId, authToken, encodedQuery);

        auto response = co_await req.get(url);
        if (!response.ok()) {
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Failed to fetch banners.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto jsonRes = response.json();
        if (jsonRes.isErr()) {
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isArray()) {
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        geode::queueInMainThread([retainedSelf, json = std::move(json), accountId, authToken] {
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
        m_pageLabel->limitLabelWidth(120.f, 0.4f, 0.1f);
    }

    if (m_prevBtn) {
        m_prevBtn->setVisible(m_page > 0);
    }
    if (m_nextBtn) {
        m_nextBtn->setVisible(m_page < totalPages - 1);
    }
}

void CBAdminPanelLayer::setTextPopupClosed(SetTextPopup* popup, gd::string text) {
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

        auto emptyLabel = CCLabelBMFont::create(emptyStr.c_str(), "goldFont.fnt");
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
        for (size_t i = start; i < end; ++i) {
            auto item = jsonArray[i];
            std::string name = item["name"].asString().unwrapOr("Unknown");
            std::string user = item["username"].asString().unwrapOr("Unknown");
            int id = item["id"].asInt().unwrapOr(0);
            int targetAccountId = item["accountId"].asInt().unwrapOr(0);
            std::string desc = item["description"].asString().unwrapOr("");
            int price = item["price"].asInt().unwrapOr(0);
            bool isLimited = item["isLimited"].asBool().unwrapOr(false);
            bool isFeatured = item["isFeatured"].asBool().unwrapOr(false);
            int amount = item["amount"].asInt().unwrapOr(0);
            std::string imageUrl = item["imageUrl"].asString().unwrapOr("");
            if (!imageUrl.empty()) {
                imageUrl = fmt::format("{}{}", comment::baseUrl, imageUrl);
            }

            float width = 380.f;
            float cellHeight = 96.f;
            auto cell = CCLayer::create();
            cell->setContentSize({width, cellHeight});

            // Background
            if (auto background = NineSlice::createWithSpriteFrameName("geode.loader/tab-bg.png")) {
                background->setContentSize({width - 5, cellHeight});
                background->setPosition({width / 2.f, cellHeight / 2.f});
                if (isFeatured) {
                    background->runAction(CCRepeatForever::create(CCSequence::create(
                        CCTintTo::create(1.f, 255, 255, 50),
                        CCTintTo::create(1.f, background->getColor().r, background->getColor().g, background->getColor().b),
                        nullptr)));
                }
                cell->addChild(background);
            }

            // Image
            auto sprite = comment::createBannerNode(imageUrl, {344.f, 104.f});
            sprite->setPosition({width / 2.f, cellHeight - 30.f});
            cell->addChild(sprite);

            // Name
            auto nameLabel = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
            nameLabel->setAnchorPoint({0.f, 0.5f});
            float nameX = 10.f;
            if (isFeatured) {
                if (auto starIcon = CCSprite::createWithSpriteFrameName("GJ_sStarsIcon_001.png")) {
                    starIcon->setScale(0.8f);
                    starIcon->setPosition({nameX, 25.f});
                    starIcon->setAnchorPoint({0.f, 0.5f});
                    cell->addChild(starIcon);
                    nameX += starIcon->getContentSize().width * starIcon->getScale() + 4.f;
                }
            }
            if (isLimited) {
                if (auto limitIcon = CCSprite::createWithSpriteFrameName("GJ_sRecentIcon_001.png")) {
                    limitIcon->setScale(0.8f);
                    limitIcon->setPosition({nameX, 25.f});
                    limitIcon->setAnchorPoint({0.f, 0.5f});
                    cell->addChild(limitIcon);
                    nameX += limitIcon->getContentSize().width * limitIcon->getScale() + 4.f;
                }
            }

            if (isFeatured && isLimited) {
                nameLabel->runAction(CCRepeatForever::create(CCSequence::create(
                    CCTintTo::create(1.f, 255, 255, 50),
                    CCTintTo::create(1.f, 255, 150, 255),
                    nullptr)));
            } else if (isFeatured) {
                nameLabel->runAction(CCRepeatForever::create(CCSequence::create(
                    CCTintTo::create(1.f, 255, 255, 50),
                    CCTintTo::create(1.f, 255, 255, 255),
                    nullptr)));
            } else if (isLimited) {
                nameLabel->runAction(CCRepeatForever::create(CCSequence::create(
                    CCTintTo::create(1.f, 255, 150, 255),
                    CCTintTo::create(1.f, 255, 255, 255),
                    nullptr)));
            }
            nameLabel->setPosition({nameX, 25.f});
            nameLabel->limitLabelWidth(60.f, 0.5f, 0.2f);
            cell->addChild(nameLabel);

            float currentX = nameX + (nameLabel->getContentSize().width * nameLabel->getScale()) + 8.f;

            // User
            if (!user.empty()) {
                auto usernameLabel = CCLabelBMFont::create(fmt::format("By {}", user).c_str(), "goldFont.fnt");
                usernameLabel->setScale(0.4f);

                auto userBtn = geode::Button::createWithNode(usernameLabel, [targetAccountId](geode::Button*) {
                    ProfilePage::create(targetAccountId, false)->show();
                });
                userBtn->setAnchorPoint({0.f, 0.5f});
                userBtn->setPosition({currentX, 25.f});
                cell->addChild(userBtn);

                currentX += (usernameLabel->getContentSize().width * usernameLabel->getScale()) + 15.f;
            } else {
                currentX += 15.f;
            }

            // Price
            if (auto priceLabel = CCLabelBMFont::create(fmt::format("{}", GameToolbox::pointsToString(price)).c_str(), "bigFont.fnt")) {
                priceLabel->setAnchorPoint({0.f, 0.5f});
                priceLabel->limitLabelWidth(100.f, 0.5f, 0.2f);
                priceLabel->setPosition({0.f, 0.f});

                auto priceNode = CCNode::create();
                priceNode->setPosition({currentX, 25.f});
                priceNode->addChild(priceLabel);

                if (auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr)) {
                    amethystIcon->setScale(0.5f);
                    amethystIcon->setAnchorPoint({0.f, 0.5f});
                    auto priceWidth = priceLabel->getContentSize().width * priceLabel->getScale();
                    amethystIcon->setPosition({priceWidth + 4.f, 0.f});
                    priceNode->addChild(amethystIcon);
                    currentX += priceWidth + (amethystIcon->getContentSize().width * amethystIcon->getScale());
                } else {
                    currentX += priceLabel->getContentSize().width * priceLabel->getScale();
                }
                cell->addChild(priceNode);
                currentX += 15.f;
            }

            // Amount
            if (isLimited) {
                if (auto amountLabel = CCLabelBMFont::create(fmt::format("Amount: {}", amount).c_str(), "goldFont.fnt")) {
                    amountLabel->setAnchorPoint({0.f, 0.5f});
                    amountLabel->limitLabelWidth(60.f, 0.4f, 0.2f);
                    amountLabel->setPosition({26.f, 40.f});
                    cell->addChild(amountLabel);
                }
            }

            // Description
            if (!desc.empty()) {
                auto descLabel = SimpleTextArea::create(desc.c_str(), "chatFont.fnt", 0.4f, 280.f);
                descLabel->setAnchorPoint({0.f, 0.5f});
                descLabel->setPosition({10.f, 10.f});
                descLabel->setMaxLines(2);
                cell->addChild(descLabel);
            }

            auto menu = CCMenu::create();
            menu->setContentSize({70.f, 30.f});
            menu->setPosition({335.f, 25.f});
            menu->setLayout(RowLayout::create()
                    ->setAxisAlignment(AxisAlignment::Center)
                    ->setGap(5.f));

            auto approveSpr = CircleButtonSprite::createWithSpriteFrameName("GJ_completesIcon_001.png", 0.7f, CircleBaseColor::Green, CircleBaseSize::Small);
            approveSpr->setScale(0.8f);
            auto approveBtn = geode::Button::createWithNode(
                approveSpr,
                [this, id, accountId](geode::Button*) {
                    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Approving banner...");
                    popup->show();
                    arc::spawn([this, id, accountId, popup]() -> arc::Future<> {
                        auto req = geode::utils::web::WebRequest();
                        req.header("Content-Type", "application/x-www-form-urlencoded");
                        std::string body = fmt::format("accountId={}&argonToken={}&id={}", accountId, this->m_authToken, id);
                        auto res = co_await req.bodyString(body).post(fmt::format("{}/approveBanner", comment::baseUrl));
                        bool ok = res.ok();
                        geode::queueInMainThread([this, popup, ok] {
                            if (ok) {
                                popup->showSuccessMessage("Banner approved!");
                                this->fetchPendingBanners();
                            } else {
                                popup->showFailMessage("Failed to approve banner.");
                            }
                        });
                        co_return;
                    });
                });
            approveBtn->setPosition({-35.f, 0.f});
            menu->addChild(approveBtn);

            auto rejectSpr = CircleButtonSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png", 0.7f, CircleBaseColor::Red, CircleBaseSize::Small);
            rejectSpr->setScale(0.8f);
            auto rejectBtn = geode::Button::createWithNode(
                rejectSpr,
                [this, id, accountId](geode::Button*) {
                    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Rejecting banner...");
                    popup->show();
                    arc::spawn([this, id, accountId, popup]() -> arc::Future<> {
                        auto req = geode::utils::web::WebRequest();
                        req.header("Content-Type", "application/x-www-form-urlencoded");
                        std::string body = fmt::format("accountId={}&argonToken={}&id={}", accountId, this->m_authToken, id);
                        auto res = co_await req.bodyString(body).post(fmt::format("{}/rejectBanner", comment::baseUrl));
                        bool ok = res.ok();
                        geode::queueInMainThread([this, popup, ok] {
                            if (ok) {
                                popup->showSuccessMessage("Banner rejected!");
                                this->fetchPendingBanners();
                            } else {
                                popup->showFailMessage("Failed to reject banner.");
                            }
                        });
                        co_return;
                    });
                });
            rejectBtn->setPosition({35.f, 0.f});
            menu->addChild(rejectBtn);

            cell->addChild(menu);
            menu->updateLayout();

            m_list->setCellColor(ccColor4B{0, 0, 0, 0});
            m_list->addCell(cell);
        }

    } else if (m_currentTab == Tab::Users) {
        for (size_t i = start; i < end; ++i) {
            auto item = jsonArray[i];
            auto cell = CCLayer::create();
            cell->setContentSize({380.f, 40.f});

            std::string equippedBannerUrl = item["equippedBannerUrl"].asString().unwrapOr("");
            if (!equippedBannerUrl.empty()) {
                equippedBannerUrl = fmt::format("{}", equippedBannerUrl);
            }

            if (!equippedBannerUrl.empty()) {
                auto sprite = LazySprite::create({380.f, 40.f}, true);
                sprite->setAutoResize(true);
                sprite->setPosition({190.f, 20.f});
                sprite->loadFromUrl(equippedBannerUrl, LazySprite::Format::kFmtWebp, false);
                cell->addChild(sprite, -2);

                auto bg = CCLayerGradient::create({0, 0, 0, 255}, {0, 0, 0, 0}, {1, 0});
                bg->setContentSize(cell->getContentSize());
                bg->setAnchorPoint({0, 0});
                bg->setOpacity(255);
                cell->addChild(bg, -1);
            }

            std::string user = item["username"].asString().unwrapOr("Unknown");
            int targetId = item["accountId"].asInt().unwrapOr(0);
            bool isAdmin = item["is_admin"].asBool().unwrapOr(false);
            bool isStaff = item["is_staff"].asBool().unwrapOr(false);
            int amethyst = item["amethyst"].asInt().unwrapOr(0);

            auto label = CCLabelBMFont::create(fmt::format("{}", user, targetId).c_str(), "goldFont.fnt");
            label->setScale(0.6f);
            float labelWidth = label->getContentSize().width * label->getScale();

            auto btn = geode::Button::createWithNode(
                label,
                [targetId](geode::Button*) {
                    ProfilePage::create(targetId, false)->show();
                });
            btn->setAnchorPoint({0, 0.5f});
            btn->setPosition({10.f, 26.f});

            auto menu = CCMenu::create();
            menu->setPosition({0, 0});
            menu->addChild(btn);
            float currentX = 10.f + labelWidth + 5.f;

            std::string rankStr = isAdmin ? "Admin" : (isStaff ? "Staff" : "User");
            auto rankLabel = CCLabelBMFont::create(rankStr.c_str(), "chatFont.fnt");
            rankLabel->setScale(0.4f);
            rankLabel->setAnchorPoint({0, 0.5f});
            rankLabel->setPosition({10.f, 12.f});
            if (isAdmin) {
                rankLabel->setColor(ccColor3B{255, 100, 100});
            } else if (isStaff) {
                rankLabel->setColor(ccColor3B{100, 255, 100});
            } else {
                rankLabel->setColor(ccColor3B{200, 200, 200});
            }
            cell->addChild(rankLabel);

            if (auto amyIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr)) {
                amyIcon->setScale(0.4f);
                amyIcon->setAnchorPoint({0, 0.5f});
                amyIcon->setPosition({currentX, 20.f});
                cell->addChild(amyIcon);
                currentX += (amyIcon->getContentSize().width * amyIcon->getScale()) + 2.f;
            }

            auto amyLabel = CCLabelBMFont::create(fmt::format("{}", amethyst).c_str(), "bigFont.fnt");
            amyLabel->setScale(0.4f);
            amyLabel->setAnchorPoint({0, 0.5f});
            amyLabel->setPosition({currentX, 20.f});
            cell->addChild(amyLabel);

            auto manageBtn = geode::Button::createWithNode(
                ButtonSprite::create("Manage", "goldFont.fnt", "GJ_button_01.png", 0.5f),
                [item](geode::Button*) {
                    CBManageUserPopup::create(item)->show();
                });
            manageBtn->setAnchorPoint({1.f, 0.5f});
            manageBtn->setPosition({370.f, 20.f});
            menu->addChild(manageBtn);

            cell->addChild(menu);

            m_list->setCellColor(ccColor4B{0, 0, 0, 0});
            m_list->addCell(cell);
        }

    } else if (m_currentTab == Tab::Banners) {
        for (size_t i = start; i < end; ++i) {
            auto item = jsonArray[i];
            std::string name = item["name"].asString().unwrapOr("Unknown");
            std::string user = item["username"].asString().unwrapOr("Unknown");
            int id = item["id"].asInt().unwrapOr(0);
            int targetAccountId = item["accountId"].asInt().unwrapOr(0);
            std::string desc = item["description"].asString().unwrapOr("");
            int price = item["price"].asInt().unwrapOr(0);
            bool isLimited = item["isLimited"].asBool().unwrapOr(false);
            bool isFeatured = item["isFeatured"].asBool().unwrapOr(false);
            int amount = item["amount"].asInt().unwrapOr(0);
            std::string imageUrl = item["imageUrl"].asString().unwrapOr("");

            float width = 380.f;
            float cellHeight = 96.f;
            auto cell = CCLayer::create();
            cell->setContentSize({width, cellHeight});

            // Background
            if (auto background = NineSlice::createWithSpriteFrameName("geode.loader/tab-bg.png")) {
                background->setContentSize({width - 5, cellHeight});
                background->setPosition({width / 2.f, cellHeight / 2.f});
                if (isFeatured) {
                    background->runAction(CCRepeatForever::create(CCSequence::create(
                        CCTintTo::create(1.f, 255, 255, 50),
                        CCTintTo::create(1.f, background->getColor().r, background->getColor().g, background->getColor().b),
                        nullptr)));
                }
                cell->addChild(background);
            }

            // Image
            auto sprite = LazySprite::create({344.f, 104.f}, true);
            sprite->setAutoResize(true);
            sprite->setPosition({width / 2.f, cellHeight - 30.f});
            if (!imageUrl.empty()) {
                sprite->loadFromUrl(imageUrl, LazySprite::Format::kFmtWebp, false);
            }
            cell->addChild(sprite);

            // Name
            auto nameLabel = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
            nameLabel->setAnchorPoint({0.f, 0.5f});
            float nameX = 10.f;
            if (isFeatured) {
                if (auto starIcon = CCSprite::createWithSpriteFrameName("GJ_sStarsIcon_001.png")) {
                    starIcon->setScale(0.8f);
                    starIcon->setPosition({nameX, 25.f});
                    starIcon->setAnchorPoint({0.f, 0.5f});
                    cell->addChild(starIcon);
                    nameX += starIcon->getContentSize().width * starIcon->getScale() + 4.f;
                }
            }
            if (isLimited) {
                if (auto limitIcon = CCSprite::createWithSpriteFrameName("GJ_sRecentIcon_001.png")) {
                    limitIcon->setScale(0.8f);
                    limitIcon->setPosition({nameX, 25.f});
                    limitIcon->setAnchorPoint({0.f, 0.5f});
                    cell->addChild(limitIcon);
                    nameX += limitIcon->getContentSize().width * limitIcon->getScale() + 4.f;
                }
            }

            if (isFeatured && isLimited) {
                nameLabel->runAction(CCRepeatForever::create(CCSequence::create(
                    CCTintTo::create(1.f, 255, 255, 50),
                    CCTintTo::create(1.f, 255, 150, 255),
                    nullptr)));
            } else if (isFeatured) {
                nameLabel->runAction(CCRepeatForever::create(CCSequence::create(
                    CCTintTo::create(1.f, 255, 255, 50),
                    CCTintTo::create(1.f, 255, 255, 255),
                    nullptr)));
            } else if (isLimited) {
                nameLabel->runAction(CCRepeatForever::create(CCSequence::create(
                    CCTintTo::create(1.f, 255, 150, 255),
                    CCTintTo::create(1.f, 255, 255, 255),
                    nullptr)));
            }
            nameLabel->setPosition({nameX, 25.f});
            nameLabel->limitLabelWidth(100.f, 0.5f, 0.2f);
            cell->addChild(nameLabel);

            float currentX = nameX + (nameLabel->getContentSize().width * nameLabel->getScale()) + 8.f;

            // User
            if (!user.empty()) {
                auto usernameLabel = CCLabelBMFont::create(fmt::format("By {}", user).c_str(), "goldFont.fnt");
                usernameLabel->setScale(0.4f);

                auto userBtn = geode::Button::createWithNode(usernameLabel, [targetAccountId](geode::Button*) {
                    ProfilePage::create(targetAccountId, false)->show();
                });
                userBtn->setAnchorPoint({0.f, 0.5f});
                userBtn->setPosition({currentX, 25.f});
                cell->addChild(userBtn);

                currentX += (usernameLabel->getContentSize().width * usernameLabel->getScale()) + 15.f;
            } else {
                currentX += 15.f;
            }

            // Description
            if (!desc.empty()) {
                auto descLabel = SimpleTextArea::create(desc.c_str(), "chatFont.fnt", 0.4f, 280.f);
                descLabel->setAnchorPoint({0.f, 0.5f});
                descLabel->setPosition({10.f, 10.f});
                descLabel->setMaxLines(2);
                cell->addChild(descLabel);
            }

            auto menu = CCMenu::create();
            menu->setContentSize({100.f, 30.f});
            menu->setPosition({320.f, 25.f});
            menu->setLayout(RowLayout::create()
                    ->setAxisAlignment(AxisAlignment::Center)
                    ->setGap(5.f));

            auto manageBtn = geode::Button::createWithNode(
                ButtonSprite::create("Manage", "goldFont.fnt", "GJ_button_01.png", 0.5f),
                [item](geode::Button*) {
                    CBAdminBannerManagePopup::create(item)->show();
                });
            menu->addChild(manageBtn);

            cell->addChild(menu);
            menu->updateLayout();

            m_list->setCellColor(ccColor4B{0, 0, 0, 0});
            m_list->addCell(cell);
        }
    }

    m_list->updateLayout();
    updatePagination();
}
