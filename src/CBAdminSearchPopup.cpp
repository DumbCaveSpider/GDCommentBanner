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

    m_input = TextInput::create(250.f, placeholder.c_str());
    m_input->setPosition({0, 15.f});
    m_mainLayer->addChildAtPosition(m_input, Anchor::Center, CCPointZero, false);

    auto confirmBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Submit", "goldFont.fnt", "GJ_button_01.png"),
        this,
        menu_selector(CBAdminSearchPopup::onConfirm));
    m_buttonMenu->addChildAtPosition(confirmBtn, Anchor::Bottom, {0.f, 25.f}, false);

    return true;
}

void CBAdminSearchPopup::onConfirm(CCObject*) {
    if (m_callback) {
        m_callback(m_input->getString());
    }
    this->onClose(nullptr);
}
