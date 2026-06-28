#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/Button.hpp>

using namespace geode::prelude;

class CBAdminSearchPopup : public geode::Popup {
protected:
    bool init(std::string title, std::string placeholder, std::function<void(std::string)> callback);
    
    std::function<void(std::string)> m_callback;
    geode::TextInput* m_input;

    void onConfirm(CCObject*);

public:
    static CBAdminSearchPopup* create(std::string title, std::string placeholder, std::function<void(std::string)> callback);
};