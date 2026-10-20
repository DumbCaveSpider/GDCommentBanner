#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/LazySprite.hpp>

using namespace geode::prelude;

namespace comment::loader {
    void queue(LazySprite* sprite, std::string source);

    std::filesystem::path getCacheDir();
    void clearDiskCache();
}
