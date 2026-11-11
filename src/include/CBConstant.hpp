#pragma once
#include <Geode/Geode.hpp>
#include <dankmeme.argon/argon.hpp>
#include <Geode/ui/LazySprite.hpp>
#include <Geode/ui/Label.hpp>
#include <Geode/ui/Layout.hpp>
#include <Geode/ui/TextArea.hpp>
#include <Geode/ui/Toggler.hpp>
#include <Geode/binding/GameManager.hpp>
#include <Geode/binding/GJAccountManager.hpp>
#include <Geode/utils/string.hpp>
#include <cue/PlayerIcon.hpp>
#include <algorithm>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>
#include "CBBannerLoader.hpp"

using namespace geode::prelude;
namespace comment {
    inline std::string baseUrl = "https://gdbanner.arcticwoof.xyz";

    inline auto argonToken = [](const argon::AccountData& accountData) -> arc::Future<std::string> {
        auto authResult = co_await argon::startAuth(accountData);
        if (!authResult) {
            co_return std::string();
        }
        co_return std::move(authResult).unwrap();
    };

    inline CCNode* createBannerNode(std::string const& imageUrl, CCSize const& bannerSize, bool priority = false) {
        if (imageUrl.empty()) {
            auto node = CCNode::create();
            node->setContentSize(bannerSize);
            node->setAnchorPoint({0.5f, 0.5f});
            return node;
        }

        auto sprite = LazySprite::create(bannerSize, true);
        sprite->setAutoResize(true);

        if (imageUrl.starts_with("/")) {
            loader::queue(sprite, fmt::format("{}{}", comment::baseUrl, imageUrl), priority);
        } else {
            loader::queue(sprite, imageUrl, priority);
        }

        return sprite;
    }

    void invalidateBannerCache(int accountId = 0);
    void updateBannerCache(int accountId, bool equipped, std::string const& imageUrl = "", std::string const& nonCompactImageUrl = "");

    inline std::string getResponseMessage(web::WebResponse const& response,
        std::string const& fallback) {
        auto message = response.string().unwrapOrDefault();
        if (!message.empty())
            return message;
        return fallback;
    }

    inline bool isUserSupporter() {
        return Mod::get()->getSavedValue<bool>("is_supporter", false);
    };

