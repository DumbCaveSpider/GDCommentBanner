#include "CBGiftAmethyst.hpp"
#include "CBShopLayer.hpp"
#include "ccTypes.h"
#include "include/CBConstant.hpp"
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/SimplePlayer.hpp>
#include <Geode/ui/NineSlice.hpp>
#include <Geode/utils/web.hpp>
#include <dankmeme.argon/argon.hpp>

namespace {
    constexpr float kColumnWidth = 140.f;
    constexpr float kRowHeight = 30.f;
    constexpr float kBackgroundPadding = 10.f;

    // horizontal row of nodes, centered
    CCNode* createRow() {
        auto row = CCNode::create();
        row->setAnchorPoint({0.5f, 0.5f});
        row->setContentSize({kColumnWidth, 20.f});
        row->setLayout(RowLayout::create()->setGap(4.f)->setAutoScale(false));
        return row;
    }

    void applyIcon(SimplePlayer* player, int iconId, int color1, int color2, int glowColor, bool hasGlow) {
        auto gm = GameManager::sharedState();
        if (iconId <= 0) iconId = 1;  // unknown icon, default cube
        player->updatePlayerFrame(iconId, IconType::Cube);
        player->setColor(gm->colorForIdx(color1));
        player->setSecondColor(gm->colorForIdx(color2));
        if (hasGlow) {
            player->setGlowOutline(gm->colorForIdx(glowColor));
        } else {
            player->disableGlowOutline();
        }
        player->updateColors();
    }
}

