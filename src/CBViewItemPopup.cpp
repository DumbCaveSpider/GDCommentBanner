#include "CBViewItemPopup.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/MDTextArea.hpp>
#include <Geode/ui/Toggler.hpp>
#include "CBBannerCell.hpp"
#include "CBProfileBannerPopup.hpp"
#include "CBUsersListBannerPopup.hpp"
#include "CBBannerPreview.hpp"

using namespace geode::prelude;

CBViewItemPopup* CBViewItemPopup::create(const CBBannerItem& banner) {
    auto popup = new CBViewItemPopup();
    if (!popup || !popup->init(banner)) {
        delete popup;
        return nullptr;
    }
    popup->autorelease();
    return popup;
}

bool CBViewItemPopup::init(const CBBannerItem& banner) {
    if (!Popup::init(380.f, 220.f, "GJ_square01.png")) {
        return false;
    }

    m_banner = banner;

    this->setTitle(m_banner.name.c_str(), "bigFont.fnt", 0.7f);

    // tappable icons left of the title, and the title tint
    CBBannerCell::decoratePopupTitle(m_mainLayer, m_title, m_banner);

    if (!m_banner.username.empty()) {
        if (auto usernameLabel = Button::createWithLabel(fmt::format("By {}", m_banner.username).c_str(), "goldFont.fnt", [this](Button* sender) {
                CBProfileBannerPopup::create(m_banner.accountId, m_banner.username)->show();
            })) {
            usernameLabel->setAnchorPoint({0.5f, 0.5f});
            usernameLabel->setScale(0.5f);
            m_mainLayer->addChildAtPosition(usernameLabel, Anchor::Top, {0, -40.f});
        }
    }

    // when it was uploaded / last edited / approved (dates only, missing ones are skipped)
    std::vector<std::string> dates;
    auto addDate = [&](char const* what, std::string const& when) {
        if (!when.empty()) dates.push_back(fmt::format("{} {}", what, when.substr(0, 10)));
    };
    addDate("Uploaded", m_banner.createdAt);
    addDate("Edited", m_banner.editedAt);
    addDate("Approved", m_banner.approvedAt);
    if (!dates.empty()) {
        auto datesLabel = Label::create(fmt::format("{}", fmt::join(dates, "  |  ")).c_str(), "chatFont.fnt");
        datesLabel->setLimitLabelWidth(m_mainLayer->getContentWidth() - 120.f, 0.5f, 0.1f);
        datesLabel->setColor({200, 200, 200});
        m_mainLayer->addChildAtPosition(datesLabel, Anchor::Top, {0, -55.f}, false);
    }

    auto priceNode = CCNode::create();
    priceNode->setScale(0.5f);
    priceNode->setContentSize({180.f, 30.f});
    priceNode->setAnchorPoint({0.f, 0.f});
    priceNode->setLayout(RowLayout::create()->setAxisAlignment(AxisAlignment::Start)->setGap(5));
    auto priceLabel = Label::create(fmt::format("{}", GameToolbox::pointsToString(m_banner.price)).c_str(), "bigFont.fnt");
    if (priceLabel) {
        priceLabel->setAnchorPoint({1.f, 0.5f});
        priceLabel->setLimitLabelWidth(150.f, 0.45f, 0.2f);
        priceNode->addChild(priceLabel);

        if (auto amethystIcon = CCSprite::createWithSpriteFrameName("CB_amethyst_002.png"_spr)) {
            amethystIcon->setAnchorPoint({0.f, 0.5f});
            amethystIcon->setScale(0.5f);
            priceNode->addChild(amethystIcon);
        }

        m_mainLayer->addChildAtPosition(priceNode, Anchor::BottomLeft, {15.f, 15.f}, false);
        priceNode->updateLayout();
    }

    if (!m_banner.imageUrl.empty()) {
        // the banner on comment cells, fits between the dates and the description
        if (auto preview = CBBannerPreview::create(m_banner.imageUrl, m_banner.nonCompactImageUrl, {345.f, 65.f})) {
            m_mainLayer->addChildAtPosition(preview, Anchor::Center, {0.f, 15.f}, false);
            // Non-Compact / Comment Overlay toggles, top right
            m_mainLayer->addChildAtPosition(preview->createToggles(), Anchor::TopRight, {-8.f, -8.f}, false);
        }
    }

    auto descriptionLabel = MDTextArea::create(m_banner.description, {m_mainLayer->getContentSize().width - 40.f, 45.f});
    if (descriptionLabel) {
        m_mainLayer->addChildAtPosition(descriptionLabel, Anchor::Center, {0.f, -45.f}, false);
    }

    auto detailNode = CCNode::create();
    detailNode->setContentSize({200, 35});
    detailNode->setAnchorPoint({1.f, 0.f});
    detailNode->setLayout(ColumnLayout::create()
            ->setGap(0.f)
            ->setCrossAxisAlignment(AxisAlignment::End)
            ->setCrossAxisLineAlignment(AxisAlignment::End)
            ->setAutoScale(false)
            ->setCrossAxisOverflow(false));
    if (m_banner.isLimited) {
        if (auto amountLabel = Label::create(fmt::format("Amount Left: {}", m_banner.amount - m_banner.totalBought).c_str(), "goldFont.fnt")) {
            amountLabel->setAnchorPoint({1.f, 0.5f});
            amountLabel->setLimitLabelWidth(80.f, 0.5f, 0.2f);
            detailNode->addChild(amountLabel);
        }
    }

    if (auto totalBoughtLabel = Label::create(fmt::format("Bought: {}", m_banner.totalBought).c_str(), "goldFont.fnt")) {
        totalBoughtLabel->setAnchorPoint({1.f, 0.5f});
        totalBoughtLabel->setLimitLabelWidth(80.f, 0.5f, 0.2f);
        detailNode->addChild(totalBoughtLabel);
    }

    if (auto equippedLabel = Label::create(fmt::format("Equipped: {}", m_banner.equippedCount).c_str(), "goldFont.fnt")) {
        equippedLabel->setAnchorPoint({1.f, 0.5f});
        equippedLabel->setLimitLabelWidth(80.f, 0.5f, 0.2f);
        detailNode->addChild(equippedLabel);
    }

    if (detailNode->getChildrenCount() > 0) {
        m_mainLayer->addChildAtPosition(detailNode, Anchor::BottomRight, {-10.f, 10.f}, false);
        detailNode->updateLayout();
    }

    auto listMenu = CCMenu::create();
    listMenu->setAnchorPoint({0.5f, 0.f});
    listMenu->setContentWidth(160.f);
    listMenu->setLayout(RowLayout::create()->setGap(3.f)->setAutoScale(true)->setCrossAxisOverflow(true)->setGrowCrossAxis(false));

    if (auto boughtBtn = Button::createWithNode(ButtonSprite::create("Users Bought", 100.f, true, "goldFont.fnt", "GJ_button_04.png", .0f, 1.f), [this](Button* sender) {
            if (auto popup = CBUsersListBannerPopup::create(m_banner.id, 1)) {
                popup->show();
            }
        })) {
        boughtBtn->setScale(0.6f);
        listMenu->addChild(boughtBtn);
    }

    if (auto equippedBtn = Button::createWithNode(ButtonSprite::create("Users Equipped", 100.f, true, "goldFont.fnt", "GJ_button_04.png", .0f, 1.f), [this](Button* sender) {
            if (auto popup = CBUsersListBannerPopup::create(m_banner.id, 2)) {
                popup->show();
            }
        })) {
        equippedBtn->setScale(0.6f);
        listMenu->addChild(equippedBtn);
    }

    listMenu->updateLayout();
    m_mainLayer->addChildAtPosition(listMenu, Anchor::Bottom, {0.f, 15.f});

    return true;
}
