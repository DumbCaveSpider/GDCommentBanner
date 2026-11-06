#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Button.hpp>
#include <Geode/ui/Toggler.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/binding/CCCounterLabel.hpp>
#include <cue/LoadingCircle.hpp>

using namespace geode::prelude;

class CBGiftAmethyst : public Popup {
public:
    static CBGiftAmethyst* create(int targetAccountId);

    static constexpr int kMinRecipientBalance = 10000;
    static constexpr int kMinGiftAmount = 10000;
    static constexpr int kMaxGiftAmount = 10000000;
    static bool isGiftAmountAllowed(int amount) { return amount >= kMinGiftAmount && amount <= kMaxGiftAmount; }

private:
    bool init(int targetAccountId);
    void fetchUsers();
    void onUsersLoaded(std::string myName, int myBalance, matjson::Value target);
    void updatePreview();
    void startBalanceRowUpdates();
    void updateBalanceRows(float dt);
    int myBalance() const;
    int targetBalance() const;
    int getAmount() const;
    void onGift(CCObject*);
    void sendGift(int amount);

    struct Column {
        CCMenu* menu = nullptr;
        CCNode* nameRow = nullptr;
        SimplePlayer* player = nullptr;
        Label* nameLabel = nullptr;
        CCNode* balanceRow = nullptr;
        CCCounterLabel* balanceLabel = nullptr;
        int balance = 0;
    };
    Column createColumn();
    void addColumn(Column const& column, CCPoint offset);
    void setUser(Column& column, std::string const& name, int balance);

    int m_targetAccountId = 0;
    std::string m_targetName;
    bool m_loaded = false;
    bool m_updatingRows = false;

    Column m_myColumn;
    Column m_targetColumn;
    Label* m_statusLabel = nullptr;
    TextInput* m_amountInput = nullptr;
    Button* m_giftBtn = nullptr;
    cue::LoadingCircle* m_loadingCircle = nullptr;
};
