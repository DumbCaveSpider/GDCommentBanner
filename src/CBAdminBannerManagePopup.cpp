#include "CBAdminBannerManagePopup.hpp"
#include <argon/argon.hpp>
#include "Geode/ui/General.hpp"
#include "include/CBConstant.hpp"
#include <Geode/binding/UploadActionPopup.hpp>
#include <Geode/utils/web.hpp>

CBAdminBannerManagePopup* CBAdminBannerManagePopup::create(matjson::Value const& bannerData) {
    auto ret = new CBAdminBannerManagePopup();
    if (ret && ret->init(bannerData)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CBAdminBannerManagePopup::init(matjson::Value const& bannerData) {
    if (!Popup::init(350.f, 240.f, "GJ_square02.png")) return false;

    m_bannerData = bannerData;
    this->setTitle("Manage Banner");
    addSideArt(m_mainLayer, SideArt::All, SideArtStyle::PopupBlue);

    std::string name = m_bannerData["name"].asString().unwrapOr("Unknown");
    std::string desc = m_bannerData["description"].asString().unwrapOr("");
    int price = m_bannerData["price"].asInt().unwrapOr(0);
    bool isLimited = m_bannerData["isLimited"].asBool().unwrapOr(false);
    int amount = m_bannerData["amount"].asInt().unwrapOr(0);
    int totalBought = m_bannerData["totalBought"].asInt().unwrapOr(0);

    auto menu = CCMenu::create();
    menu->setPosition({m_mainLayer->getContentSize().width / 2, m_mainLayer->getContentSize().height / 2});
    m_mainLayer->addChild(menu);

    float startY = 80.f;
    float gap = 45.f;

    // Name
    m_nameInput = TextInput::create(300.f, "Banner Name");
    m_nameInput->setPosition({0, startY - 20.f});
    m_nameInput->setString(name);
    m_nameInput->setCommonFilter(CommonFilter::Name);
    m_nameInput->setLabel("Banner Name");
    menu->addChild(m_nameInput);

    // Description
    m_descInput = TextInput::create(300.f, "Description");
    m_descInput->setPosition({0, startY - gap - 20.f});
    m_descInput->setString(desc);
    m_descInput->setCommonFilter(CommonFilter::Any);
    m_descInput->setLabel("Banner Description");
    menu->addChild(m_descInput);

    // Price
    m_priceInput = TextInput::create(100.f, "Price");
    m_priceInput->setCommonFilter(CommonFilter::Int);
    m_priceInput->setLabel("Price");
    m_priceInput->setString(numToString(price));
    menu->addChild(m_priceInput);

    if (isLimited) {
        m_priceInput->setPosition({-60.f, startY - gap * 2 - 20.f});
        
        m_amountInput = TextInput::create(100.f, "Amount");
        m_amountInput->setPosition({60.f, startY - gap * 2 - 20.f});
        m_amountInput->setCommonFilter(CommonFilter::Int);
        m_amountInput->setLabel("Amount");
        m_amountInput->setString(numToString(amount));
        menu->addChild(m_amountInput);
    } else {
        m_priceInput->setPosition({0, startY - gap * 2 - 20.f});
    }

    // Amount info
    std::string amountText = fmt::format("Bought: {}", totalBought);
    if (isLimited) {
        amountText += fmt::format(" / Left: {}", amount - totalBought);
    }
    auto amountLabel = CCLabelBMFont::create(amountText.c_str(), "chatFont.fnt");
    amountLabel->setPosition({0, startY - gap * 3 - 5.f});
    amountLabel->setColor(ccColor3B{200, 200, 200});
    menu->addChild(amountLabel);

    // Buttons Menu
    auto btnMenu = CCMenu::create();
    btnMenu->setPosition({m_mainLayer->getContentSize().width / 2, 25.f});
    btnMenu->setLayout(RowLayout::create()->setGap(15.f)->setAxisAlignment(AxisAlignment::Center));
    m_mainLayer->addChild(btnMenu);

    // Save Button
    auto saveBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Save", "goldFont.fnt", "GJ_button_01.png"),
        this,
        menu_selector(CBAdminBannerManagePopup::onSave));
    btnMenu->addChild(saveBtn);

    // Delete Button
    auto deleteBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Delete", "goldFont.fnt", "GJ_button_06.png"),
        this,
        menu_selector(CBAdminBannerManagePopup::onDelete));
    btnMenu->addChild(deleteBtn);

    btnMenu->updateLayout();

    return true;
}

