#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include "CBEditBannerPopup.hpp"

using namespace geode::prelude;

class CBSubmitBannerPopup : public Popup {
public:
    // isEdit: replace the image of an existing online banner (editData.id) instead of submitting a new one.
    // editData prefills the inputs, onEditSuccess is called after the banner was updated.
    static CBSubmitBannerPopup* create(
        bool isLocal = false,
        bool isEdit = false,
        CBEditBannerData const& editData = {},
        std::function<void()> onEditSuccess = nullptr);

private:
    bool init(bool isLocal, bool isEdit, CBEditBannerData const& editData, std::function<void()> onEditSuccess);
    void submitEdit();
    // sender tag 1 picks the non-compact image, anything else the compact one
    void onPickFile(CCObject*);
    void updateFileLabel();
    void onSubmit(CCObject*);
    void onToggleLimited(CCObject*);
    void onPreview(CCObject*);
    void onClosePreview(CCObject*);

    TextInput* m_nameInput = nullptr;
    TextInput* m_descInput = nullptr;
    TextInput* m_priceInput = nullptr;  // null in edit mode
    CCMenuItemToggler* m_limitedToggler = nullptr;
    CCMenuItemToggler* m_supporterOnlyToggler = nullptr;  // null for non-supporters
    TextInput* m_amountInput = nullptr;
    Label* m_fileNameLabel = nullptr;
    CCMenuItemSpriteExtra* m_previewBtn = nullptr;

    std::string m_selectedFilePath;            // compact image (1500x150)
    std::string m_selectedNonCompactPath;      // optional non-compact image (1500x350), supporters only
    bool m_canUploadNonCompact = false;
    bool m_isLocal = false;
    bool m_isEdit = false;
    CBEditBannerData m_editData;
    std::function<void()> m_onEditSuccess;
};
