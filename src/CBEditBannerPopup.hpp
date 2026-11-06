#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

class CBUpdateAmountPopup : public geode::Popup {
protected:
    int m_bannerId;
    int m_currentAmount;
    int m_totalBought;
    std::function<void()> m_onSuccess;
    geode::TextInput* m_amountInput;

    bool init(int bannerId, int currentAmount, int totalBought, std::function<void()> onSuccess);
    void onUpdate(CCObject*);

public:
    static CBUpdateAmountPopup* create(int bannerId, int currentAmount, int totalBought, std::function<void()> onSuccess);
};
