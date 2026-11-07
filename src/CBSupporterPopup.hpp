#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

class CBSupporterPopup : public Popup {
public:
    static CBSupporterPopup* create();

private:
    bool init();
    void onSelectEmail(CCObject*);
    void onSelectDiscord(CCObject*);
    void setUseDiscord(bool useDiscord);
    void onSubmit(CCObject*);
    void updateStatusLabel(bool isSupporter);

    TextInput* m_emailInput = nullptr;
    TextInput* m_discordInput = nullptr;
    ButtonSprite* m_emailTabSprite = nullptr;
    ButtonSprite* m_discordTabSprite = nullptr;
    Label* m_statusLabel = nullptr;
    bool m_useDiscord = false;
};
