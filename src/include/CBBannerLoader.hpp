#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/LazySprite.hpp>
#include <asp/fs/fs.hpp>

using namespace geode::prelude;

namespace comment::loader {
    // priority: goes ahead of everything already waiting and isn't held back by the concurrency limit
    void queue(LazySprite* sprite, std::string source, bool priority = false);

    asp::fs::Path getCacheDir();
    void clearDiskCache();
}
