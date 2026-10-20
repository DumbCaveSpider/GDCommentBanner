#pragma once
#include <Geode/Geode.hpp>
#include <argon/argon.hpp>
#include <Geode/ui/LazySprite.hpp>
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

    inline CCNode* createBannerNode(std::string const& imageUrl, CCSize const& bannerSize) {
        if (imageUrl.empty()) {
            auto node = CCNode::create();
            node->setContentSize(bannerSize);
            node->setAnchorPoint({0.5f, 0.5f});
            return node;
        }

        auto sprite = LazySprite::create(bannerSize, true);
        sprite->setAutoResize(true);

        // loading is deferred until the banner is on screen, see CBBannerLoader
        if (imageUrl.starts_with("/")) {
            loader::queue(sprite, fmt::format("{}{}", comment::baseUrl, imageUrl));
        } else {
            loader::queue(sprite, imageUrl);
        }

        return sprite;
    }

    void invalidateBannerCache(int accountId = 0);
    void updateBannerCache(int accountId, bool equipped, std::string const& imageUrl = "");

    inline std::string getResponseMessage(web::WebResponse const& response,
        std::string const& fallback) {
        auto message = response.string().unwrapOrDefault();
        if (!message.empty())
            return message;
        return fallback;
    }
};