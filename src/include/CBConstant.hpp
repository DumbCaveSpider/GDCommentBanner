#pragma once
#include <Geode/Geode.hpp>
#include <argon/argon.hpp>

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

    inline std::unordered_map<std::string, CCTexture2D*>& bannerTextureCache() {
        static std::unordered_map<std::string, CCTexture2D*> cache;
        return cache;
    }

    inline CCTexture2D* findBannerTexture(std::string const& url) {
        auto& cache = bannerTextureCache();
        auto it = cache.find(url);
        return it == cache.end() ? nullptr : it->second;
    }

    inline void cacheBannerTexture(std::string const& url, CCTexture2D* texture) {
        if (!texture) return;
        auto& cache = bannerTextureCache();
        auto it = cache.find(url);
        if (it != cache.end()) {
            if (it->second == texture) return;
            if (it->second) it->second->release();
        }
        texture->retain();
        cache[url] = texture;
    }

    inline CCNode* createBannerNode(std::string const& imageUrl, CCSize const& bannerSize) {
        auto node = CCNode::create();
        node->setContentSize(bannerSize);
        node->setAnchorPoint({0.5f, 0.5f});

        if (imageUrl.empty()) return node;

        if (auto tex = findBannerTexture(imageUrl)) {
            auto sprite = CCSprite::createWithTexture(tex);
            auto contentSize = sprite->getContentSize();
            if (contentSize.width > 0 && contentSize.height > 0) {
                float scaleX = bannerSize.width / contentSize.width;
                float scaleY = bannerSize.height / contentSize.height;
                sprite->setScale(std::min(scaleX, scaleY));
            }
            sprite->setPosition(bannerSize / 2.f);
            node->addChild(sprite);
            return node;
        }

        Ref<CCNode> nodeRef = node;
        arc::spawn([nodeRef, imageUrl, bannerSize]() -> arc::Future<> {
            auto res = co_await geode::utils::web::WebRequest().get(imageUrl);
            if (!res.ok()) co_return;

            auto data = res.data();
            geode::queueInMainThread([nodeRef, imageUrl, bannerSize, data = std::move(data)]() {
                if (auto tex = findBannerTexture(imageUrl)) {
                    auto sprite = CCSprite::createWithTexture(tex);
                    auto contentSize = sprite->getContentSize();
                    if (contentSize.width > 0 && contentSize.height > 0) {
                        float scaleX = bannerSize.width / contentSize.width;
                        float scaleY = bannerSize.height / contentSize.height;
                        sprite->setScale(std::min(scaleX, scaleY));
                    }
                    sprite->setPosition(bannerSize / 2.f);
                    nodeRef->addChild(sprite);
                    return;
                }

                auto img = new CCImage();
                if (img->initWithImageData(const_cast<uint8_t*>(data.data()), data.size())) {
                    auto tex = new CCTexture2D();
                    if (tex->initWithImage(img)) {
                        cacheBannerTexture(imageUrl, tex);
                        auto sprite = CCSprite::createWithTexture(tex);
                        auto contentSize = sprite->getContentSize();
                        if (contentSize.width > 0 && contentSize.height > 0) {
                            float scaleX = bannerSize.width / contentSize.width;
                            float scaleY = bannerSize.height / contentSize.height;
                            sprite->setScale(std::min(scaleX, scaleY));
                        }
                        sprite->setPosition(bannerSize / 2.f);
                        nodeRef->addChild(sprite);
                        tex->release();
                    }
                    img->release();
                }
            });
        });

        return node;
    }

    inline std::string getResponseMessage(web::WebResponse const& response,
        std::string const& fallback) {
        auto message = response.string().unwrapOrDefault();
        if (!message.empty())
            return message;
        return fallback;
    }
};