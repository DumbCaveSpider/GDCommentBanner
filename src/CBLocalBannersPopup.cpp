#include "CBLocalBannersPopup.hpp"
#include "CBSubmitBannerPopup.hpp"
#include "CBBannerCell.hpp"
#include "include/CBLocalBanner.hpp"
#include <Geode/ui/Scrollbar.hpp>
#include <Geode/ui/Button.hpp>
#include <algorithm>

using namespace geode::prelude;

CBLocalBannersPopup* CBLocalBannersPopup::s_instance = nullptr;

CBLocalBannersPopup* CBLocalBannersPopup::getInstance() {
    return s_instance;
}

CBLocalBannersPopup* CBLocalBannersPopup::create() {
    auto ret = new CBLocalBannersPopup();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBLocalBannersPopup::init() {
    if (!Popup::init(380.f, 280.f)) return false;

    Ref<CBLocalBannersPopup> s_instance = this;
    this->setTitle("Local Banners");

    m_list = cue::ListNode::create({340.f, 190.f}, {0, 0, 0, 0}, cue::ListBorderStyle::Comments);
    if (m_list) {
        m_mainLayer->addChildAtPosition(m_list, Anchor::Center, {0.f, 10.f}, false);

        auto scrollbar = Scrollbar::create(m_list->getScrollLayer());
        m_mainLayer->addChildAtPosition(scrollbar, Anchor::Center, {340.f / 2 + 10.f, 10.f}, false);
    }

    m_noBannersLabel = CCLabelBMFont::create("No Local Banners Found", "goldFont.fnt");
    m_noBannersLabel->setScale(0.6f);
    m_noBannersLabel->setVisible(false);
    m_mainLayer->addChildAtPosition(m_noBannersLabel, Anchor::Center, {0.f, 10.f}, false);

    auto addBtn = geode::Button::createWithNode(
        ButtonSprite::create("Add Banner", "goldFont.fnt", "GJ_button_01.png", .8f),
        [this](geode::Button* sender) {
            this->onAddBanner(sender);
        });
    m_buttonMenu->addChildAtPosition(addBtn, Anchor::Bottom, {0.f, 25.f});

    fetchBanners();

    return true;
}

CBLocalBannersPopup::~CBLocalBannersPopup() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void CBLocalBannersPopup::onAddBanner(CCObject* sender) {
    if (auto popup = CBSubmitBannerPopup::create(true)) {
        popup->show();
    }
}

void CBLocalBannersPopup::fetchBanners() {
    if (m_list) {
        m_list->clear();
    }

    auto banners = comment::local::getLocalBanners();
    if (banners.empty()) {
        if (m_noBannersLabel) m_noBannersLabel->setVisible(true);
        if (m_list) m_list->updateLayout();
        return;
    }

    if (m_noBannersLabel) m_noBannersLabel->setVisible(false);

    std::stable_sort(banners.begin(), banners.end(), [](const auto& a, const auto& b) {
        return a.equipped > b.equipped;
    });

    for (auto const& b : banners) {
        CBBannerItem item;
        item.id = b.id;
        item.name = b.name;
        item.imageUrl = (comment::local::getLocalBannersDir() / b.filename).string();
        item.equipped = b.equipped;
        item.isLocal = true;
        item.owns = true;

        if (auto cell = CBBannerCell::create(item, 340.f, true)) {
            if (m_list) {
                m_list->addCell(cell);
            }
        }
    }
    if (m_list) {
        m_list->updateLayout();
    }
}
