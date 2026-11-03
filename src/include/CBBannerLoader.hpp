#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/LazySprite.hpp>
#include <asp/fs/fs.hpp>

using namespace geode::prelude;

namespace comment::loader {
    void queue(LazySprite* sprite, std::string source);

    asp::fs::Path getCacheDir();
    void clearDiskCache();
}
