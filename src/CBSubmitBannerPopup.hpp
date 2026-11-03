#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

class CBSubmitBannerPopup : public geode::Popup {
public:
    static CBSubmitBannerPopup* create(bool isLocal = false);

private:
    bool init(bool isLocal);
    void onPickFile(CCObject*);
    void onSubmit(CCObject*);
    void onToggleLimited(CCObject*);
    void onPreview(CCObject*);
    void onClosePreview(CCObject*);

    geode::TextInput* m_nameInput = nullptr;
    geode::TextInput* m_descInput = nullptr;
    geode::TextInput* m_priceInput = nullptr;
    CCMenuItemToggler* m_limitedToggler = nullptr;
    geode::TextInput* m_amountInput = nullptr;
    geode::Label* m_fileNameLabel = nullptr;
    CCMenuItemSpriteExtra* m_previewBtn = nullptr;

    std::string m_selectedFilePath;
    bool m_isLocal = false;
};
