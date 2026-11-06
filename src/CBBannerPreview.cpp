#include "CBBannerPreview.hpp"
#include "CBBannerCell.hpp"
#include "include/CBCommentPreview.hpp"
#include <Geode/ui/Toggler.hpp>

CBBannerPreview* CBBannerPreview::create(std::string const& imageUrl, std::string const& nonCompactImageUrl, CCSize const& maxSize) {
    auto ret = new CBBannerPreview();
    if (ret && ret->init(imageUrl, nonCompactImageUrl, maxSize)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBBannerPreview::init(std::string const& imageUrl, std::string const& nonCompactImageUrl, CCSize const& maxSize) {
    if (!CCNode::init()) return false;
    this->setAnchorPoint({0.5f, 0.5f});

    bool overlay = CBBannerCell::isPreviewOverlay();
    CCSize size = CCSizeZero;
    for (bool compact : {false, true}) {
        auto preview = comment::createCommentPreview(imageUrl, compact, comment::kCommentCellWidth, nonCompactImageUrl, overlay);
        preview->setScale(std::min(maxSize.width / preview->getContentWidth(), maxSize.height / preview->getContentHeight()));
        this->addChild(preview);
        size.width = std::max(size.width, preview->getScaledContentWidth());
        size.height = std::max(size.height, preview->getScaledContentHeight());
        (compact ? m_compactPreview : m_normalPreview) = preview;
    }

    this->setContentSize(size);
    m_normalPreview->setPosition(size / 2.f);
    m_compactPreview->setPosition(size / 2.f);
    this->setCompact(CBBannerCell::isPreviewCompact());
    return true;
}

void CBBannerPreview::setCompact(bool compact) {
    m_normalPreview->setVisible(!compact);
    m_compactPreview->setVisible(compact);
}

void CBBannerPreview::setOverlay(bool overlay) {
    comment::setCommentPreviewOverlay(m_normalPreview, overlay);
    comment::setCommentPreviewOverlay(m_compactPreview, overlay);
}

CCMenu* CBBannerPreview::createToggles() {
    auto togglesMenu = CCMenu::create();
    togglesMenu->ignoreAnchorPointForPosition(false);
    togglesMenu->setAnchorPoint({1.f, 1.f});
    togglesMenu->setContentSize({90.f, 36.f});
    togglesMenu->setLayout(ColumnLayout::create()
            ->setGap(0.f)
            ->setAxisReverse(true)
            ->setAutoScale(false)
            ->setAxisAlignment(AxisAlignment::End)
            ->setCrossAxisAlignment(AxisAlignment::End)
            ->setCrossAxisLineAlignment(AxisAlignment::End));

    auto addToggle = [&](char const* text, bool toggled, Toggler::ToggleCallback callback) {
        auto row = CCMenu::create();
        row->setContentSize({90.f, 18.f});
        row->setLayout(RowLayout::create()->setGap(3.f)->setAutoScale(false)->setAxisAlignment(AxisAlignment::End));

        auto label = Label::create(text, "bigFont.fnt");
        label->setLimitLabelWidth(65.f, 0.3f, 0.1f);
        row->addChild(label);

        auto toggler = Toggler::createWithStandardSprites(0.45f, std::move(callback));
        toggler->setToggled(toggled);
        row->addChild(toggler);

        row->updateLayout();
        togglesMenu->addChild(row);
    };

    // a weak reference: the toggles usually live next to the preview, not inside it
    WeakRef<CBBannerPreview> self = this;
    addToggle("Non-Compact", !CBBannerCell::isPreviewCompact(), [self](Toggler*, bool toggled) {
        CBBannerCell::setPreviewCompact(!toggled);
        if (auto preview = self.lock()) preview->setCompact(!toggled);
    });
    addToggle("Comment Overlay", CBBannerCell::isPreviewOverlay(), [self](Toggler*, bool toggled) {
        CBBannerCell::setPreviewOverlay(toggled);
        if (auto preview = self.lock()) preview->setOverlay(toggled);
    });

    togglesMenu->updateLayout();
    return togglesMenu;
}
