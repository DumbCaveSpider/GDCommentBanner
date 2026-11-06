#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

using namespace geode::prelude;

struct CBEditBannerData {
    int id = 0;
    std::string name;
    std::string description;
    int price = 0;
    bool isLimited = false;
    int amount = 0;
    int totalBought = 0;
    bool isSupporterOnly = false;
    std::string imageUrl;
    std::string nonCompactImageUrl;
};

class CBEditBannerPopup : public Popup {
protected:
    CBEditBannerData m_data;
    std::function<void()> m_onSuccess;

    TextInput* m_nameInput = nullptr;
    TextInput* m_descInput = nullptr;
    TextInput* m_priceInput = nullptr;
    TextInput* m_amountInput = nullptr;

    bool init(CBEditBannerData data, std::function<void()> onSuccess);
    void onSave(CCObject*);
    void onChangeImage(CCObject*);
    void onDelete(CCObject*);
    void deleteBanner();
    void submitEdit(std::string name, std::string desc, int price, int amount);

public:
    static CBEditBannerPopup* create(CBEditBannerData data, std::function<void()> onSuccess);
};
