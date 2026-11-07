#include "CBUserCell.hpp"
#include "CBProfileBannerPopup.hpp"
#include "include/CBConstant.hpp"
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/SimplePlayer.hpp>

namespace {
    constexpr float kCellHeight = 40.f;

    // a stat with an icon in front
    CCNode* createStat(char const* frameName, std::string const& text, ccColor3B color) {
        auto node = CCNode::create();
        node->setAnchorPoint({0.f, 0.5f});
        node->setContentSize({0.f, 14.f});
        node->setLayout(RowLayout::create()->setGap(2.f)->setAutoScale(false)->setAutoGrowAxis(0.f));

        auto icon = CCSprite::createWithSpriteFrameName(frameName);
        if (icon) {
            icon->setScale(12.f / std::max(icon->getContentHeight(), 1.f));
            node->addChild(icon);
        }

        auto label = Label::create(text.c_str(), "bigFont.fnt");
        label->setScale(0.3f);
        label->setColor(color);
        node->addChild(label);

        node->updateLayout();
        return node;
    }
}

CBUserData CBUserData::fromJson(matjson::Value const& json) {
    CBUserData data;
    data.accountId = static_cast<int>(json["accountId"].asInt().unwrapOr(0));
    data.username = json["username"].asString().unwrapOr("Unknown");
    data.isAdmin = json["is_admin"].asBool().unwrapOr(false);
    data.isStaff = json["is_staff"].asBool().unwrapOr(false);
    data.isSupporter = json["is_supporter"].asBool().unwrapOr(false);
    data.equippedBannerUrl = json["equippedBannerUrl"].asString().unwrapOr("");
    data.iconId = static_cast<int>(json["iconId"].asInt().unwrapOr(0));
    data.color1 = static_cast<int>(json["color1"].asInt().unwrapOr(0));
    data.color2 = static_cast<int>(json["color2"].asInt().unwrapOr(0));
    data.glowColor = static_cast<int>(json["glowColor"].asInt().unwrapOr(0));
    data.hasGlow = json["hasGlow"].asBool().unwrapOr(false);
    data.amethyst = static_cast<int>(json["amethyst"].asInt().unwrapOr(0));
    data.monthlyEarnings = static_cast<int>(json["monthlyEarnings"].asInt().unwrapOr(0));
    data.totalEarned = static_cast<int>(json["totalEarned"].asInt().unwrapOr(0));
    data.totalSales = static_cast<int>(json["totalSales"].asInt().unwrapOr(0));
    return data;
}

