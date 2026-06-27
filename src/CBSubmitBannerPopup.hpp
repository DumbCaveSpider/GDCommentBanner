#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

class CBSubmitBannerPopup : public geode::Popup {
public:
    static CBSubmitBannerPopup* create();

private:
    bool init();
    void onPickFile(CCObject*);
    void onSubmit(CCObject*);
    void onToggleLimited(CCObject*);

    geode::TextInput* m_nameInput = nullptr;
    geode::TextInput* m_descInput = nullptr;
    geode::TextInput* m_priceInput = nullptr;
    CCMenuItemToggler* m_limitedToggler = nullptr;
    geode::TextInput* m_amountInput = nullptr;
    CCLabelBMFont* m_fileNameLabel = nullptr;

    std::string m_selectedFilePath;
};
