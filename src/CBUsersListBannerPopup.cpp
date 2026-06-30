#include "CBUsersListBannerPopup.hpp"
#include "CBProfileBannerPopup.hpp"
#include <argon/argon.hpp>
#include "include/CBConstant.hpp"
#include <Geode/ui/Scrollbar.hpp>
#include <Geode/ui/Button.hpp>

using namespace geode::prelude;

CBUsersListBannerPopup* CBUsersListBannerPopup::create(int bannerId, int listType) {
    auto ret = new CBUsersListBannerPopup();
    if (ret && ret->init(bannerId, listType)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBUsersListBannerPopup::init(int bannerId, int listType) {
    if (!Popup::init(400.f, 260.f)) {
        return false;
    }

    m_bannerId = bannerId;
    m_listType = listType;

    this->setTitle(m_listType == 1 ? "Users Bought" : "Users Equipped");

    m_list = cue::ListNode::create({356.f, 200.f}, {0, 0, 0, 0}, cue::ListBorderStyle::Comments);
    m_list->setPosition(m_size.width / 2.f, m_size.height / 2.f - 5.f);
    m_list->setZOrder(2);
    m_mainLayer->addChild(m_list);

    auto scrollbar = Scrollbar::create(m_list->getScrollLayer());
    scrollbar->setZOrder(5);
    scrollbar->setPosition({m_size.width / 2.f + 356.f / 2.f + 10.f, m_size.height / 2.f - 5.f});
    m_mainLayer->addChild(scrollbar);

    if (auto prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
        m_prevBtn = CCMenuItemSpriteExtra::create(prevSpr, this, menu_selector(CBUsersListBannerPopup::onPrevPage));
        m_buttonMenu->addChildAtPosition(m_prevBtn, Anchor::Left, {-20.f, 0.f});
    }

    if (auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png")) {
        nextSpr->setFlipX(true);
        m_nextBtn = CCMenuItemSpriteExtra::create(nextSpr, this, menu_selector(CBUsersListBannerPopup::onNextPage));
        m_buttonMenu->addChildAtPosition(m_nextBtn, Anchor::Right, {20.f, 0.f});
    }

    m_pageLabel = CCLabelBMFont::create("1 to 100", "goldFont.fnt");
    m_pageLabel->setScale(0.5f);
    m_pageLabel->setZOrder(5);
    m_mainLayer->addChildAtPosition(m_pageLabel, Anchor::Bottom, {0.f, 15.f}, false);

    fetchUsers();

    return true;
}

void CBUsersListBannerPopup::updatePaginationButtons() {
    m_prevBtn->setVisible(m_currentPage > 0);
    int maxPage = (m_totalItems - 1) / m_itemsPerPage;
    m_nextBtn->setVisible(m_currentPage < maxPage);

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

void CBUsersListBannerPopup::onPrevPage(CCObject* sender) {
    if (m_currentPage > 0) {
        m_currentPage--;
        fetchUsers();
    }
}

void CBUsersListBannerPopup::onNextPage(CCObject* sender) {
    int maxPage = (m_totalItems - 1) / m_itemsPerPage;
    if (m_currentPage < maxPage) {
        m_currentPage++;
        fetchUsers();
    }
}

void CBUsersListBannerPopup::fetchUsers() {
    if (m_loadingCircle) m_loadingCircle->removeFromParent();
    m_loadingCircle = cue::LoadingCircle::create(true);
    m_list->addChildAtPosition(m_loadingCircle, Anchor::Center, CCPointZero, false);
    m_loadingCircle->fadeIn();

    Ref<CBUsersListBannerPopup> retainedSelf = this;
    arc::spawn([retainedSelf]() -> arc::Future<> {
        auto req = geode::utils::web::WebRequest();
        auto body = matjson::makeObject({{"bannerId", retainedSelf->m_bannerId},
            {"page", retainedSelf->m_currentPage},
            {"limit", retainedSelf->m_itemsPerPage},
            {"listType", retainedSelf->m_listType}});

        auto response = co_await req.bodyJSON(body).post(fmt::format("{}/getBannerUsers", comment::baseUrl));

        if (!response.ok()) {
            geode::queueInMainThread([retainedSelf] {
                if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Failed to fetch users.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto jsonRes = response.json();
        if (jsonRes.isErr()) {
            geode::queueInMainThread([retainedSelf] {
                if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Invalid JSON from server.", NotificationIcon::Error)->show();
            });
            co_return;
        }

        auto json = jsonRes.unwrap();
        if (!json.isObject()) {
            geode::queueInMainThread([retainedSelf] {
                if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        int totalItems = 0;
        if (auto totalRes = json["total"].asInt(); totalRes.isOk()) {
            totalItems = static_cast<int>(totalRes.unwrap());
        }

        auto usersArr = json["users"];
        if (!usersArr.isArray()) {
            geode::queueInMainThread([retainedSelf] {
                if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        geode::queueInMainThread([retainedSelf, usersArr = std::move(usersArr), totalItems] {
            if (retainedSelf->m_loadingCircle) retainedSelf->m_loadingCircle->fadeOut();
            if (retainedSelf->m_list) retainedSelf->m_list->clear();
            retainedSelf->m_totalItems = totalItems;
            retainedSelf->updatePaginationButtons();

            if (usersArr.size() == 0) {
                auto emptyLabel = CCLabelBMFont::create("No users found", "goldFont.fnt");
                emptyLabel->limitLabelWidth(retainedSelf->m_list->getContentWidth() - 20.f, 0.7f, 0.15f);
                if (retainedSelf->m_list) {
                    retainedSelf->m_list->addChildAtPosition(emptyLabel, Anchor::Center);
                }
                return;
            }

            for (size_t i = 0; i < usersArr.size(); ++i) {
                auto item = usersArr[i];
                int accountId = item["accountId"].asInt().unwrapOr(0);
                std::string username = item["username"].asString().unwrapOr("Unknown");
                std::string equippedBannerUrl = item["equippedBannerUrl"].asString().unwrapOr("");

                auto cell = CCNode::create();
                cell->setContentSize({340.f, 36.f});

                if (!equippedBannerUrl.empty()) {
                    auto bannerSprite = comment::createBannerNode(fmt::format("{}{}", comment::baseUrl, equippedBannerUrl), {cell->getContentWidth() + 18, cell->getContentHeight() + 18});
                    if (bannerSprite) {
                        bannerSprite->setPosition({cell->getContentWidth() / 2 + 8.f, cell->getContentHeight() / 2});
                        cell->addChild(bannerSprite, -2);
                    }
                }

                auto usernameLabel = CCLabelBMFont::create(username.c_str(), "bigFont.fnt");
                usernameLabel->limitLabelWidth(200.f, 0.6f, 0.1f);

                auto usernameBtn = geode::Button::createWithNode(
                    usernameLabel,
                    [accountId, username](geode::Button* sender) {
                        if (auto popup = CBProfileBannerPopup::create(accountId, username)) {
                            popup->show();
                        }
                    });

                usernameBtn->setAnchorPoint({0.f, 0.5f});
                usernameBtn->setPosition({10.f, 18.f});
                cell->addChild(usernameBtn);

                if (retainedSelf->m_list) {
                    retainedSelf->m_list->addCell(cell);
                }
            }

            if (retainedSelf->m_list) {
                retainedSelf->m_list->getScrollLayer()->scrollToTop();
                retainedSelf->m_list->updateLayout();
            }
        });
    });
}
