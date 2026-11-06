#include "CBLogsCell.hpp"
#include <Geode/ui/TextArea.hpp>

#if defined(GEODE_IS_WINDOWS) || defined(GEODE_IS_ANDROID)
#include <Geode/binding/EmoteSprite.hpp>
#define CB_HAS_EMOTES 1
#endif

namespace {
    constexpr float kIconSize = 18.f;
    constexpr float kTextX = 36.f; 
    constexpr float kMinHeight = 40.f;
    constexpr int kMaxMessageLines = 3;

    struct LogStyle {
        char const* emote; 
        char const* sprite;
        ccColor3B color;
    };

    LogStyle styleForType(std::string const& type) {
        static ccColor3B const green{100, 255, 100};
        static ccColor3B const red{255, 100, 100};
        static ccColor3B const gold{255, 200, 100};
        static ccColor3B const pink{255, 150, 255};

        if (type == "APPROVED") return {"party", "GJ_completesIcon_001.png", green};
        if (type == "REJECTED") return {"mail", "GJ_deleteIcon_001.png", red};
        if (type == "BANNER_SOLD") return {"trophy", "GJ_sStarsIcon_001.png", green};
        if (type == "BANNER_PAYOUT" || type == "AMETHYST_REWARD" || type == "ADMIN_AMETHYST_CHANGE" || type == "AMETHYST_GIFT_RECEIVED")
            return {"chest", "GJ_sStarsIcon_001.png", pink};
        if (type == "FEATURED") return {"star", "GJ_starsIcon_001.png", gold};
        if (type == "UNFEATURED") return {"star", "GJ_starsIcon_001.png", {200, 200, 200}};
        if (type == "BANNER_REFUNDED") return {"download", "GJ_sStarsIcon_001.png", gold};
        if (type == "SUPPORTER_LINKED") return {"royal", "gj_heartOn_001.png", pink};
        if (type == "BANNER_DELETED" || type == "BANNER_REMOVED") return {"mail", "GJ_deleteIcon_001.png", red};
        if (type == "TEMP_BAN" || type == "PERMANENT_BAN") return {"mail", "GJ_deleteIcon_001.png", red};
        if (type == "BANNER_EDITED" || type == "BANNER_CHANGED" || type == "BANNER_UNEQUIPPED")
            return {"cube", "GJ_infoIcon_001.png", gold};
        return {"mail", "GJ_infoIcon_001.png", gold};
    }

    void fitLines(SimpleTextArea* area, std::string const& text) {
        if (area->getLines().size() <= kMaxMessageLines) return;
        size_t length = text.size();
        while (area->getLines().size() > kMaxMessageLines && length > 4) {
            length -= 4;
            area->setText(text.substr(0, length) + "...");
        }
    }
}

CBLogsCell* CBLogsCell::create(CBLogEntry const& entry, float width) {
    auto ret = new CBLogsCell();
    if (ret && ret->init(entry, width)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

std::string CBLogsCell::titleForType(std::string const& actionType) {
    if (actionType.empty()) return "Notification";
    std::string title;
    bool upper = true;
    for (char c : actionType) {
        if (c == '_') {
            title += ' ';
            upper = true;
        } else {
            title += upper ? static_cast<char>(std::toupper(c)) : static_cast<char>(std::tolower(c));
            upper = false;
        }
    }
    return title;
}

CCNode* CBLogsCell::createIcon(std::string const& actionType) {
    auto style = styleForType(actionType);
    CCNode* icon = nullptr;

#ifdef CB_HAS_EMOTES
    // only works once GD has loaded its emotes, otherwise falls back to the sprite below
    icon = EmoteSprite::createWithName(style.emote);
#endif
    if (!icon) icon = CCSprite::createWithSpriteFrameName(style.sprite);
    if (!icon) icon = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
    if (!icon) return nullptr;

    // like EmoteSprite::fitToHeight, scale to the icon height
    auto size = icon->getContentSize();
    float largest = std::max(size.width, size.height);
    if (largest > 0.f) icon->setScale(kIconSize / largest);
    return icon;
}

bool CBLogsCell::init(CBLogEntry const& entry, float width) {
    if (!CCNode::init()) return false;
    m_entry = entry;

    auto style = styleForType(entry.actionType);
    float textWidth = width - kTextX - 14.f;

    // message, measured first so the cell can grow to fit it
    auto body = SimpleTextArea::create(entry.message.c_str(), "chatFont.fnt", 0.54f, textWidth);
    body->setAlignment(kCCTextAlignmentLeft);
    body->setAnchorPoint({0.f, 1.f});
    fitLines(body, entry.message);

    float bodyHeight = body->getScaledContentSize().height;
    float height = std::max(kMinHeight, bodyHeight + 30.f);
    this->setContentSize({width, height});

    float top = height - 15.f;
    body->setPosition({kTextX, top - 6.f});
    this->addChild(body);

    if (auto icon = createIcon(entry.actionType)) {
        icon->setPosition({18.f, top - 4.f});
        this->addChild(icon, 1);
    }

    auto title = Label::create(titleForType(entry.actionType).c_str(), "bigFont.fnt");
    title->setLimitLabelWidth(textWidth - 60.f, 0.5f, 0.2f);
    title->setAnchorPoint({0.f, 0.5f});
    title->setColor(style.color);
    title->setPosition({kTextX, top + 3.f});
    this->addChild(title, 1);

    // date next to the title, in place of the "x ago" label
    float titleWidth = title->getScaledContentWidth();
    auto age = Label::create(fmt::format("#{} - {}", entry.id, entry.createdAt).c_str(), "chatFont.fnt");
    age->setAnchorPoint({0.f, 0.5f});
    age->setScale(0.5f);
    age->setColor({0, 0, 0});
    age->setOpacity(150);
    age->setPosition({kTextX + titleWidth + 6.f, top + 2.f});
    this->addChild(age, 1);

    return true;
}
