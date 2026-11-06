#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

// A banner on mocked up comment cells (non-compact and compact, one shown at a time),
// scaled to fit maxSize. Used by the banner info and purchase popups.
// Compact mode and the comment overlay follow the saved preview settings (see CBBannerCell).
class CBBannerPreview : public CCNode {
public:
    static CBBannerPreview* create(std::string const& imageUrl, std::string const& nonCompactImageUrl, CCSize const& maxSize);

    void setCompact(bool compact);
    void setOverlay(bool overlay);

    // "Non-Compact" and "Comment Overlay" toggles for this preview (anchor at the top right).
    // Changing them also saves the setting and updates the other previews on screen.
    CCMenu* createToggles();

private:
    bool init(std::string const& imageUrl, std::string const& nonCompactImageUrl, CCSize const& maxSize);

    CCNode* m_normalPreview = nullptr;  // non-compact comment mockup
    CCNode* m_compactPreview = nullptr;
};
