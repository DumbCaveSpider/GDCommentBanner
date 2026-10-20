#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace comment {
    inline constexpr float kCommentCellHeight = 80.f;
    inline constexpr float kCompactCommentCellHeight = 36.f;
    inline constexpr float kCommentCellWidth = 340.f;

    CCNode* createCommentPreview(std::string const& imageUrl, bool compact, float width = kCommentCellWidth);
}
