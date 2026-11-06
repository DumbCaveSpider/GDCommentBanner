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
    addSideArt(m_mainLayer, SideArt::Top, SideArtStyle::PopupBlue, false);

    m_list = cue::ListNode::create({340.f, 220.f}, {0, 0, 0, 0}, cue::ListBorderStyle::Comments);
    if (m_list) {
        m_mainLayer->addChildAtPosition(m_list, Anchor::Center, {0.f, -15.f}, false);

        auto scrollbar = Scrollbar::create(m_list->getScrollLayer());
        m_mainLayer->addChildAtPosition(scrollbar, Anchor::Center, {340.f / 2 + 5.f, -15.f}, false);
    }

    fetchLogs();

    return true;
}

void CBLogsPopup::fetchLogs() {
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

    Ref<CBLogsPopup> retainedSelf = this;
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
        auto url = fmt::format("{}/getUserLogs?accountId={}&argonToken={}", comment::baseUrl, accountId, authToken);

        auto response = co_await req.get(url);
        if (!response.ok()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Failed to fetch logs.", NotificationIcon::Error)->show();
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
        if (!json.isArray()) {
            queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
            });
            co_return;
        }

        queueInMainThread([retainedSelf, json = std::move(json)] {
            retainedSelf->m_loadingCircle->fadeOut();
            retainedSelf->m_list->clear();

            if (json.size() == 0) {
                auto emptyLabel = Label::create("No notifications yet.", "goldFont.fnt");
                emptyLabel->setLimitLabelWidth(retainedSelf->m_list->getContentWidth() - 20.f, 0.7f, 0.15f);
                retainedSelf->m_list->addChildAtPosition(emptyLabel, Anchor::Center);
                return;
            }

            retainedSelf->m_list->setCellColor(ccColor4B{0, 0, 0, 0});
            float cellWidth = retainedSelf->m_list->getContentWidth();

            for (size_t i = 0; i < json.size(); ++i) {
                auto item = json[i];
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
        });

        co_return;
    });
}
