#include "CBLogsPopup.hpp"
#include <Geode/utils/web.hpp>
#include <argon/argon.hpp>
#include "Geode/cocos/cocoa/CCGeometry.h"
#include "include/CBConstant.hpp"

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

    m_list = cue::ListNode::create({340.f, 220.f}, {0, 0, 0, 0}, cue::ListBorderStyle::Comments);
    if (m_list) {
        m_mainLayer->addChildAtPosition(m_list, Anchor::Center, {0.f, -15.f}, false);
    }

    auto listBg = NineSlice::create("square02_001.png");
    if (listBg) {
        listBg->setPosition(m_list->getPosition());
        listBg->setContentSize(m_list->getContentSize() + CCSize(5.f, 10.f));
        listBg->setOpacity(100);
        m_mainLayer->addChild(listBg, -1);
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
        FLAlertLayer::create("Error", "Invalid account ID.", "OK")->show();
        return;
    }

    Ref<CBLogsPopup> retainedSelf = this;
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
        auto url = fmt::format("{}/getUserLogs?accountId={}&argonToken={}", comment::baseUrl, accountId, authToken);

        auto response = co_await req.get(url);
        if (!response.ok()) {
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                FLAlertLayer::create("Error", "Failed to fetch logs.", "OK")->show();
            });
            co_return;
        }

        auto jsonRes = response.json();
        if (jsonRes.isErr()) {
            geode::queueInMainThread([retainedSelf] {
                retainedSelf->m_loadingCircle->fadeOut();
                FLAlertLayer::create("Error", "Invalid JSON from server.", "OK")->show();
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

        geode::queueInMainThread([retainedSelf, json = std::move(json)] {
            retainedSelf->m_loadingCircle->fadeOut();
            retainedSelf->m_list->clear();

            if (json.size() == 0) {
                auto emptyLabel = CCLabelBMFont::create("No notifications yet.", "goldFont.fnt");
                emptyLabel->limitLabelWidth(retainedSelf->m_list->getContentWidth() - 20.f, 0.7f, 0.15f);
                retainedSelf->m_list->addChildAtPosition(emptyLabel, Anchor::Center);
                return;
            }

            for (size_t i = 0; i < json.size(); ++i) {
                auto item = json[i];
                auto message = item["message"].asString().unwrapOr("Unknown message");
                auto createdAt = item["createdAt"].asString().unwrapOr("");
                auto actionType = item["actionType"].asString().unwrapOr("");

                auto cell = CCLayer::create();
                cell->setContentSize({340.f, 40.f});

                auto bg = CCScale9Sprite::create("square02_001.png");
                bg->setContentSize(cell->getContentSize());
                bg->setAnchorPoint({0, 0});
                bg->setPosition({0, 0});
                bg->setOpacity(50);
                if (actionType == "APPROVED") {
                    bg->setColor({100, 255, 100});
                } else if (actionType == "REJECTED") {
                    bg->setColor({255, 100, 100});
                }
                cell->addChild(bg);

                auto msgLabel = SimpleTextArea::create(message.c_str(), "chatFont.fnt");
                msgLabel->setWidth(330.f);
                msgLabel->setMaxLines(2);
                msgLabel->setScale(0.6f);
                msgLabel->setAlignment(kCCTextAlignmentLeft);
                msgLabel->setAnchorPoint({0.f, 0.5f});
                msgLabel->setPosition({10.f, 20.f});
                cell->addChild(msgLabel);

                retainedSelf->m_list->setCellColor(ccColor4B{0, 0, 0, 0});
                retainedSelf->m_list->addCell(cell);
            }
            retainedSelf->m_list->updateLayout();
        });

        co_return;
    });
}