void CBAdminBannerManagePopup::onSave(CCObject*) {
    int bannerId = m_bannerData["id"].asInt().unwrapOr(0);
    std::string newName = m_nameInput->getString();
    std::string newDesc = m_descInput->getString();
    std::string priceStr = m_priceInput->getString();
    int newPrice = numFromString<int>(priceStr).unwrap();
    bool hasAmount = m_amountInput != nullptr;
    int newAmount = hasAmount ? numFromString<int>(m_amountInput->getString()).unwrapOr(0) : 0;

    auto accountData = argon::getGameAccountData();
    auto accountId = accountData.accountId;
    Ref<CBAdminBannerManagePopup> retainedSelf = this;

    Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Saving changes...");
    popup->show();

    arc::spawn([retainedSelf, bannerId, accountId, accountData, newName, newDesc, newPrice, hasAmount, newAmount, popup]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            geode::queueInMainThread([popup] {
                popup->showFailMessage("Authentication failed.");
            });
            co_return;
        }
        auto authToken = std::move(authResult);

        auto req = geode::utils::web::WebRequest();
        req.header("Content-Type", "application/x-www-form-urlencoded");

        std::string body = fmt::format("accountId={}&argonToken={}&bannerId={}&price={}&name={}&description={}",
            accountId,
            authToken,
            bannerId,
            newPrice,
            newName,
            newDesc);

        if (hasAmount) {
            body += fmt::format("&amount={}", newAmount);
        }

        auto res = co_await req.bodyString(body).post(fmt::format("{}/admin/updateBannerDetails", comment::baseUrl));
        bool ok = res.ok();

        geode::queueInMainThread([retainedSelf, popup, ok] {
            if (ok) {
                popup->showSuccessMessage("Banner updated!");
                retainedSelf->onClose(nullptr);
            } else {
                popup->showFailMessage("Failed to update banner.");
            }
        });
        co_return;
    });
}

void CBAdminBannerManagePopup::onDelete(CCObject*) {
    geode::createQuickPopup("Delete Banner", "Are you sure you want to <cr>delete</c> this banner?", "Cancel", "Delete", [this](FLAlertLayer*, bool btn2) {
        if (btn2) {
            int bannerId = m_bannerData["id"].asInt().unwrapOr(0);
            auto accountData = argon::getGameAccountData();
            auto accountId = accountData.accountId;
            Ref<CBAdminBannerManagePopup> retainedSelf = this;

            Ref<UploadActionPopup> popup = UploadActionPopup::create(nullptr, "Deleting banner...");
            popup->show();

            arc::spawn([retainedSelf, bannerId, accountId, accountData, popup]() -> arc::Future<> {
                auto authResult = co_await comment::argonToken(accountData);
                if (authResult.empty()) {
                    geode::queueInMainThread([popup] {
                        popup->showFailMessage("Authentication failed.");
                    });
                    co_return;
                }
                auto authToken = std::move(authResult);

                auto req = geode::utils::web::WebRequest();
                req.header("Content-Type", "application/x-www-form-urlencoded");
                std::string body = fmt::format("accountId={}&argonToken={}&bannerId={}", accountId, authToken, bannerId);
                auto res = co_await req.bodyString(body).post(fmt::format("{}/admin/deleteBanner", comment::baseUrl));
                bool ok = res.ok();

                geode::queueInMainThread([retainedSelf, popup, ok] {
                    if (ok) {
                        popup->showSuccessMessage("Banner deleted!");
                        retainedSelf->onClose(nullptr);
                    } else {
                        popup->showFailMessage("Failed to delete banner.");
                    }
                });
                co_return;
            });
        }
    });
}
