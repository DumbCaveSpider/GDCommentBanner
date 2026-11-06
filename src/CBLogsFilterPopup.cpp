#include "CBLogsFilterPopup.hpp"
#include <Geode/ui/Label.hpp>

namespace {
    constexpr float kRowWidth = 110.f;
    constexpr float kRowHeight = 26.f;

    CCMenu* createToggleMenu(size_t rows) {
        auto menu = CCMenu::create();
        menu->setAnchorPoint({0.5f, 1.f});
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
        label->setLimitLabelWidth(kRowWidth - 30.f, 0.4f, 0.1f);
        row->addChild(label);

        row->updateLayout();
        menu->addChild(row);
        return toggler;
    }
}

std::vector<CBLogCategory> const& getLogCategories() {
    static std::vector<CBLogCategory> const categories = {
        {"Reviews", {"APPROVED", "REJECTED"}},
        {"Banners", {"BANNER_EDITED", "BANNER_CHANGED", "BANNER_DELETED", "BANNER_REMOVED", "BANNER_UNEQUIPPED", "FEATURED", "UNFEATURED"}},
        {"Sales", {"BANNER_SOLD", "BANNER_REFUNDED"}},
        {"Amethysts", {"BANNER_PAYOUT", "AMETHYST_REWARD", "ADMIN_AMETHYST_CHANGE", "AMETHYST_GIFT_SENT", "AMETHYST_GIFT_RECEIVED"}},
        {"Account", {"SUPPORTER_LINKED", "TEMP_BAN", "PERMANENT_BAN"}},
    };
    return categories;
}

std::vector<std::string> getLogTypesFor(std::set<int> const& categories) {
    std::vector<std::string> types;
    auto const& all = getLogCategories();
    for (int index : categories) {
        if (index < 0 || index >= static_cast<int>(all.size())) continue;
        for (auto type : all[index].types) types.emplace_back(type);
    }
    return types;
}

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
    auto const& categories = getLogCategories();
    auto typeMenu = createToggleMenu(categories.size());
    for (int i = 0; i < static_cast<int>(categories.size()); ++i) {
        m_typeToggles.push_back(addToggle(typeMenu, categories[i].name, m_filter.categories.contains(i), [this, i](Toggler*, bool toggled) {
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
    auto orderMenu = createToggleMenu(2);
    char const* orderNames[] = {"Newest First", "Oldest First"};
    for (int i = 0; i < 2; ++i) {
        bool oldest = i == 1;
        m_orderToggles.push_back(addToggle(orderMenu, orderNames[i], m_filter.oldestFirst == oldest, [this, oldest](Toggler* sender, bool) {
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
