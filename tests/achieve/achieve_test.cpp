#include <doctest/doctest.h>

#include "achieve/achievement.hpp"

using treelang::AchievementDef;
using treelang::AchievementEvent;
using treelang::AchievementManager;
using treelang::AchievementRewardKind;
using treelang::AchievementTrigger;
using treelang::default_achievements;

static AchievementManager make_manager()
{
    AchievementManager mgr;
    for (const AchievementDef &def : default_achievements()) mgr.register_achievement(def);
    return mgr;
}

TEST_CASE("achieve: register and query")
{
    AchievementManager mgr = make_manager();
    CHECK(mgr.total_count() == 5);
    CHECK(mgr.unlocked_count() == 0);
    CHECK(mgr.find("combo_3") != nullptr);
    CHECK(mgr.find("combo_3")->target == 3);
    CHECK(mgr.find("missing") == nullptr);
    CHECK(!mgr.is_unlocked("combo_3"));
}

TEST_CASE("achieve: first elite kill unlocks once")
{
    AchievementManager mgr = make_manager();
    auto unlocked = mgr.report({AchievementTrigger::FirstEliteKill, 0});
    REQUIRE(unlocked.size() == 1);
    CHECK(unlocked[0] == "first_elite_kill");
    CHECK(mgr.is_unlocked("first_elite_kill"));
    CHECK(mgr.unlocked_count() == 1);

    auto again = mgr.report({AchievementTrigger::FirstEliteKill, 0});
    CHECK(again.empty());
    CHECK(mgr.unlocked_count() == 1);
}

TEST_CASE("achieve: combo threshold unlocks progressively")
{
    AchievementManager mgr = make_manager();
    CHECK(mgr.report({AchievementTrigger::ComboCount, 2}).empty());
    CHECK(!mgr.is_unlocked("combo_3"));

    auto r3 = mgr.report({AchievementTrigger::ComboCount, 3});
    REQUIRE(r3.size() == 1);
    CHECK(r3[0] == "combo_3");

    auto r5 = mgr.report({AchievementTrigger::ComboCount, 5});
    REQUIRE(r5.size() == 1);
    CHECK(r5[0] == "combo_5");
    CHECK(mgr.is_unlocked("combo_3"));
    CHECK(mgr.is_unlocked("combo_5"));
}

TEST_CASE("achieve: floor reached threshold")
{
    AchievementManager mgr = make_manager();
    CHECK(mgr.report({AchievementTrigger::FloorReached, 2}).empty());
    auto r = mgr.report({AchievementTrigger::FloorReached, 3});
    REQUIRE(r.size() == 1);
    CHECK(r[0] == "floor_3");
}

TEST_CASE("achieve: fusion event triggers")
{
    AchievementManager mgr = make_manager();
    auto r = mgr.report({AchievementTrigger::FirstFusion, 0});
    REQUIRE(r.size() == 1);
    CHECK(r[0] == "first_fusion");
}

TEST_CASE("achieve: direct unlock")
{
    AchievementManager mgr = make_manager();
    CHECK(!mgr.unlock("missing"));
    CHECK(mgr.unlock("combo_3"));
    CHECK(!mgr.unlock("combo_3"));
    CHECK(mgr.is_unlocked("combo_3"));
    CHECK(mgr.unlocked_count() == 1);
}

TEST_CASE("achieve: reward metadata")
{
    const AchievementDef *first = nullptr;
    const AchievementDef *combo3 = nullptr;
    for (const AchievementDef &def : default_achievements())
    {
        if (def.id == "first_elite_kill") first = &def;
        if (def.id == "combo_3") combo3 = &def;
    }
    REQUIRE(first != nullptr);
    REQUIRE(combo3 != nullptr);

    CHECK(first->reward.kind == AchievementRewardKind::ExtraAction);
    CHECK(first->reward.extra_spells == 1);

    CHECK(combo3->reward.kind == AchievementRewardKind::Title);
    CHECK(combo3->reward.title == "流之舞者");
}

TEST_CASE("achieve: re-register overwrites definition")
{
    AchievementManager mgr = make_manager();
    AchievementDef def = default_achievements()[0];
    def.name = "改名";
    mgr.register_achievement(def);
    CHECK(mgr.find("first_elite_kill")->name == "改名");
    CHECK(mgr.total_count() == 5);
}