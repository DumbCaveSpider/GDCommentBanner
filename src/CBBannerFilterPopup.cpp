#include "CBBannerFilterPopup.hpp"
#include "CBShopLayer.hpp"
#include "include/CBConstant.hpp"
#include <Geode/ui/Label.hpp>
#include <array>

using namespace geode::prelude;

constexpr std::array kFilterNames = {"All", "Owned", "Unowned", "Limited", "Featured", "Favorite"};
constexpr std::array kSortNames = {"Recent", "Most Bought", "Most Equipped", "Highest Price", "Lowest Price"};

constexpr float kRowWidth = 90.f;

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

    if (!Popup::init(260.f, 250.f)) return false;
    addSideArt(m_mainLayer, SideArt::All, SideArtStyle::PopupGold, false);
    this->setTitle("Filter Banners");

    auto filterHeader = Label::create("Filter", "goldFont.fnt");
    filterHeader->setScale(0.5f);
    m_mainLayer->addChildAtPosition(filterHeader, Anchor::Center, {-65.f, 85.f}, false);

    auto sortHeader = Label::create("Sort", "goldFont.fnt");
    sortHeader->setScale(0.5f);
    m_mainLayer->addChildAtPosition(sortHeader, Anchor::Center, {65.f, 85.f}, false);

    auto filterMenu = comment::createToggleMenu(kRowWidth, kFilterNames.size() + 1);
    int filters = shop->getFilterState();
    for (int i = 0; i < static_cast<int>(kFilterNames.size()); ++i) {
        bool toggled = i == 0 ? filters == 0 : (filters & (1 << i)) != 0;
        m_filterToggles.push_back(comment::addToggle(filterMenu, kRowWidth, kFilterNames[i], toggled, [this, i](Toggler*, bool) { this->onFilterToggled(i); }));
    }

    m_supporterOnlyToggle = comment::addToggle(filterMenu, kRowWidth, "Supporter", shop->getSupporterOnlyFilter(), [](Toggler* sender, bool) {
        if (auto shop = CBShopLayer::getInstance()) {
            shop->setSupporterOnlyFilter(!shop->getSupporterOnlyFilter());
        }
    });
    if (!comment::isUserSupporter()) {
        m_supporterOnlyToggle->getParent()->setVisible(false);
        shop->setSupporterOnlyFilter(false);
    }

    filterMenu->updateLayout();
    m_mainLayer->addChildAtPosition(filterMenu, Anchor::Center, {-65.f, 70.f}, false);

    auto sortMenu = comment::createToggleMenu(kRowWidth, kSortNames.size());
    for (int i = 0; i < static_cast<int>(kSortNames.size()); ++i) {
        m_sortToggles.push_back(comment::addToggle(sortMenu, kRowWidth, kSortNames[i], i == shop->getSortState(), [this, i](Toggler* sender, bool) {
            comment::selectOnly(m_sortToggles, sender);
            if (auto shop = CBShopLayer::getInstance()) {
                shop->setSortState(i);
            }
        }));
    }
    sortMenu->updateLayout();
    m_mainLayer->addChildAtPosition(sortMenu, Anchor::Center, {65.f, 70.f}, false);

    auto resetButton = Button::createWithNode(
        ButtonSprite::create("Reset", "goldFont.fnt", "GJ_button_05.png", .7f),
        [this](Button* sender) {
            comment::selectOnly(m_filterToggles, m_filterToggles.front());
            comment::selectOnly(m_sortToggles, m_sortToggles.front());
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
        filters = 0;
    } else {
        filters ^= (1 << index);
        if (index == 1 && (filters & (1 << 1))) filters &= ~(1 << 2);
        if (index == 2 && (filters & (1 << 2))) filters &= ~(1 << 1);
    }

    for (int i = 0; i < static_cast<int>(m_filterToggles.size()); ++i) {
        m_filterToggles[i]->setToggled(i == 0 ? filters == 0 : (filters & (1 << i)) != 0);
    }
    shop->setFilterState(filters);
}
