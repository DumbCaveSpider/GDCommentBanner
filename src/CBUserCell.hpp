#pragma once
#include <Geode/Geode.hpp>
#include <cue/PlayerIcon.hpp>

using namespace geode::prelude;

struct CBUserData {
    int accountId = 0;
    std::string username;
    bool isAdmin = false;
    bool isStaff = false;
    bool isSupporter = false;
    std::string equippedBannerUrl;

    int iconId = 0;
    int color1 = 0;
    int color2 = 0;
    int glowColor = 0;
    bool hasGlow = false;

    int amethyst = 0;
    int monthlyEarnings = 0;
    int totalEarned = 0;
    int totalSales = 0;

    static CBUserData fromJson(matjson::Value const& json);
};

struct CBUserCellOptions {
    int rank = 0;
    bool showBanner = true;
    bool showRole = false;
    bool showSalesCount = false;
    std::function<void()> onManage;
};

class CBUserCell : public CCNode {
public:
    static CBUserCell* create(CBUserData const& data, float width, CBUserCellOptions options = {});
    static CCNode* createStatsNode(CBUserData const& data, bool showSalesCount = false);
    static cue::PlayerIcon* createPlayer(CBUserData const& data);
    static CCNode* createBadges(CBUserData const& data, float height);

private:
    bool init(CBUserData const& data, float width, CBUserCellOptions options);

    CBUserData m_data;
    CBUserCellOptions m_options;
};
