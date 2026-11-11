#include "../include/CBCommentPreview.hpp"
#include "../include/CBConstant.hpp"
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/ui/NineSlice.hpp>
#include <cue/PlayerIcon.hpp>

using namespace geode::prelude;

// me when i steal the CommentCell code from the decomp HAHAHAHAHA
// rob does the same for all other geode mod devs too so i guess this is fair game

namespace comment {
    CCNode* createCommentPreview(std::string const& imageUrl, bool compact, float width, std::string const& nonCompactImageUrl, bool showOverlay, bool priorityLoad) {
        float height = compact ? kCompactCommentCellHeight : kCommentCellHeight;
        CCSize size{width, height};

        auto clip = CCClippingNode::create(CCLayerColor::create({255, 255, 255, 255}, width, height));
        clip->setContentSize(size);
        clip->setAnchorPoint({0.5f, 0.5f});

        auto overlay = CCNode::create();
        overlay->setID(kCommentPreviewOverlayID);
        overlay->setContentSize(size);
        clip->addChild(overlay, 1);

        auto bgColor = compact ? ccColor3B{156, 85, 42} : ccColor3B{161, 88, 44};
        auto bgLayer = CCLayerColor::create({bgColor.r, bgColor.g, bgColor.b, 255}, width, height);
        overlay->addChild(bgLayer, -1);

        bool useNonCompact = !compact && !nonCompactImageUrl.empty();
        if (!imageUrl.empty() || useNonCompact) {
            auto bannerSize = (compact || useNonCompact) ? size : CCSize{800.f, 800.f};
            if (auto banner = createBannerNode(useNonCompact ? nonCompactImageUrl : imageUrl, bannerSize, priorityLoad)) {
                banner->setPosition(size / 2.f);
                bgLayer->setOpacity(100);
                clip->addChild(banner, 0);
            }
        }

        auto mainLayer = CCNode::create();
        mainLayer->setContentSize(size);
        overlay->addChild(mainLayer, 1);

        auto player = comment::createLocalPlayerIcon();
        player->setScale((compact ? 0.75f : 1.f) * 0.6f);
        player->setPosition({20.f, height - (compact ? 11.f : 19.5f)});
        mainLayer->addChild(player, 1);
        float iconSpace = 26.f;

        auto name = Label::create(comment::localUsername().c_str(), "goldFont.fnt");
        name->setLimitLabelWidth(150.f, compact ? 0.5f : 0.7f, 0.f);
        name->setAnchorPoint({0.f, 0.5f});
        name->setPosition({iconSpace + 10.f, height - (compact ? 10.f : 18.f)});
        mainLayer->addChild(name);

        if (!compact) {
            auto background = NineSlice::create("square02b_001.png");
            background->setContentSize({width - 10.f, height - 12.f});
            background->setPosition(size / 2.f);
            background->setColor({130, 64, 33});
            background->setOpacity(100);
            mainLayer->addChild(background, -1);
        }

        auto text = Label::create("This is how your banner looks in comments!", "chatFont.fnt");
        text->setAnchorPoint({0.f, 0.5f});
        if (compact) {
            text->setPosition({10.f, height * 0.5f - 9.f});
            text->setLimitLabelWidth(270.f, 0.7f, 0.f);

            auto textBg = NineSlice::create("square02_small.png");
            textBg->setInsets({5, 5, 5, 5});
            textBg->setContentSize(text->getScaledContentSize() + CCSize(5, 0));
            textBg->setAnchorPoint(text->getAnchorPoint());
            textBg->setPosition({text->getPositionX() - 2.f, text->getPositionY()});
            textBg->setOpacity(150);
            mainLayer->addChild(textBg, -1);
        } else {
            text->setPosition({10.f, height * 0.5f});
            text->setLimitLabelWidth(width - 45.f, 1.f, 0.f);
        }
        mainLayer->addChild(text);

        // likes
        float iconScale = compact ? 0.6f : 0.8f;
        float labelScale = compact ? 0.3f : 0.4f;
        float labelGap = compact ? 12.f : 13.f;
        CCPoint likePos{width - (compact ? 32.f : 39.f) - (compact ? 16.f : 20.f), height - (compact ? 14.f : 20.f)};

        auto likeLabel = Label::create("67", "bigFont.fnt");
        likeLabel->setPosition(likePos + CCPoint{(compact ? 8.f : 14.f) + labelGap, compact ? 1.f : 0.5f});
        likeLabel->setScale(labelScale);
        mainLayer->addChild(likeLabel);

        auto likeIcon = CCSprite::createWithSpriteFrameName("GJ_likesIcon_001.png");
        likeIcon->setScale(iconScale);
        likeIcon->setPosition(likePos + CCPoint{0.f, 2.f});
        mainLayer->addChild(likeIcon);

        auto dots = Label::create("...", "bigFont.fnt");
        dots->setScale(compact ? 0.4f : 0.5f);
        dots->setOpacity(120);
        dots->setPosition({width - (compact ? 10.f : 12.f), height - (compact ? 7.f : 9.f)});
        mainLayer->addChild(dots);

        auto age = Label::create("1 minute ago", "chatFont.fnt");
        age->setAnchorPoint({1.f, 0.5f});
        age->setScale(compact ? 0.4f : 0.55f);
        age->setColor({0, 0, 0});
        age->setOpacity(140);
        age->setPosition({width - (compact ? 8.f : 11.f), compact ? 7.f : 14.f});
        mainLayer->addChild(age, 1);

        comment::addCellLines(overlay, width, height);

        overlay->setVisible(showOverlay);
        return clip;
    }

    void setCommentPreviewOverlay(CCNode* preview, bool visible) {
        if (!preview) return;
        if (auto overlay = preview->getChildByID(kCommentPreviewOverlayID)) {
            overlay->setVisible(visible);
        }
    }
}
