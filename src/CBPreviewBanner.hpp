#pragma once
#include <Geode/Geode.hpp>
#include <Geode/binding/CCBlockLayer.hpp>
#include <Geode/ui/Toggler.hpp>

using namespace geode::prelude;

// Full screen preview of a banner on a mocked up comment cell, in both
// non-compact and compact mode. Images can be local file paths or banner URLs.
class CBPreviewBanner : public CCBlockLayer {
public:
    // nonCompactImage is optional, the compact image is used for both modes without it
    static CBPreviewBanner* create(std::string const& compactImage, std::string const& nonCompactImage = "");

    // adds the preview on top of the running scene
    void show();
    void close();

    void keyBackClicked() override;

private:
    bool init(std::string const& compactImage, std::string const& nonCompactImage);
    // builds both previews once, the overlay toggle only \shows/hides the comment on top
    void buildPreviews();

    std::string m_compactImage;
    std::string m_nonCompactImage;
    bool m_showOverlay = true;
    CCNode* m_container = nullptr;
    std::vector<CCNode*> m_previews;
};
