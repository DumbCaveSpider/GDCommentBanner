#include "CBProfileBannerPopup.hpp"
#include "CBBannerCell.hpp"
#include <argon/argon.hpp>
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

    std::string titleStr = targetUsername.empty() ? "User Banners" : fmt::format("{}'s Banners", targetUsername);
    this->setTitle(titleStr, "goldFont.fnt");

    // Make title clickable to open ProfilePage
    if (m_title) {
        m_title->setVisible(false);  // Hide the default title, we will replace it with a button
    }

    auto titleBtn = geode::Button::createWithLabel(titleStr.c_str(), "goldFont.fnt", [this](geode::Button* sender) {
        ProfilePage::create(m_targetAccountId, false)->show();
    });
    m_buttonMenu->addChild(titleBtn);
    titleBtn->setPosition(m_title->getPosition());
    titleBtn->setScale(m_title->getScale());

    m_list = cue::ListNode::create({340.f, 220.f}, {0, 0, 0, 0}, cue::ListBorderStyle::Comments);
    if (m_list) {
        m_mainLayer->addChildAtPosition(m_list, Anchor::Center, {0.f, -5.f}, false);

        auto scrollbar = Scrollbar::create(m_list->getScrollLayer());
        m_mainLayer->addChildAtPosition(scrollbar, Anchor::Center, {340.f / 2 + 5.f, -5.f}, false);
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

    m_prevBtn = CCMenuItemSpriteExtra::create(
        CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png"),
        this,
        menu_selector(CBProfileBannerPopup::onPrevPage));
    m_prevBtn->setPosition({-210.f, 110.f});
    navMenu->addChild(m_prevBtn);

    auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    nextSpr->setFlipX(true);
    m_nextBtn = CCMenuItemSpriteExtra::create(
        nextSpr,
        this,
        menu_selector(CBProfileBannerPopup::onNextPage));
    m_nextBtn->setPosition({210.f, 110.f});
    navMenu->addChild(m_nextBtn);

    m_pageLabel = CCLabelBMFont::create("0 to 0 of 0", "goldFont.fnt");
    m_pageLabel->limitLabelWidth(100.f, 0.4f, 0.1f);
    m_mainLayer->addChildAtPosition(m_pageLabel, Anchor::Bottom, {0, 15}, false);

    m_mainLayer->addChild(navMenu);

    updatePaginationButtons();
    fetchBanners();

    return true;
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

        auto req = geode::utils::web::WebRequest();
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
                Notification::create("Invalid JSON from server.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isObject()) {
            geode::queueInMainThread([retainedSelf] {
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
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        geode::queueInMainThread([retainedSelf, bannersArr = std::move(bannersArr), totalItems] {
            retainedSelf->m_loadingCircle->fadeOut();
            retainedSelf->m_list->clear();
            retainedSelf->m_totalItems = totalItems;
            retainedSelf->updatePaginationButtons();

            if (bannersArr.size() == 0) {
                auto emptyLabel = CCLabelBMFont::create("No Banners", "goldFont.fnt");
                emptyLabel->limitLabelWidth(retainedSelf->m_list->getContentWidth() - 20.f, 0.7f, 0.15f);
                retainedSelf->m_list->addChildAtPosition(emptyLabel, Anchor::Center);
                return;
            }

            for (size_t i = 0; i < bannersArr.size(); ++i) {
                auto item = bannersArr[i];
                CBBannerItem banner;
                if (auto urlRes = item["imageUrl"].asString(); urlRes.isOk()) {
                    banner.imageUrl = urlRes.unwrap();
                }
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

                if (auto cell = CBBannerCell::create(banner, 340.f)) {
                    retainedSelf->m_list->addCell(cell);
                }
            }
            retainedSelf->m_list->getScrollLayer()->scrollToTop();
        });
    });
}
