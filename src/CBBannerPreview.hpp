#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

class CBBannerPreview : public CCNode {
public:
    static CBBannerPreview* create(std::string const& imageUrl, std::string const& nonCompactImageUrl, CCSize const& maxSize);

    void setCompact(bool compact);
    void setOverlay(bool overlay);

    CCMenu* createToggles();

private:
    bool init(std::string const& imageUrl, std::string const& nonCompactImageUrl, CCSize const& maxSize);

    CCNode* m_normalPreview = nullptr;
    CCNode* m_compactPreview = nullptr;
};