CBGiftAmethyst* CBGiftAmethyst::create(int targetAccountId) {
    auto ret = new CBGiftAmethyst();
    if (ret && ret->init(targetAccountId)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

CBGiftAmethyst::Column CBGiftAmethyst::createColumn() {
    Column column;
    column.menu = CCMenu::create();
    column.menu->setZOrder(2);
    column.menu->ignoreAnchorPointForPosition(false);
    column.menu->setAnchorPoint({0.5f, 0.5f});
    column.menu->setContentSize({kColumnWidth, kRowHeight * 4.f});
    column.menu->setLayout(ColumnLayout::create()
            ->setGap(10.f)
            ->setAxisReverse(true)  // first child on top
            ->setAutoScale(false)
            ->setAxisAlignment(AxisAlignment::Center));

    // icon + name
    column.nameRow = createRow();
    // SimplePlayer draws around its origin, so it sits centered in a fixed-size holder
    auto playerHolder = CCNode::create();
    playerHolder->setContentSize({20.f, 20.f});
    column.player = SimplePlayer::create(1);
    column.player->setScale(0.6f);
    column.player->setPosition({10.f, 10.f});
    playerHolder->addChild(column.player);
    column.nameRow->addChild(playerHolder);
    column.nameLabel = Label::create("...", "goldFont.fnt");
    column.nameLabel->setLimitLabelWidth(kColumnWidth - 30.f, 0.6f, 0.2f);
    column.nameRow->addChild(column.nameLabel);
    column.nameRow->updateLayout();
    column.menu->addChild(column.nameRow);

    // amethyst + balance
    column.balanceRow = createRow();
    if (auto amethyst = CCSprite::createWithSpriteFrameName("CB_amethyst_001.png"_spr)) {
        amethyst->setScale(0.5f);
        column.balanceRow->addChild(amethyst);
    }
    column.balanceLabel = Label::create("...", "bigFont.fnt");
    column.balanceLabel->setScale(0.45f);
    column.balanceRow->addChild(column.balanceLabel);
    column.balanceRow->updateLayout();
    column.menu->addChild(column.balanceRow);

    // row 3: the amount input on your side, an empty spacer on theirs (added by init)

    column.afterLabel = Label::create("", "bigFont.fnt");
    column.afterLabel->setScale(0.45f);
    return column;
}

void CBGiftAmethyst::addColumn(Column const& column, CCPoint offset) {
    column.menu->addChild(column.afterLabel);
    column.menu->updateLayout();
    m_mainLayer->addChildAtPosition(column.menu, Anchor::Center, offset, false);

    auto background = NineSlice::create("square02d_001.png");
    background->setContentSize(column.menu->getContentSize() + CCSize{kBackgroundPadding, kBackgroundPadding});
    background->setOpacity(100);
    m_mainLayer->addChildAtPosition(background, Anchor::Center, offset, false);
    background->setZOrder(1);
}

void CBGiftAmethyst::setUser(Column& column, std::string const& name, int balance) {
    column.nameLabel->setString(name.c_str());
    column.nameLabel->setLimitLabelWidth(kColumnWidth - 30.f, 0.6f, 0.2f);
    column.nameRow->updateLayout();
    column.balanceLabel->setString(GameToolbox::pointsToString(balance).c_str());
    column.balanceRow->updateLayout();
}

bool CBGiftAmethyst::init(int targetAccountId) {
    if (!Popup::init(340.f, 250.f, "GJ_square04.png")) return false;
    m_targetAccountId = targetAccountId;

    this->setTitle("Gift Amethysts");
    addSideArt(m_mainLayer, SideArt::All, SideArtStyle::PopupBlue, false);

    // your column, with your local icon
    m_myColumn = createColumn();
    auto gm = GameManager::sharedState();
    applyIcon(m_myColumn.player, gm->getPlayerFrame(), gm->getPlayerColor(), gm->getPlayerColor2(), gm->getPlayerGlowColor(), gm->getPlayerGlow());
    m_amountInput = TextInput::create(110.f, "Amount");
    m_amountInput->setCommonFilter(CommonFilter::Int);
    m_amountInput->setMaxCharCount(9);
    m_amountInput->setCallback([this](std::string const&) { this->updatePreview(); });
    m_amountInput->setEnabled(false);
    m_myColumn.menu->addChild(m_amountInput);
    addColumn(m_myColumn, {-80.f, 10.f});

    m_targetColumn = createColumn();
    addColumn(m_targetColumn, {80.f, 10.f});

    // arrow between the columns
    if (auto arrow = CCSprite::createWithSpriteFrameName("edit_rightBtn2_001.png")) {
        arrow->setScale(0.5f);
        arrow->setZOrder(3);
        m_mainLayer->addChildAtPosition(arrow, Anchor::Center, {0.f, 10.f}, false);
    }

    m_statusLabel = Label::create("", "bigFont.fnt");
    m_statusLabel->setScale(0.6f);
    m_statusLabel->setLimitLabelWidth(m_mainLayer->getContentSize().width - 30.f, 0.7f, 0.1f);
    m_mainLayer->addChildAtPosition(m_statusLabel, Anchor::Bottom, {0.f, 55.f}, false);

    m_giftBtn = Button::createWithNode(ButtonSprite::create("Gift", "goldFont.fnt", "GJ_button_01.png", .8f), [this](Button* sender) { this->onGift(sender); });
    m_giftBtn->setEnabled(false);
    m_giftBtn->setCascadeOpacityEnabled(true);  // so the dimmed look reaches the sprite
    m_giftBtn->setOpacity(150);
    m_buttonMenu->addChildAtPosition(m_giftBtn, Anchor::Bottom, {0.f, 25.f});

    m_loadingCircle = cue::LoadingCircle::create(true);
    m_mainLayer->addChildAtPosition(m_loadingCircle, Anchor::Center, CCPointZero, false);
    m_loadingCircle->fadeIn();

    fetchUsers();
    return true;
}

void CBGiftAmethyst::fetchUsers() {
    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;
    if (accountId <= 0) {
        m_loadingCircle->fadeOut();
        m_statusLabel->setString("You need to be logged in to gift amethysts.");
        m_statusLabel->setColor(ccRED);
        return;
    }

    Ref<CBGiftAmethyst> self = this;
    int targetAccountId = m_targetAccountId;
    arc::spawn([self, accountId, accountData, targetAccountId]() -> arc::Future<> {
        auto fail = [self](std::string message) {
            queueInMainThread([self, message] {
                if (self->m_loadingCircle) self->m_loadingCircle->fadeOut();
                self->m_statusLabel->setString(message.c_str());
                self->m_statusLabel->setColor(ccRED);
            });
        };

        auto authToken = co_await comment::argonToken(accountData);
        if (authToken.empty()) {
            fail("Authentication failed.");
            co_return;
        }

        // /getUser for you, then for them
        std::optional<matjson::Value> users[2];
        int const targets[2] = {accountId, targetAccountId};
        for (int i = 0; i < 2; ++i) {
            auto body = matjson::makeObject({{"accountId", accountId}, {"argonToken", authToken}, {"targetAccountId", targets[i]}});
            auto response = co_await utils::web::WebRequest().bodyJSON(body).post(fmt::format("{}/getUser", comment::baseUrl));
            if (!response.ok()) break;
            auto json = response.json();
            if (json.isErr()) break;
            users[i] = json.unwrap();
        }

        auto& me = users[0];
        auto& them = users[1];
        if (!me || !them) {
            fail("Failed to load users. They may not have used Comment Banners yet.");
            co_return;
        }

        queueInMainThread([self,
                              myName = (*me)["username"].asString().unwrapOr("You"),
                              myBalance = static_cast<int>((*me)["amethyst"].asInt().unwrapOr(0)),
                              target = std::move(*them)] {
            self->onUsersLoaded(myName, myBalance, target);
        });
        co_return;
    });
}

void CBGiftAmethyst::onUsersLoaded(std::string myName, int myBalance, matjson::Value target) {
    if (m_loadingCircle) m_loadingCircle->fadeOut();
    std::string targetName = target["username"].asString().unwrapOr("Unknown");
    int targetBalance = static_cast<int>(target["amethyst"].asInt().unwrapOr(0));

    m_loaded = true;
    m_myBalance = myBalance;
    m_targetBalance = targetBalance;
    m_targetName = targetName;

    setUser(m_myColumn, myName, myBalance);
    setUser(m_targetColumn, targetName, targetBalance);
    applyIcon(m_targetColumn.player,
        static_cast<int>(target["iconId"].asInt().unwrapOr(0)),
        static_cast<int>(target["color1"].asInt().unwrapOr(0)),
        static_cast<int>(target["color2"].asInt().unwrapOr(0)),
        static_cast<int>(target["glowColor"].asInt().unwrapOr(0)),
        target["hasGlow"].asBool().unwrapOr(false));

    // keep the shop counter in sync with what the server says
    Mod::get()->setSavedValue("amethyst", myBalance);
    if (auto shop = CBShopLayer::getInstance()) shop->updateAmethystLabel(myBalance);

    m_amountInput->setEnabled(targetBalance >= kMinRecipientBalance);
    updatePreview();
}

int CBGiftAmethyst::getAmount() const {
    return numFromString<int>(m_amountInput->getString()).unwrapOr(0);
}

void CBGiftAmethyst::updatePreview() {
    if (!m_loaded) return;

    bool canReceive = m_targetBalance >= kMinRecipientBalance;
    int amount = getAmount();
    bool valid = canReceive && amount > 0 && amount <= m_myBalance;

    if (amount > 0) {
        // your balance goes down (red), theirs goes up (green)
        m_myColumn.afterLabel->setString(fmt::format("{} (-{})", GameToolbox::pointsToString(std::max(0, m_myBalance - amount)), GameToolbox::pointsToString(amount)).c_str());
        m_myColumn.afterLabel->setColor(amount > m_myBalance ? ccGRAY : ccRED);
        m_targetColumn.afterLabel->setString(fmt::format("{} (+{})", GameToolbox::pointsToString(m_targetBalance + amount), GameToolbox::pointsToString(amount)).c_str());
        m_targetColumn.afterLabel->setColor(canReceive ? ccGREEN : ccGRAY);
    } else {
        m_myColumn.afterLabel->setString(GameToolbox::pointsToString(m_myBalance).c_str());
        m_myColumn.afterLabel->setColor(ccWHITE);
        m_targetColumn.afterLabel->setString(GameToolbox::pointsToString(m_targetBalance).c_str());
        m_targetColumn.afterLabel->setColor(ccWHITE);
    }
    m_myColumn.afterLabel->setLimitLabelWidth(kColumnWidth - 10.f, 0.45f, 0.2f);
    m_targetColumn.afterLabel->setLimitLabelWidth(kColumnWidth - 10.f, 0.45f, 0.2f);

    if (!canReceive) {
        m_statusLabel->setString(fmt::format("{} needs at least {} amethysts to receive gifts.", m_targetName, GameToolbox::pointsToString(kMinRecipientBalance)).c_str());
        m_statusLabel->setColor(ccRED);
    } else if (amount > m_myBalance) {
        m_statusLabel->setString("You don't have enough amethysts.");
        m_statusLabel->setColor(ccRED);
    } else {
        m_statusLabel->setString("");
    }

    m_giftBtn->setEnabled(valid);
    m_giftBtn->setOpacity(valid ? 255 : 150);
}

void CBGiftAmethyst::onGift(CCObject*) {
    int amount = getAmount();
    if (!m_loaded || amount <= 0 || amount > m_myBalance || m_targetBalance < kMinRecipientBalance) return;

    Ref<CBGiftAmethyst> self = this;
    createQuickPopup(
        "Gift Amethysts",
        fmt::format("Are you sure you want to gift <cp>{} amethysts</c> to <cy>{}</c>?\nThis <cr>cannot be undone</c>.", GameToolbox::pointsToString(amount), m_targetName),
        "Cancel",
        "Gift",
        [self, amount](FLAlertLayer*, bool btn2) {
            if (btn2) self->sendGift(amount);
        });
}

void CBGiftAmethyst::sendGift(int amount) {
    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;
    if (accountId <= 0) return;

    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Sending gift...");
    popup->show();

    Ref<CBGiftAmethyst> self = this;
    int targetAccountId = m_targetAccountId;
    arc::spawn([self, accountId, accountData, targetAccountId, amount, popup]() -> arc::Future<> {
        auto authToken = co_await comment::argonToken(accountData);
        if (authToken.empty()) {
            queueInMainThread([popup] {
                if (popup && popup->getParent()) popup->showFailMessage("Authentication failed.");
            });
            co_return;
        }

        auto body = matjson::makeObject({{"accountId", accountId},
            {"argonToken", authToken},
            {"targetAccountId", targetAccountId},
            {"amount", amount}});
        auto response = co_await utils::web::WebRequest().bodyJSON(body).post(fmt::format("{}/giftAmethyst", comment::baseUrl));
        if (!response.ok()) {
            auto error = comment::getResponseMessage(response, "Failed to gift amethysts.");
            queueInMainThread([popup, error] {
                if (popup && popup->getParent()) popup->showFailMessage(error);
            });
            co_return;
        }

        auto json = response.json().unwrapOrDefault();
        int newBalance = static_cast<int>(json["amethyst"].asInt().unwrapOr(0));
        std::string message = json["message"].asString().unwrapOr("Gift sent!");

        queueInMainThread([self, popup, newBalance, message] {
            Mod::get()->setSavedValue("amethyst", newBalance);
            if (auto shop = CBShopLayer::getInstance()) shop->updateAmethystLabel(newBalance);
            if (popup && popup->getParent()) popup->showSuccessMessage(message);
            if (self->getParent()) self->onClose(nullptr);
        });
        co_return;
    });
}
