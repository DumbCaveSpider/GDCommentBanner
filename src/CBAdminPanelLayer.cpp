#include "CBAdminPanelLayer.hpp"
#include <Geode/utils/web.hpp>
#include <argon/argon.hpp>
#include "Geode/ui/Layout.hpp"
#include "include/CBConstant.hpp"
#include <Geode/ui/Button.hpp>
#include <Geode/ui/LazySprite.hpp>

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

    this->setTitle("Admin Panel");

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

    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;

    Ref<CBAdminPanelLayer> retainedSelf = this;
    arc::spawn([retainedSelf, accountId, accountData]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                FLAlertLayer::create("Error", "Authentication failed.", "OK")->show();
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
                FLAlertLayer::create("Error", "Failed to fetch pending banners.", "OK")->show();
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
                auto emptyLabel = CCLabelBMFont::create("No pending banners.", "chatFont.fnt");
                emptyLabel->setScale(0.8f);
                emptyLabel->setID("empty-label");
                retainedSelf->m_list->addChildAtPosition(emptyLabel, Anchor::Center);
                return;
            }

            for (size_t i = 0; i < json.size(); ++i) {
                auto item = json[i];
                auto cell = CCLayer::create();
                cell->setContentSize({380.f, 90.f});

                auto bg = CCScale9Sprite::create("square02_001.png");
                bg->setContentSize(cell->getContentSize());
                bg->setAnchorPoint({0, 0});
                bg->setOpacity(50);
                cell->addChild(bg);

                std::string name = item["name"].asString().unwrapOr("Unknown");
                std::string user = item["username"].asString().unwrapOr("Unknown");
                int id = item["id"].asInt().unwrapOr(0);
                std::string desc = item["description"].asString().unwrapOr("");
                int price = item["price"].asInt().unwrapOr(0);
                bool isLimited = item["isLimited"].asBool().unwrapOr(false);
                std::string imageUrl = item["imageUrl"].asString().unwrapOr("");
                if (!imageUrl.empty()) {
                    imageUrl = fmt::format("{}{}", comment::baseUrl, imageUrl);
                }

                auto sprite = LazySprite::create({304.f, 84.f}, true);
                sprite->setAutoResize(true);
                sprite->setPosition({380.f / 2.f, 60.f});
                if (!imageUrl.empty()) {
                    sprite->loadFromUrl(imageUrl, LazySprite::Format::kFmtWebp, false);
                }
                cell->addChild(sprite);

                auto label = CCLabelBMFont::create(fmt::format("{} by {}", name, user).c_str(), "bigFont.fnt");
                label->limitLabelWidth(150.f, 0.7f, 0.3f);
                label->setAnchorPoint({0, 0.5f});
                label->setPosition({10.f, 25.f});
                cell->addChild(label);

                float currentX = label->getPositionX() + (label->getContentSize().width * label->getScale()) + 5.f;

                if (isLimited) {
                    auto limitedIcon = CCSprite::createWithSpriteFrameName("GJ_sRecentIcon_001.png");
                    limitedIcon->setScale(0.6f);
                    limitedIcon->setAnchorPoint({0, 0.5f});
                    limitedIcon->setPosition({currentX, 25.f});
                    cell->addChild(limitedIcon);
                    currentX += (limitedIcon->getContentSize().width * limitedIcon->getScale()) + 5.f;
                }

                auto priceLabel = CCLabelBMFont::create(fmt::format("{}", price).c_str(), "bigFont.fnt");
                priceLabel->limitLabelWidth(50.f, 0.4f, 0.2f);
                priceLabel->setAnchorPoint({0, 0.5f});
                priceLabel->setPosition({currentX, 25.f});
                cell->addChild(priceLabel);
                currentX += (priceLabel->getContentSize().width * priceLabel->getScale()) + 2.f;

                if (auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr)) {
                    amethystIcon->setScale(0.45f);
                    amethystIcon->setAnchorPoint({0, 0.5f});
                    amethystIcon->setPosition({currentX, 25.f});
                    cell->addChild(amethystIcon);
                }

                auto descLabel = SimpleTextArea::create(desc.c_str(), "chatFont.fnt", 0.4f);
                descLabel->setAnchorPoint({0, 0.5f});
                descLabel->setPosition({10.f, 10.f});
                cell->addChild(descLabel);

                auto menu = CCMenu::create();
                menu->setContentSize({100.f, 30.f});
                menu->setPosition({320.f, 20.f});
                menu->setLayout(RowLayout::create()
                        ->setAxisAlignment(AxisAlignment::Center)
                        ->setGap(10.f));

                auto approveBtn = geode::Button::createWithNode(
                    CCSprite::createWithSpriteFrameName("GJ_completesIcon_001.png"),
                    [retainedSelf, id, accountId, authToken](geode::Button*) {
                        arc::spawn([retainedSelf, id, accountId, authToken]() -> arc::Future<> {
                            auto req = geode::utils::web::WebRequest();
                            req.header("Content-Type", "application/x-www-form-urlencoded");
                            std::string body = fmt::format("accountId={}&argonToken={}&id={}", accountId, authToken, id);
                            auto res = co_await req.bodyString(body).post(fmt::format("{}/approveBanner", comment::baseUrl));
                            geode::queueInMainThread([retainedSelf] { retainedSelf->fetchPendingBanners(); });
                            co_return;
                        });
                    });
                approveBtn->setPosition({-35.f, 0.f});
                menu->addChild(approveBtn);

                auto rejectBtn = geode::Button::createWithNode(
                    CCSprite::createWithSpriteFrameName("GJ_deleteIcon_001.png"),
                    [retainedSelf, id, accountId, authToken](geode::Button*) {
                        arc::spawn([retainedSelf, id, accountId, authToken]() -> arc::Future<> {
                            auto req = geode::utils::web::WebRequest();
                            req.header("Content-Type", "application/x-www-form-urlencoded");
                            std::string body = fmt::format("accountId={}&argonToken={}&id={}", accountId, authToken, id);
                            auto res = co_await req.bodyString(body).post(fmt::format("{}/rejectBanner", comment::baseUrl));
                            geode::queueInMainThread([retainedSelf] { retainedSelf->fetchPendingBanners(); });
                            co_return;
                        });
                    });
                rejectBtn->setPosition({35.f, 0.f});
                menu->addChild(rejectBtn);

                cell->addChild(menu);

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
    m_list->addChild(m_loadingCircle);
    m_loadingCircle->fadeIn();

    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;

    Ref<CBAdminPanelLayer> retainedSelf = this;
    arc::spawn([retainedSelf, accountId, accountData]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                FLAlertLayer::create("Error", "Authentication failed.", "OK")->show();
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
                FLAlertLayer::create("Error", "Failed to fetch users.", "OK")->show();
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

                auto bg = CCScale9Sprite::create("square02_001.png");
                bg->setContentSize(cell->getContentSize());
                bg->setAnchorPoint({0, 0});
                bg->setOpacity(50);
                cell->addChild(bg);

                std::string user = item["username"].asString().unwrapOr("Unknown");
                int targetId = item["accountId"].asInt().unwrapOr(0);

                auto label = CCLabelBMFont::create(fmt::format("{} (ID: {})", user, targetId).c_str(), "chatFont.fnt");
                label->setScale(0.6f);
                label->setAnchorPoint({0, 0.5f});
                label->setPosition({10.f, 20.f});
                cell->addChild(label);

                retainedSelf->m_list->setCellColor(ccColor4B{0, 0, 0, 0});
                retainedSelf->m_list->addCell(cell);
            }
            retainedSelf->m_list->updateLayout();
        });

        co_return;
    });
}