CBUserCell* CBUserCell::create(CBUserData const& data, float width, CBUserCellOptions options) {
    auto ret = new CBUserCell();
    if (ret && ret->init(data, width, std::move(options))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

CCNode* CBUserCell::createStatsNode(CBUserData const& data, bool showSalesCount) {
    auto row = CCNode::create();
    row->setAnchorPoint({0.f, 0.5f});
    row->setContentSize({0.f, 14.f});
    row->setLayout(RowLayout::create()
            ->setGap(8.f)
            ->setAutoScale(false)
            ->setAutoGrowAxis(0.f)
            ->setAxisAlignment(AxisAlignment::Center));

    row->addChild(createStat("CB_amethyst_001.png"_spr, GameToolbox::pointsToString(data.amethyst), {255, 255, 255}));
    row->addChild(createStat("particle_337_001.png", fmt::format("+{}/mo", GameToolbox::pointsToString(data.monthlyEarnings)), {100, 255, 100}));
    row->addChild(createStat("particle_336_001.png",
        showSalesCount ? fmt::format("{} Sales", GameToolbox::pointsToString(data.totalSales))
                       : GameToolbox::pointsToString(data.totalEarned),
        {150, 255, 150}));

    row->updateLayout();
    return row;
}

SimplePlayer* CBUserCell::createPlayer(CBUserData const& data) {
    auto gm = GameManager::sharedState();
    int iconId = data.iconId > 0 ? data.iconId : 1;
    auto player = SimplePlayer::create(iconId);
    player->updatePlayerFrame(iconId, IconType::Cube);
    if (data.iconId > 0) {
        player->setColor(gm->colorForIdx(data.color1));
        player->setSecondColor(gm->colorForIdx(data.color2));
        if (data.hasGlow) {
            player->setGlowOutline(gm->colorForIdx(data.glowColor));
        } else {
            player->disableGlowOutline();
        }
    }
    player->updateColors();
    return player;
}

CCNode* CBUserCell::createBadges(CBUserData const& data, float height) {
    std::vector<char const*> frames;
    if (data.isAdmin) frames.push_back("CB_admin_badge.png"_spr);
    else if (data.isStaff) frames.push_back("CB_staff_badge.png"_spr);
    if (data.isSupporter) frames.push_back("CB_supporter_badge.png"_spr);
    if (frames.empty()) return nullptr;

    auto row = CCNode::create();
    row->setAnchorPoint({0.f, 0.5f});
    row->setContentSize({0.f, height});
    row->setLayout(RowLayout::create()
            ->setGap(4.f)
            ->setAutoScale(false)
            ->setAutoGrowAxis(0.f)
            ->setAxisAlignment(AxisAlignment::Start));

    for (auto frame : frames) {
        if (auto badge = CCSprite::createWithSpriteFrameName(frame)) {
            badge->setScale(height / std::max(badge->getContentHeight(), 1.f));
            row->addChild(badge);
        }
    }
    row->updateLayout();
    return row;
}

bool CBUserCell::init(CBUserData const& data, float width, CBUserCellOptions options) {
    if (!CCNode::init()) return false;
    m_data = data;
    m_options = std::move(options);
    this->setContentSize({width, kCellHeight});

    // equipped banner behind the cell, faded to the left so the text stays readable
    if (m_options.showBanner && !m_data.equippedBannerUrl.empty()) {
        if (auto banner = comment::createBannerNode(m_data.equippedBannerUrl, {width + 10.f, kCellHeight + 10.f})) {
            banner->setPosition({width / 2.f, kCellHeight / 2.f});
            this->addChild(banner, -2);
        }
        auto fade = CCLayerGradient::create({0, 0, 0, 200}, {0, 0, 0, 0}, {1, 0});
        fade->setContentSize({width, kCellHeight});
        this->addChild(fade, -1);
    }

    float x = 10.f;

    if (m_options.rank > 0) {
        auto rankLabel = Label::create(numToString(m_options.rank).c_str(), "goldFont.fnt");
        rankLabel->setLimitLabelWidth(28.f, 0.6f, 0.2f);
        rankLabel->setAnchorPoint({0.f, 0.5f});
        rankLabel->setPosition({x, kCellHeight / 2.f});
        this->addChild(rankLabel);
        x += 30.f;
    }

    // player cube
    auto player = createPlayer(m_data);
    player->setScale(0.7f);
    player->setPosition({x + 12.f, kCellHeight / 2.f});
    this->addChild(player, 1);
    x += 30.f;

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    menu->setContentSize({width, kCellHeight});
    this->addChild(menu, 2);

    // manage button (admin panel)
    float rightEdge = width - 10.f;
    if (m_options.onManage) {
        auto onManage = m_options.onManage;
        auto manageBtn = Button::createWithNode(
            CircleButtonSprite::createWithSpriteFrameName("followBtnOff_001.png", 1.f, CircleBaseColor::Blue),
            [onManage](Button*) { onManage(); });
        manageBtn->setScale(0.75f);
        manageBtn->setPosition({width - 22.f, kCellHeight / 2.f});
        menu->addChild(manageBtn);
        rightEdge = width - 42.f;
    }

    float availableWidth = rightEdge - x;
    constexpr float kNameY = 27.f;
    constexpr float kStatsY = 12.f;

    // username (opens their profile)
    auto nameLabel = Label::create(m_data.username.c_str(), "goldFont.fnt");
    nameLabel->setLimitLabelWidth(std::max(40.f, availableWidth - 60.f), 0.6f, 0.2f);
    int accountId = m_data.accountId;
    std::string username = m_data.username;
    auto nameBtn = Button::createWithNode(nameLabel, [accountId, username](Button*) {
        if (auto popup = CBProfileBannerPopup::create(accountId, username)) {
            popup->show();
        }
    });
    nameBtn->setAnchorPoint({0.f, 0.5f});
    nameBtn->setPosition({x, kNameY});
    menu->addChild(nameBtn);
    float nameRight = x + nameBtn->getScaledContentWidth() + 4.f;


    // badges, as tall as the name
    if (auto badges = createBadges(m_data, nameLabel->getScaledContentHeight())) {
        badges->setPosition({nameRight, kNameY});
        this->addChild(badges, 1);
        nameRight += badges->getScaledContentWidth() + 4.f;
    }

    if (m_options.showRole) {
        std::string role = m_data.isAdmin ? "Admin" : (m_data.isStaff ? "Staff" : "User");
        auto roleLabel = Label::create(role.c_str(), "chatFont.fnt");
        roleLabel->setScale(0.4f);
        roleLabel->setAnchorPoint({0.f, 0.5f});
        roleLabel->setPosition({nameRight, kNameY - 1.f});
        roleLabel->setColor(m_data.isAdmin ? ccColor3B{255, 100, 100} : (m_data.isStaff ? ccColor3B{100, 255, 100} : ccColor3B{200, 200, 200}));
        this->addChild(roleLabel, 1);
    }

    // stats under the name
    auto stats = createStatsNode(m_data, m_options.showSalesCount);
    if (stats->getContentWidth() > availableWidth) {
        stats->setScale(availableWidth / stats->getContentWidth());
    }
    stats->setPosition({x, kStatsY});
    this->addChild(stats, 1);

    return true;
}