    inline float randomSigned() {
        return (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.f - 1.f;
    }

    inline CCSprite* createAmethyst(float height) {
        auto sprite = CCSprite::createWithSpriteFrameName("CB_amethyst_003.png"_spr);
        if (sprite) sprite->setScale(height / std::max(sprite->getContentHeight(), 1.f));
        return sprite;
    }

    struct AmethystSettings {
        float base;
        float spread;
        float scale;
    };

    inline AmethystSettings amethystSettingsForCount(int count, float time) {
        AmethystSettings s{time, time * 0.3f, 1.f};
        if (count < 10) {
            s.spread = 0.2f;
            s.base = 0.8f;
        } else if (count > 400) {
            s.spread = 0.9f;
            s.scale = 0.8f;
        } else if (count > 300) {
            s.spread = 0.8f;
            s.scale = 0.8f;
        } else if (count > 200) {
            s.spread = 0.7f;
            s.scale = 0.8f;
        } else if (count > 100) {
            s.spread = 0.6f;
            s.scale = 0.9f;
        } else if (count > 50) {
            s.spread = 0.5f;
            s.scale = 0.9f;
        } else if (count > 30) {
            s.spread = 0.4f;
            s.scale = 0.95f;
        } else if (count > 20) {
            s.scale = 0.95f;
        }
        return s;
    }

    inline std::string imageMimeType(std::string const& filePath) {
        std::string ext = filePath.substr(filePath.find_last_of(".") + 1);
        utils::string::toLowerIP(ext);
        if (ext == "webp") return "image/webp";
        if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
        if (ext == "gif") return "image/gif";
        return "image/png";
    }

    inline int submissionFeeForPrice(int price) {
        if (price <= 0) price = 1;
        int multiplier = ((price - 1) / 100000) + 1;
        return 15000 * std::max(1, multiplier);
    }

    inline float earnRateForEquips(float equippedCount) {
        if (equippedCount <= 0) return 0.f;
        return std::min(0.30f, 0.05f + (equippedCount / 5.f) * 0.05f);
    }

    inline CCNode* createStat(char const* frameName, std::string const& text, ccColor3B color) {
        auto node = CCNode::create();
        node->setAnchorPoint({0.f, 0.5f});
        node->setContentSize({0.f, 14.f});
        node->setLayout(RowLayout::create()->setGap(2.f)->setAutoScale(false)->setAutoGrowAxis(0.f));

        auto icon = CCSprite::createWithSpriteFrameName(frameName);
        if (icon) {
            icon->setScale(12.f / std::max(icon->getContentHeight(), 1.f));
            node->addChild(icon);
        }

        auto label = Label::create(text.c_str(), "bigFont.fnt");
        label->setScale(0.3f);
        label->setColor(color);
        node->addChild(label);

        node->updateLayout();
        return node;
    }

    inline CCNode* createRow(float width, float height = 20.f, float gap = 4.f) {
        auto row = CCNode::create();
        row->setAnchorPoint({0.5f, 0.5f});
        row->setContentSize({width, height});
        row->setLayout(RowLayout::create()->setGap(gap)->setAutoScale(false));
        return row;
    }

    inline CCMenu* createToggleMenu(float width, size_t rows, float rowHeight = 26.f) {
        auto menu = CCMenu::create();
        menu->setAnchorPoint({0.5f, 1.f});
        menu->setContentSize({width, rowHeight * rows});
        menu->setLayout(ColumnLayout::create()
                ->setGap(0.f)
                ->setAxisReverse(true)
                ->setAutoScale(false)
                ->setAxisAlignment(AxisAlignment::End));
        return menu;
    }

    inline Toggler* addToggle(CCMenu* menu, float width, char const* text, bool toggled, Toggler::ToggleCallback callback) {
        auto row = CCNode::create();
        row->setAnchorPoint({0.5f, 0.5f});
        row->setContentSize({width, 26.f});
        row->setLayout(RowLayout::create()
                ->setAxisAlignment(AxisAlignment::Start)
                ->setGap(5.f)
                ->setAutoScale(false));

        auto toggler = Toggler::createWithStandardSprites(0.6f, std::move(callback));
        toggler->setToggled(toggled);
        row->addChild(toggler);

        auto label = Label::create(text, "bigFont.fnt");
        label->setLimitLabelWidth(width - 30.f, 0.4f, 0.1f);
        row->addChild(label);

        row->updateLayout();
        menu->addChild(row);
        return toggler;
    }

    inline void selectOnly(std::vector<Toggler*> const& group, Toggler* selected) {
        for (auto toggler : group) {
            toggler->setToggled(toggler == selected);
        }
    }

    inline void applyPlayerIcon(cue::PlayerIcon* player, int iconId, int color1, int color2, int glowColor, bool hasGlow) {
        if (iconId <= 0) iconId = 1;
        player->updateIcons(cue::Icons{IconType::Cube, iconId, color1, color2, hasGlow ? glowColor : -1});
    }

    inline cue::PlayerIcon* createLocalPlayerIcon() {
        auto gm = GameManager::sharedState();
        auto player = cue::PlayerIcon::create(
            IconType::Cube,
            gm->getPlayerFrame(),
            gm->getPlayerColor(),
            gm->getPlayerColor2(),
            gm->getPlayerGlow() ? gm->getPlayerGlowColor() : -1);
        player->setAnchorPoint({0.5f, 0.5f});
        return player;
    }

    inline std::string localUsername() {
        std::string name = GJAccountManager::sharedState()->m_username;
        if (name.empty()) name = GameManager::sharedState()->m_playerName;
        if (name.empty()) name = "Player";
        return name;
    }

    inline void addCellLines(CCNode* cell, float width, float height) {
        for (float y : {0.f, height - 1.f}) {
            auto line = CCLayerColor::create({0, 0, 0, 75}, width - 2.f, 1.f);
            line->setPosition({1.f, y});
            cell->addChild(line, 5);
        }
    }

    struct LogStyle {
        char const* emote;
        char const* sprite;
        ccColor3B color;
    };

    inline LogStyle logStyleForType(std::string const& type) {
        if (type == "APPROVED") return {"check", "GJ_completesIcon_001.png", ccGREEN};
        if (type == "REJECTED") return {"cross", "GJ_deleteIcon_001.png", ccRED};
        if (type == "BANNER_SOLD") return {"chartup", "GJ_sStarsIcon_001.png", ccGREEN};
        if (type == "BANNER_PAYOUT" || type == "AMETHYST_REWARD" || type == "ADMIN_AMETHYST_CHANGE" || type == "AMETHYST_GIFT_RECEIVED")
            return {"chest", "GJ_sStarsIcon_001.png", ccMAGENTA};
        if (type == "FEATURED") return {"star", "GJ_starsIcon_001.png", ccORANGE};
        if (type == "UNFEATURED") return {"brokenheart", "GJ_noStarsIcon_001.png", ccGRAY};
        if (type == "BANNER_REFUNDED") return {"chartdown", "GJ_sStarsIcon_001.png", ccORANGE};
        if (type == "SUPPORTER_LINKED") return {"royal", "gj_heartOn_001.png", ccMAGENTA};
        if (type == "BANNER_DELETED" || type == "BANNER_REMOVED") return {"mail", "GJ_deleteIcon_001.png", ccRED};
        if (type == "TEMP_BAN" || type == "PERMANENT_BAN") return {"skullbomb", "GJ_deleteIcon_001.png", ccRED};
        if (type == "BANNER_EDITED" || type == "BANNER_CHANGED" || type == "BANNER_UNEQUIPPED")
            return {"tools", "GJ_infoIcon_001.png", ccYELLOW};
        return {"mail", "GJ_infoIcon_001.png", ccYELLOW};
    }

    struct CBLogCategory {
        char const* name;
        std::vector<char const*> types;
    };

    inline std::vector<CBLogCategory> const& getLogCategories() {
        static std::vector<CBLogCategory> const categories = {
            {"Reviews", {"APPROVED", "REJECTED"}},
            {"Banners", {"BANNER_EDITED", "BANNER_CHANGED", "BANNER_DELETED", "BANNER_REMOVED", "BANNER_UNEQUIPPED", "FEATURED", "UNFEATURED"}},
            {"Sales", {"BANNER_SOLD", "BANNER_REFUNDED"}},
            {"Amethysts", {"BANNER_PAYOUT", "AMETHYST_REWARD", "ADMIN_AMETHYST_CHANGE", "AMETHYST_GIFT_SENT", "AMETHYST_GIFT_RECEIVED"}},
            {"Account", {"SUPPORTER_LINKED", "TEMP_BAN", "PERMANENT_BAN"}},
        };
        return categories;
    }

    inline std::vector<std::string> getLogTypesFor(std::set<int> const& categories) {
        std::vector<std::string> types;
        auto const& all = getLogCategories();
        for (int index : categories) {
            if (index < 0 || index >= static_cast<int>(all.size())) continue;
            for (auto type : all[index].types) types.emplace_back(type);
        }
        return types;
    }
};