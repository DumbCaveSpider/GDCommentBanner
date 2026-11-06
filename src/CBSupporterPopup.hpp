#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

// Links a Ko-fi membership (by email or Discord username) to the current GD account.
class CBSupporterPopup : public Popup {
public:
    static CBSupporterPopup* create();

private:
    bool init();
    void onSelectEmail(CCObject*);
    void onSelectDiscord(CCObject*);
    void setUseDiscord(bool useDiscord);
    void onSubmit(CCObject*);

    TextInput* m_emailInput = nullptr;
    TextInput* m_discordInput = nullptr;
    ButtonSprite* m_emailTabSprite = nullptr;
    ButtonSprite* m_discordTabSprite = nullptr;
    bool m_useDiscord = false;
};
