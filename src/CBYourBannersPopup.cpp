#include "CBYourBannersPopup.hpp"
#include <Geode/utils/web.hpp>
#include <argon/argon.hpp>
#include "Geode/ui/Button.hpp"
#include "include/CBConstant.hpp"
#include <Geode/ui/Scrollbar.hpp>
#include "CBShopLayer.hpp"

using namespace geode::prelude;

CBYourBannersPopup* CBYourBannersPopup::create() {
    auto ret = new CBYourBannersPopup();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBYourBannersPopup::init() {
    if (!Popup::init(380.f, 280.f)) return false;

    this->setTitle("Your Banners");

    m_list = cue::ListNode::create({340.f, 220.f}, {0, 0, 0, 0}, cue::ListBorderStyle::Comments);
    if (m_list) {
        m_mainLayer->addChildAtPosition(m_list, Anchor::Center, {0.f, -15.f}, false);

        auto scrollbar = Scrollbar::create(m_list->getScrollLayer());
        m_mainLayer->addChildAtPosition(scrollbar, Anchor::Center, {340.f / 2 + 5.f, -15.f}, false);
    }

    auto listBg = NineSlice::create("square02_001.png");
    if (listBg) {
        listBg->setPosition(m_list->getPosition());
        listBg->setContentSize(m_list->getContentSize() + CCSize(5.f, 10.f));
        listBg->setOpacity(100);
        m_mainLayer->addChild(listBg, -1);
    }

    fetchBanners();

    return true;
}

void CBYourBannersPopup::fetchBanners() {
    if (m_loadingCircle) m_loadingCircle->removeFromParent();
    m_loadingCircle = cue::LoadingCircle::create(true);
    m_list->addChildAtPosition(m_loadingCircle, Anchor::Center, CCPointZero, false);
    m_loadingCircle->fadeIn();

    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;

    if (accountId <= 0) {
        m_loadingCircle->fadeOut();
        Notification::create("Invalid account ID.", NotificationIcon::Error)->show();
        return;
    }

    Ref<CBYourBannersPopup> retainedSelf = this;
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
        auto url = fmt::format("{}/getUserBanners?accountId={}&argonToken={}", comment::baseUrl, accountId, authToken);

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
                Notification::create("Invalid JSON from server.", NotificationIcon::Error)->show();
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
                auto emptyLabel = CCLabelBMFont::create("No Banners", "goldFont.fnt");
                emptyLabel->limitLabelWidth(retainedSelf->m_list->getContentWidth() - 20.f, 0.7f, 0.15f);
                retainedSelf->m_list->addChildAtPosition(emptyLabel, Anchor::Center);
                return;
            }

            float totalEarnRate = 0.f;

            for (size_t i = 0; i < json.size(); ++i) {
                auto item = json[i];
                auto name = item["name"].asString().unwrapOr("Unknown Banner");
                auto imageUrl = item["imageUrl"].asString().unwrapOr("");
                int price = item["price"].asInt().unwrapOr(0);
                bool isPending = item["isPending"].asBool().unwrapOr(false);
                bool isLimited = item["isLimited"].asBool().unwrapOr(false);
                int amount = item["amount"].asInt().unwrapOr(0);
                int totalBought = item["totalBought"].asInt().unwrapOr(0);
                int equippedCount = item["equippedCount"].asInt().unwrapOr(0);
                int id = item["id"].asInt().unwrapOr(0);

                auto status = isPending ? "PENDING" : "APPROVED";

                float width = 340.f;
                float cellHeight = 96.f;
                auto cell = CCLayer::create();
                cell->setContentSize({width, cellHeight});

                // Background
                if (auto background = NineSlice::createWithSpriteFrameName("geode.loader/tab-bg.png")) {
                    background->setContentSize({width - 5, cellHeight});
                    background->setPosition({width / 2.f, cellHeight / 2.f});
                    if (isPending) {
                        background->setColor({255, 255, 150});
                    } else {
                        background->setColor({150, 255, 150});
                    }
                    cell->addChild(background);
                }

                // Image
                auto sprite = comment::createBannerNode(imageUrl, {324.f, 104.f});
                sprite->setPosition({width / 2.f, cellHeight - 30.f});
                cell->addChild(sprite);

                // Name
                auto nameLabel = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
                nameLabel->setAnchorPoint({0.f, 0.5f});
                float nameX = 10.f;
                if (isLimited) {
                    if (auto starIcon = CCSprite::createWithSpriteFrameName("GJ_sRecentIcon_001.png")) {
                        starIcon->setScale(0.8f);
                        starIcon->setPosition({nameX, 13.f});
                        starIcon->setAnchorPoint({0.f, 0.5f});
                        cell->addChild(starIcon);
                        nameLabel->runAction(CCRepeatForever::create(CCSequence::create(
                            CCTintTo::create(1.f, 255, 150, 255),
                            CCTintTo::create(1.f, 255, 255, 255),
                            nullptr)));
                        nameX += starIcon->getContentSize().width * starIcon->getScale() + 4.f;
                    }
                }
                nameLabel->setPosition({nameX, 15.f});
                nameLabel->limitLabelWidth(120.f, 0.5f, 0.2f);
                cell->addChild(nameLabel);

                // Status Label
                auto statusLabel = CCLabelBMFont::create(status, "goldFont.fnt");
                statusLabel->setScale(0.5f);
                statusLabel->setAnchorPoint({1.f, 0.5f});
                statusLabel->setPosition({width - 10.f, 15.f});
                if (isPending) {
                    statusLabel->setColor({255, 200, 50});
                } else {
                    statusLabel->setColor({50, 255, 50});
                }
                cell->addChild(statusLabel);

                if (isPending) {
                    auto refundBtn = geode::Button::createWithNode(ButtonSprite::create("Refund"), [retainedSelf, id](geode::Button* sender) {
                        retainedSelf->onRefund(sender);
                    });
                    refundBtn->setTag(id);
                    refundBtn->setPosition({width - 45.f, 35.f});
                    refundBtn->setScale(0.6f);
                    cell->addChild(refundBtn);
                }

                // Price
                if (auto priceLabel = CCLabelBMFont::create(fmt::format("{}", GameToolbox::pointsToString(price)).c_str(), "bigFont.fnt")) {
                    priceLabel->setAnchorPoint({0.f, 0.5f});
                    priceLabel->setScale(0.5f);
                    priceLabel->setPosition({-8.f, 0.f});

                    auto priceNode = CCNode::create();
                    priceNode->setPosition({20.f, 35.f});
                    priceNode->addChild(priceLabel);

                    if (auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr)) {
                        amethystIcon->setScale(0.5f);
                        auto priceWidth = priceLabel->getContentSize().width * priceLabel->getScale();
                        amethystIcon->setPosition({priceWidth + 4.f, 0.f});
                        priceNode->addChild(amethystIcon);
                    }
                    cell->addChild(priceNode);
                }

                // Details (amount/bought/earn rate)
                auto detailNode = CCNode::create();
                detailNode->setScale(0.6f);
                detailNode->setAnchorPoint({1.f, 1.f});
                detailNode->setContentSize({200.f, 75.f});
                detailNode->setLayout(ColumnLayout::create()
                        ->setAxisAlignment(AxisAlignment::Center)
                        ->setCrossAxisLineAlignment(AxisAlignment::End)
                        ->setCrossAxisAlignment(AxisAlignment::End)
                        ->setGap(2.f)
                        ->setAxisReverse(true)
                        ->setAutoScale(false)
                        ->setGrowCrossAxis(true)
                        ->setCrossAxisOverflow(false));

                if (isLimited) {
                    if (auto amountLabel = CCLabelBMFont::create(fmt::format("Amount Left: {}", amount - totalBought).c_str(), "goldFont.fnt")) {
                        amountLabel->limitLabelWidth(150.f, 0.5f, 0.2f);
                        detailNode->addChild(amountLabel);
                    }
                }

                if (auto totalBoughtLabel = CCLabelBMFont::create(fmt::format("Bought: {}", totalBought).c_str(), "goldFont.fnt")) {
                    totalBoughtLabel->limitLabelWidth(150.f, 0.5f, 0.2f);
                    detailNode->addChild(totalBoughtLabel);
                }

                if (!isPending) {
                    int rateMultiplier = equippedCount / 5;
                    float rate = std::min(0.30f, rateMultiplier * 0.05f);
                    float bannerEarnRate = price * rate;
                    totalEarnRate += bannerEarnRate;

                    if (auto equippedLabel = CCLabelBMFont::create(fmt::format("Total Equipped: {}", equippedCount).c_str(), "goldFont.fnt")) {
                        equippedLabel->limitLabelWidth(150.f, 0.5f, 0.2f);
                        detailNode->addChild(equippedLabel);
                    }

                    if (auto earnLabel = CCLabelBMFont::create(fmt::format("Earn: +{:.1f}/mo", bannerEarnRate).c_str(), "goldFont.fnt")) {
                        earnLabel->limitLabelWidth(150.f, 0.5f, 0.2f);
                        detailNode->addChild(earnLabel);
                    }
                }

                detailNode->updateLayout();

                if (detailNode->getChildrenCount() > 0) {
                    cell->addChildAtPosition(detailNode, Anchor::BottomRight, {-90.f, 50.f}, false);
                }

                retainedSelf->m_list->setCellColor(ccColor4B{0, 0, 0, 0});
                retainedSelf->m_list->addCell(cell);
            }
            retainedSelf->m_list->updateLayout();

            if (totalEarnRate >= 0.f) {
                if (retainedSelf->m_earnLabel) {
                    retainedSelf->m_earnLabel->setString(fmt::format("+{:.1f}/mo", totalEarnRate).c_str());
                } else {
                    auto earnNode = CCNode::create();

                    retainedSelf->m_earnLabel = CCLabelBMFont::create(fmt::format("+{:.1f}/mo", totalEarnRate).c_str(), "bigFont.fnt");
                    retainedSelf->m_earnLabel->setScale(0.5f);
                    retainedSelf->m_earnLabel->setAnchorPoint({1.f, 0.5f});
                    retainedSelf->m_earnLabel->setPosition({0.f, 0.f});
                    earnNode->addChild(retainedSelf->m_earnLabel);

                    if (auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr)) {
                        amethystIcon->setScale(0.5f);
                        amethystIcon->setPosition({5.f, 0.f});
                        amethystIcon->setAnchorPoint({0.f, 0.5f});
                        earnNode->addChild(amethystIcon);
                    }

                    retainedSelf->m_mainLayer->addChildAtPosition(earnNode, Anchor::TopRight, {-30.f, -25.f});
                }
            }
        });

        co_return;
    });
}

