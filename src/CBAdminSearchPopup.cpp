#include "CBAdminSearchPopup.hpp"

CBAdminSearchPopup* CBAdminSearchPopup::create(std::string title, std::string placeholder, std::function<void(std::string)> callback) {
    auto ret = new CBAdminSearchPopup();
    if (ret && ret->init(title, placeholder, callback)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBAdminSearchPopup::init(std::string title, std::string placeholder, std::function<void(std::string)> callback) {
    if (!Popup::init(300.f, 150.f, "GJ_square02.png")) return false;

    m_callback = callback;

    this->setTitle(title.c_str());
    addSideArt(m_mainLayer, SideArt::All, SideArtStyle::PopupBlue);

    auto menu = CCMenu::create();
    menu->setPosition({m_mainLayer->getContentSize().width / 2, m_mainLayer->getContentSize().height / 2 - 10.f});
    m_mainLayer->addChild(menu);

    m_input = TextInput::create(250.f, placeholder.c_str());
    m_input->setPosition({0, 15.f});
    menu->addChild(m_input);

    auto confirmBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Submit", "goldFont.fnt", "GJ_button_01.png", 0.8f),
        this,
        menu_selector(CBAdminSearchPopup::onConfirm));
    confirmBtn->setPosition({0, -30.f});
    menu->addChild(confirmBtn);

    return true;
}

void CBAdminSearchPopup::onConfirm(CCObject*) {
    if (m_callback) {
        m_callback(m_input->getString());
    }
    this->onClose(nullptr);
}
