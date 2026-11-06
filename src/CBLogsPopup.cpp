#include "CBLogsPopup.hpp"
#include "CBLogsCell.hpp"
#include <Geode/utils/web.hpp>
#include <dankmeme.argon/argon.hpp>
#include "Geode/ui/Notification.hpp"
#include "include/CBConstant.hpp"
#include <Geode/ui/Scrollbar.hpp>

using namespace geode::prelude;

CBLogsPopup* CBLogsPopup::create() {
    auto ret = new CBLogsPopup();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBLogsPopup::init() {
    if (!Popup::init(380.f, 280.f)) return false;

    this->setTitle("Notifications & Logs");
    addSideArt(m_mainLayer, SideArt::TopLeft, SideArtStyle::PopupBlue, false);
    addSideArt(m_mainLayer, SideArt::Bottom, SideArtStyle::PopupBlue, false);

    // a bit shorter than before to leave room for the page label underneath
    m_list = cue::ListNode::create({340.f, 200.f}, {0, 0, 0, 0}, cue::ListBorderStyle::Comments);
    if (m_list) {
        m_mainLayer->addChildAtPosition(m_list, Anchor::Center, {0.f, -5.f}, false);

        auto scrollbar = Scrollbar::create(m_list->getScrollLayer());
        m_mainLayer->addChildAtPosition(scrollbar, Anchor::Center, {340.f / 2 + 5.f, -5.f}, false);
    }

    // filter, same button as the shop
    m_filterBtn = Button::createWithNode(
        EditorButtonSprite::createWithSpriteFrameName("GJ_filterIcon_001.png", 1.0f, EditorBaseColor::Gray),
        [this](Button*) {
            Ref<CBLogsPopup> self = this;
            if (auto popup = CBLogsFilterPopup::create(m_filter, [self](CBLogsFilter const& filter) {
                    self->setFilter(filter);
                })) {
                popup->show();
            }
        });
    m_filterBtn->setScale(0.8f);
    m_mainLayer->addChildAtPosition(m_filterBtn, Anchor::TopRight, {-20.f, -20.f}, false);

    // pagination
    m_prevBtn = Button::createWithSpriteFrameName("GJ_arrow_01_001.png", [this](Button*) { this->onPrevPage(); });
    m_mainLayer->addChildAtPosition(m_prevBtn, Anchor::Left, {-20.f, -5.f}, false);

    m_nextBtn = Button::createWithSpriteFrameName("GJ_arrow_01_001.png", [this](Button*) { this->onNextPage(); });
    static_cast<CCSprite*>(m_nextBtn->getDisplayNode())->setFlipX(true);
    m_mainLayer->addChildAtPosition(m_nextBtn, Anchor::Right, {20.f, -5.f}, false);

    m_pageLabel = Label::create("", "goldFont.fnt");
    m_mainLayer->addChildAtPosition(m_pageLabel, Anchor::Bottom, {0.f, 15.f}, false);

    updatePagination();
    fetchLogs();

    return true;
}

void CBLogsPopup::setFilter(CBLogsFilter const& filter) {
    m_filter = filter;
    m_page = 0;
    updateFilterButton();
    fetchLogs();
}

void CBLogsPopup::updateFilterButton() {
    m_filterBtn->setDisplayNode(EditorButtonSprite::createWithSpriteFrameName(
        "GJ_filterIcon_001.png", 1.0f, m_filter.isDefault() ? EditorBaseColor::Gray : EditorBaseColor::Cyan));
}

void CBLogsPopup::updatePagination() {
    int pageCount = std::max(1, (m_total + kLogsPerPage - 1) / kLogsPerPage);
    m_prevBtn->setVisible(m_page > 0);
    m_nextBtn->setVisible(m_page + 1 < pageCount);

    if (m_total == 0) {
        m_pageLabel->setVisible(false);
        return;
    }
    int first = m_page * kLogsPerPage + 1;
    int last = std::min(first + kLogsPerPage - 1, m_total);
    m_pageLabel->setVisible(true);
    m_pageLabel->setString(fmt::format("{} to {} of {}", first, last, m_total).c_str());
    m_pageLabel->setLimitLabelWidth(120.f, 0.4f, 0.1f);
}

void CBLogsPopup::onPrevPage() {
    if (m_page <= 0) return;
    m_page--;
    fetchLogs();
}

void CBLogsPopup::onNextPage() {
    if ((m_page + 1) * kLogsPerPage >= m_total) return;
    m_page++;
    fetchLogs();
}

void CBLogsPopup::fetchLogs() {
    m_list->clear();
    if (m_loadingCircle) m_loadingCircle->removeFromParent();
    m_loadingCircle = cue::LoadingCircle::create(true);
    m_list->addChildAtPosition(m_loadingCircle, Anchor::Center, CCPointZero, false);
    m_loadingCircle->fadeIn();

    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;

    if (accountId <= 0) {
        m_loadingCircle->fadeOut();
        Notification::create("Invalid account ID", NotificationIcon::Error)->show();
        return;
    }

    // ?types=A,B&order=oldest&page=0&limit=10
    std::string query = fmt::format("&page={}&limit={}", m_page, kLogsPerPage);
    auto types = getLogTypesFor(m_filter.categories);
    if (!types.empty()) {
        query += "&types=" + utils::string::join(types, ",");
    }
    if (m_filter.oldestFirst) {
        query += "&order=oldest";
    }

    int fetchId = ++m_fetchId;
    Ref<CBLogsPopup> retainedSelf = this;
    arc::spawn([retainedSelf, accountId, accountData, query, fetchId]() -> arc::Future<> {
        auto isStale = [retainedSelf, fetchId] { return retainedSelf->m_fetchId != fetchId; };
        auto fail = [retainedSelf, isStale](char const* message) {
            queueInMainThread([retainedSelf, isStale, message] {
                if (isStale()) return;
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create(message, NotificationIcon::Error)->show();
            });
        };

        auto authToken = co_await comment::argonToken(accountData);
        if (authToken.empty()) {
            fail("Authentication failed.");
            co_return;
        }

        auto url = fmt::format("{}/getUserLogs?accountId={}&argonToken={}{}", comment::baseUrl, accountId, authToken, query);
        auto response = co_await utils::web::WebRequest().get(url);
        if (!response.ok()) {
            fail("Failed to fetch logs.");
            co_return;
        }

        auto jsonRes = response.json();
        if (jsonRes.isErr() || !jsonRes.unwrap()["logs"].isArray()) {
            fail("Invalid JSON from server.");
            co_return;
        }

        queueInMainThread([retainedSelf, isStale, json = jsonRes.unwrap()] {
            if (isStale()) return;
            retainedSelf->m_loadingCircle->fadeOut();
            retainedSelf->m_list->clear();

            auto const& logs = json["logs"];
            retainedSelf->m_total = static_cast<int>(json["total"].asInt().unwrapOr(0));
            retainedSelf->updatePagination();

            if (logs.size() == 0) {
                auto emptyLabel = Label::create(
                    retainedSelf->m_filter.categories.empty() ? "No notifications yet." : "No notifications match this filter.",
                    "goldFont.fnt");
                emptyLabel->setLimitLabelWidth(retainedSelf->m_list->getContentWidth() - 20.f, 0.7f, 0.15f);
                retainedSelf->m_list->addChildAtPosition(emptyLabel, Anchor::Center);
                return;
            }

            retainedSelf->m_list->setCellColor(ccColor4B{0, 0, 0, 0});
            float cellWidth = retainedSelf->m_list->getContentWidth();

            for (size_t i = 0; i < logs.size(); ++i) {
                auto const& item = logs[i];
                CBLogEntry entry{
                    .id = static_cast<int>(item["id"].asInt().unwrapOr(0)),
                    .actionType = item["actionType"].asString().unwrapOr(""),
                    .message = item["message"].asString().unwrapOr("Unknown message"),
                    .createdAt = item["createdAt"].asString().unwrapOr(""),
                };

                if (auto cell = CBLogsCell::create(entry, cellWidth)) {
                    retainedSelf->m_list->addCell(cell);
                }
            }
            retainedSelf->m_list->updateLayout();
            retainedSelf->m_list->scrollToTop();
        });

        co_return;
    });
}