void CBYourBannersPopup::onRefund(CCObject* sender) {
    int id = sender->getTag();
    geode::createQuickPopup("Refund Banner", "Are you sure you want to <cr>refund</c> this pending banner?", "Cancel", "Refund", [this, id](FLAlertLayer*, bool btn2) {
        if (!btn2) return;

        auto accountData = argon::getGameAccountData();
        auto accountId = accountData.accountId;

        if (accountId <= 0) {
            Notification::create("Invalid account ID.", NotificationIcon::Error)->show();
            return;
        }

        if (this->m_loadingCircle) this->m_loadingCircle->removeFromParent();
        this->m_loadingCircle = cue::LoadingCircle::create(true);
        this->m_list->addChildAtPosition(this->m_loadingCircle, Anchor::Center, CCPointZero, false);
        this->m_loadingCircle->fadeIn();

        Ref<CBYourBannersPopup> retainedSelf = this;
        arc::spawn([retainedSelf, accountId, accountData, id]() -> arc::Future<> {
            auto authResult = co_await comment::argonToken(accountData);
            if (authResult.empty()) {
                geode::queueInMainThread([retainedSelf] {
                    retainedSelf->m_loadingCircle->fadeOut();
                    Notification::create("Authentication failed.", NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto authToken = std::move(authResult);

            geode::utils::web::MultipartForm form;
            form.param("accountId", numToString(accountId));
            form.param("argonToken", authToken);
            form.param("bannerId", numToString(id));

            auto req = geode::utils::web::WebRequest();
            req.bodyMultipart(std::move(form));

            auto response = co_await req.post(fmt::format("{}/refundBanner", comment::baseUrl));
            if (!response.ok()) {
                geode::queueInMainThread([retainedSelf, response] {
                    retainedSelf->m_loadingCircle->fadeOut();
                    Notification::create(comment::getResponseMessage(response, "Failed to refund banner.").c_str(), NotificationIcon::Error)->show();
                });
                co_return;
            }

            auto jsonRes = response.json();
            int refundedAmount = 0;
            if (jsonRes.isOk()) {
                refundedAmount = jsonRes.unwrap()["refunded"].asInt().unwrapOr(0);
            }

            geode::queueInMainThread([retainedSelf, refundedAmount] {
                retainedSelf->m_loadingCircle->fadeOut();
                Notification::create("Banner refunded!", NotificationIcon::Success)->show();

                if (refundedAmount > 0) {
                    int currentAmethyst = Mod::get()->getSavedValue<int>("amethyst", 0);
                    int newAmethyst = currentAmethyst + refundedAmount;
                    Mod::get()->setSavedValue("amethyst", newAmethyst);
                    if (auto shopLayer = CBShopLayer::getInstance()) {
                        shopLayer->updateAmethystLabel(newAmethyst);
                    }
                }

                retainedSelf->fetchBanners();
            });

            co_return;
        });
    });
}
