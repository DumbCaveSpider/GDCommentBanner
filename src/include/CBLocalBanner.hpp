#pragma once

#include <Geode/Geode.hpp>
#include <matjson.hpp>
#include <asp/fs/fs.hpp>
#include <vector>
#include <string>
#include <algorithm>

using namespace geode::prelude;

namespace comment::local {
    struct LocalBanner {
        int id = 0;
        std::string filename;
        std::string name;
        bool equipped = false;

        matjson::Value toJson() const {
            auto obj = matjson::makeObject({
                {"id", id},
                {"filename", filename},
                {"name", name},
                {"equipped", equipped}
            });
            return obj;
        }

        static LocalBanner fromJson(const matjson::Value& val) {
            LocalBanner b;
            b.id = val["id"].asInt().unwrapOr(0);
            b.filename = val["filename"].asString().unwrapOr("");
            b.name = val["name"].asString().unwrapOr("Unnamed Banner");
            b.equipped = val["equipped"].asBool().unwrapOr(false);
            return b;
        }
    };

    inline asp::fs::Path getLocalBannersDir() {
        auto dir = Mod::get()->getSaveDir() / "local_banners";
        (void)asp::fs::createDirAll(dir);
        return dir;
    }

    inline asp::fs::Path getMetadataPath() {
        return getLocalBannersDir() / "metadata.json";
    }

    inline std::vector<LocalBanner> getLocalBanners() {
        std::vector<LocalBanner> banners;
        auto path = getMetadataPath();
        if (!path.exists()) {
            return banners;
        }
        auto res = file::readString(path);
        if (res.isErr()) return banners;
        auto jsonRes = matjson::parse(res.unwrap());
        if (jsonRes.isErr()) return banners;
        auto arr = jsonRes.unwrap();
        if (!arr.isArray()) return banners;
        for (auto const& item : arr.asArray().unwrap()) {
            banners.push_back(LocalBanner::fromJson(item));
        }
        return banners;
    }

    inline void saveLocalBanners(const std::vector<LocalBanner>& banners) {
        std::vector<matjson::Value> arr;
        for (auto const& b : banners) {
            arr.push_back(b.toJson());
        }
        auto val = matjson::Value(arr);
        auto res = file::writeString(getMetadataPath(), val.dump());
        if (res.isErr()) {
            log::error("Failed to save local banners metadata: {}", res.unwrapErr());
        }
    }

    inline void unequipLocalBanner() {
        auto banners = getLocalBanners();
        for (auto& b : banners) {
            b.equipped = false;
        }
        saveLocalBanners(banners);
        Mod::get()->setSavedValue("equipped-local-banner", std::string(""));
    }

    inline std::string getEquippedLocalBannerUrl() {
        auto saved = Mod::get()->getSavedValue<std::string>("equipped-local-banner", "");
        if (saved.empty()) return "";
        auto path = getLocalBannersDir() / saved;
        if (path.exists()) {
            return path.string();
        }
        // If file doesn't exist on disk, unequip
        unequipLocalBanner();
        return "";
    }

    inline void equipLocalBanner(int id) {
        auto banners = getLocalBanners();
        std::string equippedFilename = "";
        for (auto& b : banners) {
            if (b.id == id) {
                b.equipped = true;
                equippedFilename = b.filename;
            } else {
                b.equipped = false;
            }
        }
        saveLocalBanners(banners);
        Mod::get()->setSavedValue("equipped-local-banner", equippedFilename);
    }

    inline bool deleteLocalBanner(int id) {
        auto banners = getLocalBanners();
        auto it = std::find_if(banners.begin(), banners.end(), [id](const LocalBanner& b) {
            return b.id == id;
        });
        if (it != banners.end()) {
            if (it->equipped) {
                Mod::get()->setSavedValue("equipped-local-banner", std::string(""));
            }
            if (!it->filename.empty()) {
                auto path = getLocalBannersDir() / it->filename;
                (void)asp::fs::remove(path);
            }
            banners.erase(it);
            saveLocalBanners(banners);
            return true;
        }
        return false;
    }

    inline bool addLocalBanner(const std::string& name, const std::string& sourceImagePath) {
        if (sourceImagePath.empty() || !asp::fs::exists(sourceImagePath)) {
            return false;
        }
        auto dir = getLocalBannersDir();
        auto banners = getLocalBanners();
        int maxId = 0;
        for (auto const& b : banners) {
            if (b.id > maxId) maxId = b.id;
        }
        int newId = maxId + 1;

        std::string ext = "png";
        auto pos = sourceImagePath.find_last_of(".");
        if (pos != std::string::npos) {
            ext = sourceImagePath.substr(pos + 1);
        }
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != "png" && ext != "gif" && ext != "webp") {
            log::error("Unsupported file extension for local banner: {}. Only .png, .gif, and .webp are supported.", ext);
            return false;
        }
        std::string filename = fmt::format("banner_{}.{}", newId, ext);
        auto destPath = dir / filename;

        auto copyRes = asp::fs::copy(sourceImagePath, destPath);
        if (!copyRes) {
            log::error("Failed to copy local banner file: {}", copyRes.unwrapErr());
            return false;
        }

        LocalBanner b;
        b.id = newId;
        b.filename = filename;
        b.name = name.empty() ? fmt::format("Local Banner #{}", newId) : name;
        b.equipped = false;

        banners.push_back(b);
        saveLocalBanners(banners);
        return true;
    }
}
