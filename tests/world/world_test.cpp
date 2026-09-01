#include <doctest/doctest.h>

#include "core/rng.hpp"
#include "core/types.hpp"
#include "model/entity.hpp"
#include "model/room.hpp"
#include "world/mapgen.hpp"

using treelang::MapGrid;
using treelang::Rng;
using treelang::RoomType;
using treelang::WorldConfig;
using treelang::WorldError;
using treelang::generate_floor;
using treelang::is_center;
using treelang::kCenterCol;
using treelang::kCenterRow;
using treelang::kMapSize;
using treelang::make_enemy;
using treelang::make_item;
using treelang::make_npc;
using treelang::make_spell;
using treelang::reward_gold;

static int count_type(const MapGrid &grid, RoomType type)
{
    int n = 0;
    for (std::size_t r = 0; r < kMapSize; ++r)
        for (std::size_t c = 0; c < kMapSize; ++c)
            if (grid[r][c].type == type) ++n;
    return n;
}

TEST_CASE("world: center is the start room")
{
    Rng rng{42};
    auto res = generate_floor(1, rng);
    REQUIRE(res.is_ok());
    const auto &grid = res.unwrap();
    CHECK(grid[kCenterRow][kCenterCol].type == RoomType::Start);
    CHECK(is_center(kCenterRow, kCenterCol));
    CHECK(!is_center(0, 0));
}

TEST_CASE("world: room type counts match config")
{
    Rng rng{7};
    WorldConfig cfg;
    cfg.elite_rooms = 4;
    cfg.function_rooms = 3;
    cfg.story_rooms = 3;

    auto res = generate_floor(2, rng, cfg);
    REQUIRE(res.is_ok());
    const auto &grid = res.unwrap();
    CHECK(count_type(grid, RoomType::Start) == 1);
    CHECK(count_type(grid, RoomType::Elite) == 4);
    CHECK(count_type(grid, RoomType::Function) == 3);
    CHECK(count_type(grid, RoomType::Story) == 3);
    CHECK(count_type(grid, RoomType::Reward) == 14);  // 24 - 10
}

TEST_CASE("world: elite rooms hold floor-scaled enemies")
{
    Rng rng{11};
    auto res = generate_floor(1, rng);
    REQUIRE(res.is_ok());
    const auto &grid = res.unwrap();

    bool found_elite = false;
    for (std::size_t r = 0; r < kMapSize; ++r)
        for (std::size_t c = 0; c < kMapSize; ++c)
            if (grid[r][c].type == RoomType::Elite)
            {
                found_elite = true;
                CHECK(!grid[r][c].content.enemies.empty());
                CHECK(grid[r][c].content.enemies[0].max_hp >= 1);
                CHECK(grid[r][c].content.enemies[0].hp == grid[r][c].content.enemies[0].max_hp);
                CHECK(!grid[r][c].content.enemies[0].weaknesses.empty());
                CHECK(grid[r][c].content.gold_reward > 0);
            }
    CHECK(found_elite);
}

TEST_CASE("world: enemies grow stronger with floor")
{
    Rng rng{3};
    WorldConfig cfg;
    auto res1 = generate_floor(1, rng, cfg);
    auto res3 = generate_floor(3, rng, cfg);
    REQUIRE(res1.is_ok());
    REQUIRE(res3.is_ok());
    const auto &g1 = res1.unwrap();
    const auto &g3 = res3.unwrap();

    auto first_elite_hp = [](const MapGrid &g) -> int
    {
        for (std::size_t r = 0; r < kMapSize; ++r)
            for (std::size_t c = 0; c < kMapSize; ++c)
                if (g[r][c].type == RoomType::Elite && !g[r][c].content.enemies.empty())
                    return g[r][c].content.enemies[0].max_hp;
        return -1;
    };
    auto first_elite_power = [](const MapGrid &g) -> int
    {
        for (std::size_t r = 0; r < kMapSize; ++r)
            for (std::size_t c = 0; c < kMapSize; ++c)
                if (g[r][c].type == RoomType::Elite && !g[r][c].content.enemies.empty())
                    return g[r][c].content.enemies[0].power;
        return -1;
    };

    CHECK(first_elite_hp(g3) > first_elite_hp(g1));
    CHECK(first_elite_power(g3) > first_elite_power(g1));
}

