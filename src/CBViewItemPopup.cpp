#include "CBViewItemPopup.hpp"
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/Button.hpp>
#include "CBBannerCell.hpp"
#include "CBProfileBannerPopup.hpp"
#include "CBUsersListBannerPopup.hpp"
#include "include/CBConstant.hpp"

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
    if (!Popup::init(380.f, 190.f, "GJ_square01.png")) {
        return false;
    }

    m_banner = banner;

    this->setTitle(m_banner.name.c_str(), "bigFont.fnt", 0.7f);

    float titleWidth = m_title->getContentSize().width * m_title->getScale();
    float currentIconX = -titleWidth / 2.f - 5.f;

    if (m_banner.isLimited) {
        if (auto limitIcon = CCSprite::createWithSpriteFrameName("GJ_sRecentIcon_001.png")) {
            limitIcon->setScale(0.8f);
            limitIcon->setAnchorPoint({1.f, 0.5f});
            m_mainLayer->addChildAtPosition(limitIcon, Anchor::Top, {currentIconX, -20.f});
            currentIconX -= limitIcon->getContentSize().width * limitIcon->getScale() + 4.f;
        }
    }
    if (m_banner.isFeatured) {
        if (auto starIcon = CCSprite::createWithSpriteFrameName("GJ_sStarsIcon_001.png")) {
            starIcon->setScale(0.8f);
            starIcon->setAnchorPoint({1.f, 0.5f});
            m_mainLayer->addChildAtPosition(starIcon, Anchor::Top, {currentIconX, -20.f});
        }
    }

    if (m_banner.isFeatured && m_banner.isLimited) {
        m_title->runAction(CCRepeatForever::create(CCSequence::create(
            CCTintTo::create(1.f, 255, 255, 50),
            CCTintTo::create(1.f, 255, 150, 255),
            nullptr)));
    } else if (m_banner.isFeatured) {
        m_title->runAction(CCRepeatForever::create(CCSequence::create(
            CCTintTo::create(1.f, 255, 255, 50),
            CCTintTo::create(1.f, 255, 255, 255),
            nullptr)));
    } else if (m_banner.isLimited) {
        m_title->runAction(CCRepeatForever::create(CCSequence::create(
            CCTintTo::create(1.f, 255, 150, 255),
            CCTintTo::create(1.f, 255, 255, 255),
            nullptr)));
    }

    if (!m_banner.username.empty()) {
        if (auto usernameLabel = Button::createWithLabel(fmt::format("By {}", m_banner.username).c_str(), "goldFont.fnt", [this](geode::Button* sender) {
                CBProfileBannerPopup::create(m_banner.accountId, m_banner.username)->show();
            })) {
            usernameLabel->setAnchorPoint({0.5f, 0.5f});
            usernameLabel->setScale(0.5f);
            m_mainLayer->addChildAtPosition(usernameLabel, Anchor::Top, {0, -43.f});
        }
    }

    auto priceNode = CCNode::create();
    priceNode->setScale(0.5f);
    priceNode->setContentSize({400.f, 30.f});
    priceNode->setLayout(RowLayout::create()->setAxisAlignment(AxisAlignment::Start)->setGap(5));
    priceNode->setAnchorPoint({0.f, 0.f});
    auto priceLabel = CCLabelBMFont::create(fmt::format("{}", GameToolbox::pointsToString(m_banner.price)).c_str(), "bigFont.fnt");
    if (priceLabel) {
        priceLabel->setAnchorPoint({1.f, 0.5f});
        priceLabel->limitLabelWidth(150.f, 0.45f, 0.2f);
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
        auto bannerSprite = comment::createBannerNode(m_banner.imageUrl, {345.f, 35.f});
        if (bannerSprite) {
            m_mainLayer->addChildAtPosition(bannerSprite, Anchor::Center, {0.f, 15.f}, false);
        }
    }

    auto descriptionLabel = SimpleTextArea::create(m_banner.description.c_str(), "chatFont.fnt");
    if (descriptionLabel) {
        descriptionLabel->setWidth(m_mainLayer->getContentSize().width - 40.f);
        descriptionLabel->setMaxLines(3);
        descriptionLabel->setScale(0.8);
        descriptionLabel->setAlignment(kCCTextAlignmentCenter);
        m_mainLayer->addChildAtPosition(descriptionLabel, Anchor::Center, {0.f, -20.f}, false);
    }

    auto detailNode = CCNode::create();
    detailNode->setContentSize({200, 40});
    detailNode->setAnchorPoint({1.f, 0.f});
    detailNode->setLayout(ColumnLayout::create()
            ->setCrossAxisAlignment(AxisAlignment::End)
            ->setCrossAxisLineAlignment(AxisAlignment::End)
            ->setAutoScale(false)
            ->setCrossAxisOverflow(false));
    if (m_banner.isLimited) {
        if (auto amountLabel = CCLabelBMFont::create(fmt::format("Amount Left: {}", m_banner.amount - m_banner.totalBought).c_str(), "goldFont.fnt")) {
            amountLabel->setAnchorPoint({1.f, 0.5f});
            amountLabel->limitLabelWidth(80.f, 0.5f, 0.2f);
            detailNode->addChild(amountLabel);
        }
    }

    if (auto totalBoughtLabel = CCLabelBMFont::create(fmt::format("Bought: {}", m_banner.totalBought).c_str(), "goldFont.fnt")) {
        totalBoughtLabel->setAnchorPoint({1.f, 0.5f});
        totalBoughtLabel->limitLabelWidth(80.f, 0.5f, 0.2f);
        detailNode->addChild(totalBoughtLabel);
    }

    if (auto equippedLabel = CCLabelBMFont::create(fmt::format("Equipped: {}", m_banner.equippedCount).c_str(), "goldFont.fnt")) {
        equippedLabel->setAnchorPoint({1.f, 0.5f});
        equippedLabel->limitLabelWidth(80.f, 0.5f, 0.2f);
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

    if (auto boughtBtn = geode::Button::createWithNode(ButtonSprite::create("Users Bought", 100.f, true, "goldFont.fnt", "GJ_button_04.png", .0f, 1.f), [this](geode::Button* sender) {
            if (auto popup = CBUsersListBannerPopup::create(m_banner.id, 1)) {
                popup->show();
            }
        })) {
        boughtBtn->setScale(0.6f);
        listMenu->addChild(boughtBtn);
    }

    if (auto equippedBtn = geode::Button::createWithNode(ButtonSprite::create("Users Equipped", 100.f, true, "goldFont.fnt", "GJ_button_04.png", .0f, 1.f), [this](geode::Button* sender) {
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
