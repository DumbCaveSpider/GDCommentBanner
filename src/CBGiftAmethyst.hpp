#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/Toggler.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <cue/LoadingCircle.hpp>

using namespace geode::prelude;

class CBGiftAmethyst : public Popup {
public:
    static CBGiftAmethyst* create(int targetAccountId);

    static constexpr int kMinRecipientBalance = 10000;

private:
    bool init(int targetAccountId);
    void fetchUsers();
    void onUsersLoaded(std::string myName, int myBalance, matjson::Value target);
    void updatePreview();
    int getAmount() const;
    void onGift(CCObject*);
    void sendGift(int amount);

    struct Column {
        CCMenu* menu = nullptr;
        CCNode* nameRow = nullptr;
        SimplePlayer* player = nullptr;
        Label* nameLabel = nullptr;
        CCNode* balanceRow = nullptr;
        Label* balanceLabel = nullptr;
        Label* afterLabel = nullptr;
    };
    Column createColumn();
    void addColumn(Column const& column, CCPoint offset);
    void setUser(Column& column, std::string const& name, int balance);

    int m_targetAccountId = 0;
    int m_myBalance = 0;
    int m_targetBalance = 0;
    std::string m_targetName;
    bool m_loaded = false;

    Column m_myColumn;
    Column m_targetColumn;
    Label* m_statusLabel = nullptr;
    TextInput* m_amountInput = nullptr;
    Button* m_giftBtn = nullptr;
    cue::LoadingCircle* m_loadingCircle = nullptr;
};