TEST_CASE("world: reward rooms carry gold, loot and sometimes spells")
{
    Rng rng{9};
    WorldConfig cfg;
    auto res = generate_floor(2, rng, cfg);
    REQUIRE(res.is_ok());
    const auto &grid = res.unwrap();

    bool found_reward = false;
    for (std::size_t r = 0; r < kMapSize; ++r)
        for (std::size_t c = 0; c < kMapSize; ++c)
            if (grid[r][c].type == RoomType::Reward)
            {
                found_reward = true;
                CHECK(grid[r][c].content.gold_reward == 40 + 20 * 1);  // 楼层 2
                CHECK(!grid[r][c].content.loot.empty());
            }
    CHECK(found_reward);
}

TEST_CASE("world: story rooms have npc, service rooms stay empty")
{
    Rng rng{5};
    auto res = generate_floor(1, rng);
    REQUIRE(res.is_ok());
    const auto &grid = res.unwrap();

    for (std::size_t r = 0; r < kMapSize; ++r)
    {
        for (std::size_t c = 0; c < kMapSize; ++c)
        {
            const auto &room = grid[r][c];
            if (room.type == RoomType::Story)
                CHECK(!room.content.npcs.empty());
            if (room.type == RoomType::Function || room.type == RoomType::Start)
            {
                CHECK(room.content.enemies.empty());
                CHECK(room.content.npcs.empty());
                CHECK(room.content.loot.empty());
                CHECK(room.content.spell_rewards.empty());
                CHECK(room.content.gold_reward == 0);
            }
        }
    }
}

TEST_CASE("world: invalid config rejected")
{
    Rng rng{1};
    WorldConfig cfg;
    cfg.elite_rooms = 20;
    cfg.function_rooms = 10;
    cfg.story_rooms = 10;
    auto res = generate_floor(1, rng, cfg);
    CHECK(res.is_err());
    CHECK(res.unwrap_err() == WorldError::InvalidConfig);
}

TEST_CASE("world: empty template pools rejected")
{
    Rng rng{1};
    WorldConfig cfg;
    cfg.elite_names.clear();
    auto res = generate_floor(1, rng, cfg);
    CHECK(res.is_err());
    CHECK(res.unwrap_err() == WorldError::EmptyPools);
}

TEST_CASE("world: same seed produces identical map")
{
    Rng a{5};
    Rng b{5};
    auto res_a = generate_floor(1, a);
    auto res_b = generate_floor(1, b);
    REQUIRE(res_a.is_ok());
    REQUIRE(res_b.is_ok());
    const auto &g1 = res_a.unwrap();
    const auto &g2 = res_b.unwrap();

    for (std::size_t r = 0; r < kMapSize; ++r)
    {
        for (std::size_t c = 0; c < kMapSize; ++c)
        {
            CHECK(g1[r][c].type == g2[r][c].type);
            CHECK(g1[r][c].content.enemies.size() == g2[r][c].content.enemies.size());
            CHECK(g1[r][c].content.npcs.size() == g2[r][c].content.npcs.size());
            CHECK(g1[r][c].content.loot.size() == g2[r][c].content.loot.size());
            CHECK(g1[r][c].content.spell_rewards.size() == g2[r][c].content.spell_rewards.size());
            CHECK(g1[r][c].content.gold_reward == g2[r][c].content.gold_reward);
        }
    }
}

TEST_CASE("world: content builders produce valid values")
{
    Rng rng{2};
    WorldConfig cfg;

    auto enemy = make_enemy(rng, cfg, true, 1);
    CHECK(!enemy.name.empty());
    CHECK(enemy.max_hp > 0);
    CHECK(enemy.power > 0);
    CHECK(!enemy.weaknesses.empty());

    auto npc = make_npc(rng, cfg);
    CHECK(!npc.name.empty());

    auto item = make_item(rng, cfg);
    CHECK(!item.name.empty());
    CHECK(item.kind == treelang::ItemKind::Consumable);

    auto spell = make_spell(rng, cfg, 5);
    CHECK(!spell.id.empty());
    CHECK(!spell.name.empty());
    CHECK(!spell.elements.empty());
    CHECK(spell.base_multiplier == 100);

    CHECK(reward_gold(1, cfg) == 40);
    CHECK(reward_gold(3, cfg) == 80);
}