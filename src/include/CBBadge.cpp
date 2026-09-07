#include <Geode/Geode.hpp>
#include <dasshu.badgified/include/Badgified.hpp>
#include "CBConstant.hpp"
#include <unordered_map>
#include <vector>

using namespace geode::prelude;

struct RoleCache {
    bool isAdmin = false;
    bool isStaff = false;
};
static std::unordered_map<int, RoleCache> s_roleCache;

struct PendingBadge {
    dasshu::badgified::Badge badge;
    bool isAdminBadge;
};
static std::unordered_map<int, std::vector<PendingBadge>> s_pendingBadges;

void checkAndShowBadge(const dasshu::badgified::Badge& badge, bool isAdminBadge) {
    if (!badge.user || !badge.target) return;
    int targetAccountId = badge.user->m_accountID;
    if (targetAccountId <= 0) return;

    if (s_roleCache.contains(targetAccountId)) {
        auto const& roles = s_roleCache[targetAccountId];
        if (isAdminBadge && roles.isAdmin) {
            dasshu::badgified::showBadge(badge, CCSprite::createWithSpriteFrameName("CB_admin_badge.png"_spr));
        } else if (!isAdminBadge && roles.isStaff) {
            dasshu::badgified::showBadge(badge, CCSprite::createWithSpriteFrameName("CB_staff_badge.png"_spr));
        }
        return;
    }

    auto accountData = argon::getGameAccountData();
    int accountId = accountData.accountId;
    if (accountId <= 0) return;

    bool alreadyPending = s_pendingBadges.contains(targetAccountId);
    s_pendingBadges[targetAccountId].push_back({badge, isAdminBadge});
    if (alreadyPending) return;

    arc::spawn([targetAccountId, accountData, accountId]() -> arc::Future<> {
        auto authResult = co_await comment::argonToken(accountData);
        if (authResult.empty()) {
            geode::queueInMainThread([targetAccountId]() {
                s_pendingBadges.erase(targetAccountId);
            });
            co_return;
        }

        auto req = geode::utils::web::WebRequest();
        auto body = matjson::makeObject({{"accountId", accountId},
            {"argonToken", authResult},
            {"targetAccountId", targetAccountId}});
        auto response = co_await req.bodyJSON(body).post(fmt::format("{}/getUser", comment::baseUrl));
        if (!response.ok()) {
            geode::queueInMainThread([targetAccountId]() {
                s_pendingBadges.erase(targetAccountId);
            });
            co_return;
        }
        auto jsonRes = response.json();
        if (jsonRes.isErr()) {
            geode::queueInMainThread([targetAccountId]() {
                s_pendingBadges.erase(targetAccountId);
            });
            co_return;
        }
        auto json = jsonRes.unwrap();

        RoleCache roles;
        roles.isAdmin = json["is_admin"].asBool().unwrapOr(false);
        roles.isStaff = json["is_staff"].asBool().unwrapOr(false);

        geode::queueInMainThread([targetAccountId, roles]() {
            s_roleCache[targetAccountId] = roles;
            if (s_pendingBadges.contains(targetAccountId)) {
                for (auto const& item : s_pendingBadges[targetAccountId]) {
                    auto const& b = item.badge;
                    if (!b.user || !b.target) continue;
                    if (item.isAdminBadge && roles.isAdmin) {
                        dasshu::badgified::showBadge(b, CCSprite::createWithSpriteFrameName("CB_admin_badge.png"_spr));
                    } else if (!item.isAdminBadge && roles.isStaff) {
                        dasshu::badgified::showBadge(b, CCSprite::createWithSpriteFrameName("CB_staff_badge.png"_spr));
                    }
                }
                s_pendingBadges.erase(targetAccountId);
            }
        });
    });
}

$execute {
    dasshu::badgified::waitForBadgified([]() {
        dasshu::badgified::registerBadge(
            "cb-admin"_spr,
            "Comment Banners Admin",
            "This user is an <cr>Administrator</c> for <cp>Comment Banners</c>. They have the ability to <co>delete/modify</c> banners as well as <cg>promote users to Staff</c>",
            [](const dasshu::badgified::Badge& badge) {
                checkAndShowBadge(badge, true);
            });

        dasshu::badgified::registerBadge(
            "cb-staff"_spr,
            "Comment Banners Staff",
            "This user is a <cg>Staff</c> for <cp>Comment Banners</c>. They have the ability to <cg>approve pending banners</c> and <cy>feature banners</c>",
            [](const dasshu::badgified::Badge& badge) {
                checkAndShowBadge(badge, false);
            });
    });
}