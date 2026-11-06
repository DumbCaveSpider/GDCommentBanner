#include "CBPreviewBanner.hpp"
#include "include/CBCommentPreview.hpp"
#include <Geode/ui/NineSlice.hpp>

CBPreviewBanner* CBPreviewBanner::create(std::string const& compactImage, std::string const& nonCompactImage) {
    auto ret = new CBPreviewBanner();
    if (ret && ret->init(compactImage, nonCompactImage)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBPreviewBanner::init(std::string const& compactImage, std::string const& nonCompactImage) {
    if (!CCBlockLayer::init()) return false;
    if (compactImage.empty() && nonCompactImage.empty()) return false;

    auto winSize = CCDirector::sharedDirector()->getWinSize();

    auto menu = CCMenu::create();
    menu->setZOrder(1);
    auto closeBtn = Button::createWithSpriteFrameName("GJ_arrow_01_001.png", [this](Button*) { this->close(); });
    menu->addChildAtPosition(closeBtn, Anchor::TopLeft, {40.f, -35.f});
    this->addChild(menu);

    // bg for the preview
    auto previewBg = NineSlice::create("square02_001.png");
    previewBg->setContentSize(winSize - CCSize{30.f, 20.f});
    previewBg->setPosition(winSize / 2);
    previewBg->setOpacity(150);
    this->addChild(previewBg, -1);

    m_compactImage = compactImage;
    m_nonCompactImage = nonCompactImage;

    m_container = CCNode::create();
    m_container->setContentSize({comment::kCommentCellWidth, 200.f});
    m_container->setAnchorPoint({0.5f, 0.5f});
    m_container->setPosition(winSize / 2);
    m_container->setLayout(ColumnLayout::create()
            ->setGap(8.f)
            ->setAutoScale(false)
            ->setAxisAlignment(AxisAlignment::End));
    this->addChild(m_container);
    buildPreviews();

    // "Comment Overlay" toggle: green = comment shown on top, grey = just the banner image
    auto overlayToggle = Toggler::create(
        ButtonSprite::create("Comment Overlay", "goldFont.fnt", "GJ_button_04.png", .7f),
        ButtonSprite::create("Comment Overlay", "goldFont.fnt", "GJ_button_01.png", .7f),
        [this](Toggler*, bool toggled) {
            // no rebuild: the banners stay loaded, only the comment on top is shown/hidden
            m_showOverlay = toggled;
            for (auto preview : m_previews) comment::setCommentPreviewOverlay(preview, toggled);
        });
    overlayToggle->setToggled(m_showOverlay);
    menu->addChildAtPosition(overlayToggle, Anchor::Bottom, {0.f, 35.f});

    return true;
}

void CBPreviewBanner::buildPreviews() {
    m_container->removeAllChildren();
    m_previews.clear();

    // mockup of the in-game comment cell, banner applied the same way as CBHook.
    // priority load: the preview covers the screen, it shouldn't wait for banners behind it
    auto addPreview = [&](bool compact, char const* title) {
        auto preview = comment::createCommentPreview(
            m_compactImage, compact, comment::kCommentCellWidth, m_nonCompactImage, m_showOverlay, true);
        m_previews.push_back(preview);
        m_container->addChild(preview);
        auto label = Label::create(title, "goldFont.fnt");
        label->setScale(0.45f);
        m_container->addChild(label);
    };
    addPreview(false, "Non-Compact Mode");
    addPreview(true, "Compact Mode");

    m_container->updateLayout();
}

void CBPreviewBanner::show() {
    if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
        // above any open popups
        this->setZOrder(std::max(scene->getHighestChildZ() + 1, 200));
        scene->addChild(this);
    }
}

void CBPreviewBanner::close() {
    this->removeFromParent();
}

void CBPreviewBanner::keyBackClicked() {
    this->close();
}
