#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/Toggler.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include "CBEditBannerPopup.hpp"

using namespace geode::prelude;

class CBSubmitBannerPopup : public Popup {
public:
    static CBSubmitBannerPopup* create(
        bool isLocal = false,
        bool isEdit = false,
        CBEditBannerData const& editData = {},
        std::function<void()> onEditSuccess = nullptr);

private:
    bool init(bool isLocal, bool isEdit, CBEditBannerData const& editData, std::function<void()> onEditSuccess);
    void submitEdit();

    void onPickFile(CCObject*);
    void updateFileLabel();
    void onSubmit(CCObject*);
    void onToggleLimited(CCObject*);
    void onPreview(CCObject*);

    TextInput* m_nameInput = nullptr;
    TextInput* m_descInput = nullptr;
    TextInput* m_priceInput = nullptr;
    Toggler* m_limitedToggler = nullptr;
    Toggler* m_supporterOnlyToggler = nullptr;
    TextInput* m_amountInput = nullptr;
    Label* m_fileNameLabel = nullptr;
    Button* m_previewBtn = nullptr;

    std::string m_selectedFilePath;
    std::string m_selectedNonCompactPath;
    bool m_canUploadNonCompact = false;
    bool m_isLocal = false;
    bool m_isEdit = false;
    CBEditBannerData m_editData;
    std::function<void()> m_onEditSuccess;
};
