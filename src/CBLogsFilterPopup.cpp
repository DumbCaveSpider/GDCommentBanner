#include "CBLogsFilterPopup.hpp"
#include "include/CBConstant.hpp"
#include <Geode/ui/Label.hpp>

constexpr float kRowWidth = 110.f;

CBLogsFilterPopup* CBLogsFilterPopup::create(CBLogsFilter filter, std::function<void(CBLogsFilter const&)> onChange) {
    auto ret = new CBLogsFilterPopup();
    if (ret && ret->init(std::move(filter), std::move(onChange))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBLogsFilterPopup::init(CBLogsFilter filter, std::function<void(CBLogsFilter const&)> onChange) {
    if (!Popup::init(280.f, 230.f)) return false;
    m_filter = std::move(filter);
    m_onChange = std::move(onChange);

    addSideArt(m_mainLayer, SideArt::All, SideArtStyle::PopupGold, false);
    this->setTitle("Filter Logs");

    auto typeHeader = Label::create("Type", "goldFont.fnt");
    typeHeader->setScale(0.5f);
    m_mainLayer->addChildAtPosition(typeHeader, Anchor::Center, {-65.f, 70.f}, false);

    auto orderHeader = Label::create("Order", "goldFont.fnt");
    orderHeader->setScale(0.5f);
    m_mainLayer->addChildAtPosition(orderHeader, Anchor::Center, {70.f, 70.f}, false);

    // types: any combination, none selected shows everything
    auto const& categories = comment::getLogCategories();
    auto typeMenu = comment::createToggleMenu(kRowWidth, categories.size());
    for (int i = 0; i < static_cast<int>(categories.size()); ++i) {
        m_typeToggles.push_back(comment::addToggle(typeMenu, kRowWidth, categories[i].name, m_filter.categories.contains(i), [this, i](Toggler*, bool toggled) {
            if (toggled) {
                m_filter.categories.insert(i);
            } else {
                m_filter.categories.erase(i);
            }
            this->apply();
        }));
    }
    typeMenu->updateLayout();
    m_mainLayer->addChildAtPosition(typeMenu, Anchor::Center, {-65.f, 58.f}, false);

    // order: one of the two
    auto orderMenu = comment::createToggleMenu(kRowWidth, 2);
    char const* orderNames[] = {"Newest First", "Oldest First"};
    for (int i = 0; i < 2; ++i) {
        bool oldest = i == 1;
        m_orderToggles.push_back(comment::addToggle(orderMenu, kRowWidth, orderNames[i], m_filter.oldestFirst == oldest, [this, oldest](Toggler* sender, bool) {
            for (auto toggler : m_orderToggles) toggler->setToggled(toggler == sender);
            m_filter.oldestFirst = oldest;
            this->apply();
        }));
    }
    orderMenu->updateLayout();
    m_mainLayer->addChildAtPosition(orderMenu, Anchor::Center, {70.f, 58.f}, false);

    auto resetButton = Button::createWithNode(
        ButtonSprite::create("Reset", "goldFont.fnt", "GJ_button_05.png", .7f),
        [this](Button*) {
            for (auto toggler : m_typeToggles) toggler->setToggled(false);
            for (size_t i = 0; i < m_orderToggles.size(); ++i) m_orderToggles[i]->setToggled(i == 0);
            m_filter = {};
            this->apply();
        });
    m_mainLayer->addChildAtPosition(resetButton, Anchor::Bottom, {0.f, 25.f}, false);

    return true;
}

void CBLogsFilterPopup::apply() {
    if (m_onChange) m_onChange(m_filter);
}
