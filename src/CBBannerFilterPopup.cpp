#include "CBBannerFilterPopup.hpp"
#include "CBShopLayer.hpp"
#include "include/CBConstant.hpp"
#include <Geode/ui/Label.hpp>
#include <array>

using namespace geode::prelude;

namespace {
    // filter/sort states, which are sent to /getBanners.
    constexpr std::array kFilterNames = {"All", "Owned", "Unowned", "Limited", "Featured"};
    constexpr std::array kSortNames = {"Recent", "Most Bought", "Most Equipped", "Highest Price", "Lowest Price"};

    constexpr float kRowWidth = 90.f;
    constexpr float kRowHeight = 26.f;

    CCMenu* createToggleMenu(size_t rows) {
        auto menu = CCMenu::create();
        menu->setAnchorPoint({0.5f, 0.5f});
        menu->setContentSize({kRowWidth, kRowHeight * rows});
        menu->setLayout(ColumnLayout::create()
                ->setGap(0.f)
                ->setAxisReverse(true)
                ->setAutoScale(false)
                ->setAxisAlignment(AxisAlignment::End));
        return menu;
    }

    Toggler* addToggle(CCMenu* menu, char const* text, bool toggled, Toggler::ToggleCallback callback) {
        auto row = CCNode::create();
        row->setAnchorPoint({0.5f, 0.5f});
        row->setContentSize({kRowWidth, kRowHeight});
        row->setLayout(RowLayout::create()
                ->setAxisAlignment(AxisAlignment::Start)
                ->setGap(5.f)
                ->setAutoScale(false));

        auto toggler = Toggler::createWithStandardSprites(0.6f, std::move(callback));
        toggler->setToggled(toggled);
        row->addChild(toggler);

        auto label = Label::create(text, "bigFont.fnt");
        label->setLimitLabelWidth(60.f, 0.4f, 0.1f);
        row->addChild(label);

        row->updateLayout();
        menu->addChild(row);
        return toggler;
    }

    void selectOnly(std::vector<Toggler*> const& group, Toggler* selected) {
        for (auto toggler : group) {
            toggler->setToggled(toggler == selected);
        }
    }
}

CBBannerFilterPopup* CBBannerFilterPopup::create() {
    auto ret = new CBBannerFilterPopup();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBBannerFilterPopup::init() {
    auto shop = CBShopLayer::getInstance();
    if (!shop) return false;

    if (!Popup::init(260.f, 240.f)) return false;
    addSideArt(m_mainLayer, SideArt::All, SideArtStyle::PopupGold, false);
    this->setTitle("Filter Banners");

    auto filterHeader = Label::create("Filter", "goldFont.fnt");
    filterHeader->setScale(0.5f);
    m_mainLayer->addChildAtPosition(filterHeader, Anchor::Center, {-65.f, 75.f}, false);

    auto sortHeader = Label::create("Sort", "goldFont.fnt");
    sortHeader->setScale(0.5f);
    m_mainLayer->addChildAtPosition(sortHeader, Anchor::Center, {65.f, 75.f}, false);

    // one extra row for Supporter Only
    // several filters can be on at once (bit i = filter i), "All" clears them
    auto filterMenu = createToggleMenu(kFilterNames.size() + 1);
    int filters = shop->getFilterState();
    for (int i = 0; i < static_cast<int>(kFilterNames.size()); ++i) {
        bool toggled = i == 0 ? filters == 0 : (filters & (1 << i)) != 0;
        m_filterToggles.push_back(addToggle(filterMenu, kFilterNames[i], toggled,
            [this, i](Toggler*, bool) { this->onFilterToggled(i); }));
    }

    // Supporter Only, combines with the filter above (not part of the select-one group)
    m_supporterOnlyToggle = addToggle(filterMenu, "Supporter", shop->getSupporterOnlyFilter(),
        [](Toggler* sender, bool) {
            if (auto shop = CBShopLayer::getInstance()) {
                shop->setSupporterOnlyFilter(!shop->getSupporterOnlyFilter());
            }
        });
    if (!comment::isUserSupporter()) {
        // hide the whole row (toggle + label)
        m_supporterOnlyToggle->getParent()->setVisible(false);
        shop->setSupporterOnlyFilter(false);
    }

    filterMenu->updateLayout();
    // anchored at the top so the extra row extends downwards, level with the sort menu's top
    filterMenu->setAnchorPoint({0.5f, 1.f});
    m_mainLayer->addChildAtPosition(filterMenu, Anchor::Center, {-65.f, 60.f}, false);

    auto sortMenu = createToggleMenu(kSortNames.size());
    for (int i = 0; i < static_cast<int>(kSortNames.size()); ++i) {
        m_sortToggles.push_back(addToggle(sortMenu, kSortNames[i], i == shop->getSortState(),
            [this, i](Toggler* sender, bool) {
                selectOnly(m_sortToggles, sender);
                if (auto shop = CBShopLayer::getInstance()) {
                    shop->setSortState(i);
                }
            }));
    }
    sortMenu->updateLayout();
    m_mainLayer->addChildAtPosition(sortMenu, Anchor::Center, {65.f, -5.f}, false);

    auto resetButton = Button::createWithNode(
        ButtonSprite::create("Reset", "goldFont.fnt", "GJ_button_05.png", .7f),
        [this](Button* sender) {
            selectOnly(m_filterToggles, m_filterToggles.front());
            selectOnly(m_sortToggles, m_sortToggles.front());
            m_supporterOnlyToggle->setToggled(false);
            if (auto shop = CBShopLayer::getInstance()) {
                shop->resetFilterSort();
            }
        });
    m_mainLayer->addChildAtPosition(resetButton, Anchor::Bottom, {0.f, 25.f}, false);

    return true;
}

void CBBannerFilterPopup::onFilterToggled(int index) {
    auto shop = CBShopLayer::getInstance();
    if (!shop) return;

    int filters = shop->getFilterState();
    if (index == 0) {
        filters = 0;  // All
    } else {
        filters ^= (1 << index);
        // Owned and Unowned can't both match, picking one drops the other
        if (index == 1 && (filters & (1 << 1))) filters &= ~(1 << 2);
        if (index == 2 && (filters & (1 << 2))) filters &= ~(1 << 1);
    }

    for (int i = 0; i < static_cast<int>(m_filterToggles.size()); ++i) {
        m_filterToggles[i]->setToggled(i == 0 ? filters == 0 : (filters & (1 << i)) != 0);
    }
    shop->setFilterState(filters);
}
