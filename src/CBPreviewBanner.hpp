#pragma once
#include <Geode/Geode.hpp>
#include <Geode/binding/CCBlockLayer.hpp>
#include <Geode/ui/Toggler.hpp>

using namespace geode::prelude;

class CBPreviewBanner : public CCBlockLayer {
public:
    static CBPreviewBanner* create(std::string const& compactImage, std::string const& nonCompactImage = "");

    void show();
    void close();

    void keyBackClicked() override;

private:
    bool init(std::string const& compactImage, std::string const& nonCompactImage);
    void buildPreviews();

    std::string m_compactImage;
    std::string m_nonCompactImage;
    bool m_showOverlay = true;
    CCNode* m_container = nullptr;
    std::vector<CCNode*> m_previews;
};
