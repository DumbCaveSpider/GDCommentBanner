#include "CBAdminPanelLayer.hpp"
#include <Geode/utils/web.hpp>
#include <argon/argon.hpp>
#include "Geode/ui/BasedButtonSprite.hpp"
#include "Geode/ui/General.hpp"
#include "Geode/ui/Layout.hpp"
#include "include/CBConstant.hpp"
#include <Geode/ui/Button.hpp>
#include <Geode/ui/LazySprite.hpp>
#include <Geode/binding/UploadActionPopup.hpp>
#include "CBManageUserPopup.hpp"

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

    m_mainLayer->addChildAtPosition(tabMenu, Anchor::Top, {0.f, -50.f}, false);
    tabMenu->updateLayout();

    fetchPendingBanners();

    return true;
}

void CBAdminPanelLayer::onTabPending(CCObject*) {
    if (m_currentTab == Tab::Pending) return;
    m_currentTab = Tab::Pending;
    fetchPendingBanners();
}

void CBAdminPanelLayer::onTabUsers(CCObject*) {
    if (m_currentTab == Tab::Users) return;
    m_currentTab = Tab::Users;
    fetchUsers();
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
            retainedSelf->m_list->clear();

            if (auto empty = retainedSelf->m_list->getChildByID("empty-label")) {
                empty->removeFromParent();
            }

            if (json.size() == 0) {
                auto emptyLabel = CCLabelBMFont::create("No pending banners.", "goldFont.fnt");
                emptyLabel->setScale(0.8f);
                emptyLabel->setID("empty-label");
                retainedSelf->m_list->addChildAtPosition(emptyLabel, Anchor::Center);
                return;
            }

            for (size_t i = 0; i < json.size(); ++i) {
                auto item = json[i];
                std::string name = item["name"].asString().unwrapOr("Unknown");
                std::string user = item["username"].asString().unwrapOr("Unknown");
                int id = item["id"].asInt().unwrapOr(0);
                std::string desc = item["description"].asString().unwrapOr("");
                int price = item["price"].asInt().unwrapOr(0);
                bool isLimited = item["isLimited"].asBool().unwrapOr(false);
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
                if (isLimited) {
                    if (auto starIcon = CCSprite::createWithSpriteFrameName("GJ_sRecentIcon_001.png")) {
                        starIcon->setScale(0.8f);
                        starIcon->setPosition({nameX, 25.f});
                        starIcon->setAnchorPoint({0.f, 0.5f});
                        cell->addChild(starIcon);
                        nameLabel->runAction(CCRepeatForever::create(CCSequence::create(
                            CCTintTo::create(1.f, 255, 150, 255),
                            CCTintTo::create(1.f, 255, 255, 255),
                            nullptr)));
                        nameX += starIcon->getContentSize().width * starIcon->getScale() + 4.f;
                    }
                }
                nameLabel->setPosition({nameX, 25.f});
                nameLabel->limitLabelWidth(100.f, 0.5f, 0.2f);
                cell->addChild(nameLabel);

                float currentX = nameX + (nameLabel->getContentSize().width * nameLabel->getScale()) + 8.f;

                // User
                if (!user.empty()) {
                    auto usernameLabel = CCLabelBMFont::create(fmt::format("By {}", user).c_str(), "goldFont.fnt");
                    usernameLabel->setAnchorPoint({0.f, 0.5f});
                    usernameLabel->setPosition({currentX, 25.f});
                    usernameLabel->setScale(0.4f);
                    cell->addChild(usernameLabel);
                    currentX += (usernameLabel->getContentSize().width * usernameLabel->getScale()) + 15.f;
                } else {
                    currentX += 15.f;
                }

                // Price
                if (auto priceLabel = CCLabelBMFont::create(fmt::format("{}", price).c_str(), "bigFont.fnt")) {
                    priceLabel->setAnchorPoint({0.f, 0.5f});
                    priceLabel->setScale(0.5f);
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
                        amountLabel->setPosition({currentX, 25.f});
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
                    [retainedSelf, id, accountId, authToken](geode::Button*) {
                        Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Approving banner...");
                        popup->show();
                        arc::spawn([retainedSelf, id, accountId, authToken, popup]() -> arc::Future<> {
                            auto req = geode::utils::web::WebRequest();
                            req.header("Content-Type", "application/x-www-form-urlencoded");
                            std::string body = fmt::format("accountId={}&argonToken={}&id={}", accountId, authToken, id);
                            auto res = co_await req.bodyString(body).post(fmt::format("{}/approveBanner", comment::baseUrl));
                            bool ok = res.ok();
                            geode::queueInMainThread([retainedSelf, popup, ok] {
                                if (ok) {
                                    popup->showSuccessMessage("Banner approved!");
                                    retainedSelf->fetchPendingBanners();
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
                    [retainedSelf, id, accountId, authToken](geode::Button*) {
                        Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Rejecting banner...");
                        popup->show();
                        arc::spawn([retainedSelf, id, accountId, authToken, popup]() -> arc::Future<> {
                            auto req = geode::utils::web::WebRequest();
                            req.header("Content-Type", "application/x-www-form-urlencoded");
                            std::string body = fmt::format("accountId={}&argonToken={}&id={}", accountId, authToken, id);
                            auto res = co_await req.bodyString(body).post(fmt::format("{}/rejectBanner", comment::baseUrl));
                            bool ok = res.ok();
                            geode::queueInMainThread([retainedSelf, popup, ok] {
                                if (ok) {
                                    popup->showSuccessMessage("Banner rejected!");
                                    retainedSelf->fetchPendingBanners();
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

                retainedSelf->m_list->setCellColor(ccColor4B{0, 0, 0, 0});
                retainedSelf->m_list->addCell(cell);
            }
            retainedSelf->m_list->updateLayout();
        });

        co_return;
    });
}

void CBAdminPanelLayer::fetchUsers() {
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
        auto url = fmt::format("{}/admin/usersWithBanners?accountId={}&argonToken={}", comment::baseUrl, accountId, authToken);

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
            retainedSelf->m_list->clear();

            for (size_t i = 0; i < json.size(); ++i) {
                auto item = json[i];
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

                retainedSelf->m_list->setCellColor(ccColor4B{0, 0, 0, 0});
                retainedSelf->m_list->addCell(cell);
            }
            retainedSelf->m_list->updateLayout();
        });

        co_return;
    });
}
