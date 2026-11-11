#include "CBLogsCell.hpp"
#include "include/CBConstant.hpp"
#include <Geode/ui/TextArea.hpp>
#include <cctype>

#if defined(GEODE_IS_WINDOWS) || defined(GEODE_IS_ANDROID)
#include <Geode/binding/EmoteSprite.hpp>
#define CB_HAS_EMOTES 1
#endif

constexpr float kTextX = 36.f;
constexpr float kMinHeight = 40.f;

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

    auto words = utils::string::split(actionType, "_");
    for (auto& word : words) {
        utils::string::toLowerIP(word);
        if (!word.empty()) {
            word.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(word.front())));
        }
    }
    return utils::string::join(words, " ");
}

CCNode* CBLogsCell::createIcon(std::string const& actionType) {
    auto style = comment::logStyleForType(actionType);
    CCNode* icon = nullptr;

#ifdef CB_HAS_EMOTES
    icon = EmoteSprite::createWithName(style.emote);
#endif
    if (!icon) icon = CCSprite::createWithSpriteFrameName(style.sprite);
    if (!icon) icon = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
    if (!icon) return nullptr;

    auto size = icon->getContentSize();
    float largest = std::max(size.width, size.height);
    if (largest > 0.f) icon->setScale(18.f / largest);
    return icon;
}

bool CBLogsCell::init(CBLogEntry const& entry, float width) {
    if (!CCNode::init()) return false;
    m_entry = entry;

    auto style = comment::logStyleForType(entry.actionType);
    float textWidth = width - kTextX - 14.f;

    auto body = SimpleTextArea::create(entry.message.c_str(), "chatFont.fnt", 0.54f, textWidth);
    body->setAlignment(kCCTextAlignmentLeft);
    body->setAnchorPoint({0.f, 1.f});
    body->setMaxLines(3);

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
