#include "CBProfileBannerPopup.hpp"
#include "CBBannerCell.hpp"
#include "CBUserCell.hpp"
#include "CBGiftAmethyst.hpp"
#include <dankmeme.argon/argon.hpp>
#include "include/CBConstant.hpp"
#include <Geode/ui/Scrollbar.hpp>
#include <Geode/ui/Button.hpp>

using namespace geode::prelude;

CBProfileBannerPopup* CBProfileBannerPopup::create(int targetAccountId, const std::string& targetUsername) {
    auto ret = new CBProfileBannerPopup();
    if (ret && ret->init(targetAccountId, targetUsername)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBProfileBannerPopup::init(int targetAccountId, const std::string& targetUsername) {
    if (!Popup::init(380.f, 280.f, "GJ_square01.png")) {
        return false;
    }
    m_targetAccountId = targetAccountId;
    m_targetUsername = targetUsername;

    std::string titleStr = targetUsername.empty() ? "Player" : targetUsername;
    this->setTitle(titleStr, "bigFont.fnt");

    auto profileBtn = Button::createWithSpriteFrameName("GJ_profileButton_001.png", [this](Button* sender) {
        ProfilePage::create(m_targetAccountId, false)->show();
    });
    profileBtn->setScale(0.7f);
    m_buttonMenu->addChildAtPosition(profileBtn, Anchor::TopRight, {-3.f, -3.f});

    // gift amethysts to this user (not on your own profile)
    if (targetAccountId != argon::getGameAccountData().accountId) {
        auto giftBtn = Button::createWithNode(
            CircleButtonSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr, 1.f, CircleBaseColor::DarkPurple, CircleBaseSize::Small),
            [this](Button*) {
                if (auto popup = CBGiftAmethyst::create(m_targetAccountId)) {
                    popup->show();
                }
            });
        m_buttonMenu->addChildAtPosition(giftBtn, Anchor::TopRight, {-43.f, -3.f});
    }

    // 10px shorter and lower than before, the stats row sits above it
    m_list = cue::ListNode::create({340.f, 200.f}, {0, 0, 0, 0}, cue::ListBorderStyle::Comments);
    if (m_list) {
        m_mainLayer->addChildAtPosition(m_list, Anchor::Center, {0.f, -15.f}, false);

        // auto scrollbar = Scrollbar::create(m_list->getScrollLayer());
        // m_mainLayer->addChildAtPosition(scrollbar, Anchor::Center, {340.f / 2 + 5.f, -10.f}, false);
    }

    auto listBg = NineSlice::create("square02_001.png");
    if (listBg) {
        listBg->setPosition(m_list->getPosition());
        listBg->setContentSize(m_list->getContentSize() + CCSize(5.f, 10.f));
        listBg->setOpacity(100);
        m_mainLayer->addChild(listBg, -1);
    }

    // Pagination buttons
    auto navMenu = CCMenu::create();
    navMenu->setPosition({m_mainLayer->getContentSize().width / 2, 20.f});

    m_prevBtn = Button::createWithSpriteFrameName("GJ_arrow_01_001.png", [this](Button* sender) { this->onPrevPage(sender); });
    m_prevBtn->setPosition({-210.f, 110.f});
    navMenu->addChild(m_prevBtn);

    m_nextBtn = Button::createWithSpriteFrameName("GJ_arrow_01_001.png", [this](Button* sender) { this->onNextPage(sender); });
    static_cast<CCSprite*>(m_nextBtn->getDisplayNode())->setFlipX(true);
    m_nextBtn->setPosition({210.f, 110.f});
    navMenu->addChild(m_nextBtn);

    m_pageLabel = Label::create("0 to 0 of 0", "goldFont.fnt");
    m_pageLabel->setLimitLabelWidth(100.f, 0.4f, 0.1f);
    m_mainLayer->addChildAtPosition(m_pageLabel, Anchor::Bottom, {0, 15}, false);

    m_mainLayer->addChild(navMenu);

    updatePaginationButtons();
    fetchBanners();
    fetchStats();

    return true;
}

void CBProfileBannerPopup::fetchStats() {
    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;
    // /getUser needs a logged in account
    if (accountId <= 0) return;

    Ref<CBProfileBannerPopup> retainedSelf = this;
    int targetAccountId = m_targetAccountId;
    arc::spawn([retainedSelf, accountId, accountData, targetAccountId]() -> arc::Future<> {
        auto authToken = co_await comment::argonToken(accountData);
        if (authToken.empty()) co_return;

        auto body = matjson::makeObject({{"accountId", accountId},
            {"argonToken", authToken},
            {"targetAccountId", targetAccountId}});
        auto response = co_await utils::web::WebRequest().bodyJSON(body).post(fmt::format("{}/getUser", comment::baseUrl));
        if (!response.ok()) co_return;

        auto json = response.json();
        if (json.isErr()) co_return;

        queueInMainThread([retainedSelf, user = json.unwrap()] {
            if (retainedSelf->getParent()) {
                retainedSelf->showStats(user);
            }
        });
        co_return;
    });
}

void CBProfileBannerPopup::showStats(matjson::Value const& user) {
    if (m_statsNode) m_statsNode->removeFromParent();
    if (m_badgesNode) m_badgesNode->removeFromParent();
    m_statsNode = nullptr;
    m_badgesNode = nullptr;

    auto data = CBUserData::fromJson(user);
    data.monthlyEarnings = static_cast<int>(user["monthly_earnings"].asInt().unwrapOr(0));

    m_statsNode = CBUserCell::createStatsNode(data);
    m_statsNode->setAnchorPoint({0.5f, 0.5f});
    m_statsNode->setScale(1.2f);
    m_mainLayer->addChildAtPosition(m_statsNode, Anchor::Top, {0.f, -45.f}, false);

    if (m_title) {
        if (m_playerNode) m_playerNode->removeFromParent();
        auto player = CBUserCell::createPlayer(data);
        player->setScale(0.7f);
        float halfCube = player->getScaledContentWidth() / 2.f;
        player->setPosition(m_title->getPosition() - CCPoint{m_title->getScaledContentWidth() / 2.f + 4.f + halfCube, 0.f});
        m_mainLayer->addChild(player, m_title->getZOrder());
        m_playerNode = player;

        if (auto badges = CBUserCell::createBadges(data, m_title->getScaledContentHeight())) {
            m_badgesNode = badges;
            badges->setPosition(m_title->getPosition() + CCPoint{m_title->getScaledContentWidth() / 2.f + 4.f, 0.f});
            m_mainLayer->addChild(badges, m_title->getZOrder());
        }
    }
}

void CBProfileBannerPopup::onNextPage(cocos2d::CCObject* sender) {
    m_currentPage++;
    fetchBanners();
}

void CBProfileBannerPopup::onPrevPage(cocos2d::CCObject* sender) {
    if (m_currentPage > 0) {
        m_currentPage--;
        fetchBanners();
    }
}

void CBProfileBannerPopup::updatePaginationButtons() {
    if (m_prevBtn) {
        m_prevBtn->setVisible(m_currentPage > 0);
    }
    if (m_nextBtn) {
        int maxPage = m_totalItems > 0 ? (m_totalItems - 1) / m_itemsPerPage : 0;
        m_nextBtn->setVisible(m_currentPage < maxPage);
    }
    if (m_pageLabel) {
        if (m_totalItems == 0) {
            m_pageLabel->setVisible(false);
        } else {
            m_pageLabel->setVisible(true);
            int startItem = (m_currentPage * m_itemsPerPage) + 1;
            int endItem = std::min(startItem + m_itemsPerPage - 1, m_totalItems);
            m_pageLabel->setString(fmt::format("{} to {} of {}", startItem, endItem, m_totalItems).c_str());
        }
    }
}

void CBProfileBannerPopup::fetchBanners() {
    if (m_loadingCircle) m_loadingCircle->removeFromParent();
    m_loadingCircle = cue::LoadingCircle::create(true);
    m_list->addChildAtPosition(m_loadingCircle, Anchor::Center, CCPointZero, false);
    m_loadingCircle->fadeIn();

    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;

    Ref<CBProfileBannerPopup> retainedSelf = this;
    arc::spawn([retainedSelf, accountId, accountData]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);

        auto req = utils::web::WebRequest();
        auto body = matjson::makeObject({
            {"targetAccountId", retainedSelf->m_targetAccountId},
            {"page", retainedSelf->m_currentPage},
            {"limit", retainedSelf->m_itemsPerPage},
            {"sort", 0}  // Recent
        });

        // Optional auth if account is valid
        if (accountId > 0 && !authResult.empty()) {
            body["accountId"] = accountId;
            body["argonToken"] = authResult;
        }

        auto response = co_await req.bodyJSON(body).post(fmt::format("{}/getBanners", comment::baseUrl));
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
                Notification::create("Invalid JSON from server.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isObject()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        int totalItems = 0;
        if (auto totalRes = json["total"].asInt(); totalRes.isOk()) {
            totalItems = static_cast<int>(totalRes.unwrap());
        }

        auto bannersArr = json["banners"];
        if (!bannersArr.isArray()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        queueInMainThread([retainedSelf, bannersArr = std::move(bannersArr), totalItems] {
            retainedSelf->m_loadingCircle->fadeOut();
            retainedSelf->m_list->clear();
            retainedSelf->m_totalItems = totalItems;
            retainedSelf->updatePaginationButtons();

            if (bannersArr.size() == 0) {
                auto emptyLabel = Label::create("No Banners", "goldFont.fnt");
                emptyLabel->setLimitLabelWidth(retainedSelf->m_list->getContentWidth() - 20.f, 0.7f, 0.15f);
                retainedSelf->m_list->addChildAtPosition(emptyLabel, Anchor::Center);
                return;
            }

            for (size_t i = 0; i < bannersArr.size(); ++i) {
                auto item = bannersArr[i];
                CBBannerItem banner;
                if (auto urlRes = item["imageUrl"].asString(); urlRes.isOk()) {
                    banner.imageUrl = urlRes.unwrap();
                }
                banner.nonCompactImageUrl = item["nonCompactImageUrl"].asString().unwrapOr("");
                banner.isSupporterOnly = item["isSupporterOnly"].asBool().unwrapOr(false);
                banner.createdAt = item["createdAt"].asString().unwrapOr("");
                banner.editedAt = item["editedAt"].asString().unwrapOr("");
                banner.approvedAt = item["approvedAt"].asString().unwrapOr("");
                banner.isFavorite = item["isFavorite"].asBool().unwrapOr(false);
                if (auto usernameRes = item["username"].asString(); usernameRes.isOk()) {
                    banner.username = usernameRes.unwrap();
                }
                if (auto nameRes = item["name"].asString(); nameRes.isOk()) {
                    banner.name = nameRes.unwrap();
                }
                if (auto descriptionRes = item["description"].asString(); descriptionRes.isOk()) {
                    banner.description = descriptionRes.unwrap();
                }
                if (auto priceRes = item["price"].asInt(); priceRes.isOk()) {
                    banner.price = static_cast<int>(priceRes.unwrap());
                }
                if (auto idRes = item["id"].asInt(); idRes.isOk()) {
                    banner.id = static_cast<int>(idRes.unwrap());
                }
                if (auto accountIdRes = item["accountId"].asInt(); accountIdRes.isOk()) {
                    banner.accountId = static_cast<int>(accountIdRes.unwrap());
                }
                if (auto ownsRes = item["owns"].asBool(); ownsRes.isOk()) {
                    banner.owns = ownsRes.unwrap();
                }
                if (auto isLimitedRes = item["isLimited"].asBool(); isLimitedRes.isOk()) {
                    banner.isLimited = isLimitedRes.unwrap();
                }
                if (auto isFeaturedRes = item["isFeatured"].asBool(); isFeaturedRes.isOk()) {
                    banner.isFeatured = isFeaturedRes.unwrap();
                }
                if (auto amountRes = item["amount"].asInt(); amountRes.isOk()) {
                    banner.amount = static_cast<int>(amountRes.unwrap());
                }
                if (auto totalBoughtRes = item["totalBought"].asInt(); totalBoughtRes.isOk()) {
                    banner.totalBought = static_cast<int>(totalBoughtRes.unwrap());
                }
                if (auto equippedCountRes = item["equippedCount"].asInt(); equippedCountRes.isOk()) {
                    banner.equippedCount = static_cast<int>(equippedCountRes.unwrap());
                }

                banner.equipped = (Mod::get()->getSavedValue<int>("equipped-banner", -1) == banner.id);

                if (auto cell = CBBannerCell::create(banner, 340.f, true)) {
                    retainedSelf->m_list->addCell(cell);
                }
            }
            retainedSelf->m_list->getScrollLayer()->scrollToTop();
        });
    });
}
