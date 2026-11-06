#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

struct CBUserData {
    int accountId = 0;
    std::string username;
    bool isAdmin = false;
    bool isStaff = false;
    bool isSupporter = false;
    std::string equippedBannerUrl;

    // player cube, 0 = unknown (shown as the default cube)
    int iconId = 0;
    int color1 = 0;
    int color2 = 0;
    int glowColor = 0;
    bool hasGlow = false;

    int amethyst = 0;
    int monthlyEarnings = 0;
    int totalEarned = 0;
    int totalSales = 0;

    // reads the fields returned by /getLeaderboard and /admin/searchUsers
    static CBUserData fromJson(matjson::Value const& json);
};

struct CBUserCellOptions {
    int rank = 0;                // shown on the left when > 0
    bool showBanner = true;      // equipped banner behind the cell
    bool showRole = false;       // "Admin" / "Staff" / "User" under the name
    bool showSalesCount = false; // sales stat shows the number of sales instead of amethysts earned
    std::function<void()> onManage;  // adds the manage button when set
};

// A user row with their cube, name, amethysts, monthly earning rate and sales.
class CBUserCell : public CCNode {
public:
    static CBUserCell* create(CBUserData const& data, float width, CBUserCellOptions options = {});

private:
    bool init(CBUserData const& data, float width, CBUserCellOptions options);
    CCNode* createStatsRow();

    CBUserData m_data;
    CBUserCellOptions m_options;
};
