/**
 * @file mapgen.hpp
 * @brief 5×5 楼层地图生成。
 *
 * 每层生成一张 5×5 网格地图：中央为安全的【初始补给点】（Start），
 * 其余格子按配置随机分配 精英怪房 / 功能房 / 剧情房，剩余为奖励房。
 * 网格中相邻格子均可通行，所有房间均可从中央到达。
 *
 * 房间内容随房间类型生成：精英怪房驻守随楼层增强的敌人（附高品质
 * 奖励），剧情房放置 NPC，奖励房提供金币/道具/术式奖励；初始房与
 * 功能房为服务型内容，由游戏流程按房间类型触发，不预置数据。
 */

#ifndef INCLUDE_TREELANG_WORLD_MAPGEN_HPP
#define INCLUDE_TREELANG_WORLD_MAPGEN_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <pjh_result.hpp>
#include <string>
#include <utility>
#include <vector>

#include "core/matrix.hpp"
#include "core/rng.hpp"
#include "core/types.hpp"
#include "model/entity.hpp"
#include "model/item.hpp"
#include "model/room.hpp"
#include "model/spell.hpp"

namespace treelang
{
    /**
     * @brief 地图生成错误。
     */
    enum class WorldError : std::uint8_t
    {
        InvalidConfig, /**< 房间数量配置无效（数量为负或超出可用格数） */
        EmptyPools,    /**< 内容模板池为空 */
    };

    /**
     * @brief 地图生成可调参数与内容模板池。
     *
     * 敌人强度、奖励数量随楼层增长的系数与基数，以及敌人/术式/道具/NPC
     * 的名称模板池均可在此调整；后续数据模块可将模板迁移到配置文件。
     */
    struct WorldConfig
    {
        // ---- 房间数量 ----
        int elite_rooms = 4;    /**< 精英怪房数量 */
        int function_rooms = 3; /**< 功能房（术式融合）数量 */
        int story_rooms = 3;    /**< 剧情房数量 */

        // ---- 敌人强度（随楼层提升） ----
        double floor_hp_factor = 1.25;    /**< 每层血量倍率 */
        double floor_power_factor = 1.25; /**< 每层攻击倍率 */
        int base_enemy_hp = 30;           /**< 1 层敌人基础血量 */
        int base_enemy_power = 5;         /**< 1 层敌人基础攻击 */
        int enemy_san_min = 30;           /**< 敌人理智随机范围下界 */
        int enemy_san_max = 60;           /**< 敌人理智随机范围上界 */

        // ---- 奖励 ----
        int reward_gold = 40;           /**< 奖励房基础金币 */
        int reward_gold_per_floor = 20; /**< 奖励房每层金币递增 */

        // ---- 内容模板池 ----
        std::vector<std::string> enemy_names = {
            "哥布林", "狼人", "石魔像", "暗影刺客", "腐尸守卫",
        }; /**< 普通敌人名称池 */
        std::vector<std::string> elite_names = {
            "精英·哥布林王",
            "精英·狼人统领",
            "精英·魔像之主",
            "精英·暗影领主",
        }; /**< 精英敌人名称池 */
        std::vector<std::string> spell_names = {
            "火球", "水刃", "金剑", "烈焰冲击", "寒冰斩", "锐金破",
        }; /**< 奖励术式名称池 */
        std::vector<std::string> item_names = {
            "恢复药水",
            "咒力药剂",
        }; /**< 奖励道具名称池 */
        std::vector<std::string> npc_names = {
            "旅商",
            "老学者",
            "神秘旅人",
        }; /**< 剧情房 NPC 名称池 */
    };

    /**
     * @brief 判断格子是否为中央初始房。
     * @param row 行下标。
     * @param col 列下标。
     * @return 中央返回 true。
     */
    inline bool is_center(std::size_t row, std::size_t col)
    {
        return row == kCenterRow && col == kCenterCol;
    }

    /**
     * @brief 计算指定楼层的奖励金币数。
     * @param floor 楼层（从 1 开始）。
     * @param config 地图参数。
     * @return 金币数。
     */
    inline int reward_gold(int floor, const WorldConfig &config)
    {
        return config.reward_gold + config.reward_gold_per_floor * (floor - 1);
    }

    /**
     * @brief 生成一名敌人（普通或精英）。
     *
     * 血量与攻击按楼层呈指数增长（基础值 × 倍率^(楼层-1)）；弱点元素
     * 从元素全集中随机取 1~2 个。
     *
     * @param rng 随机源。
     * @param config 地图参数。
     * @param elite 是否精英（名称与数值同源，仅名称区分）。
     * @param floor 楼层（从 1 开始）。
     * @return 生成的敌人。
     */
    inline Enemy make_enemy(Rng &rng, const WorldConfig &config, bool elite, int floor)
    {
        Enemy enemy;
        const std::vector<std::string> &names =
            elite ? config.elite_names : config.enemy_names;
        enemy.name = names[rng.uniform_int(0, static_cast<int>(names.size()) - 1)];

        double hp_factor = std::pow(config.floor_hp_factor, floor - 1);
        enemy.max_hp = static_cast<int>(std::round(config.base_enemy_hp * hp_factor));
        enemy.hp = enemy.max_hp;
        enemy.power = static_cast<int>(std::round(
            config.base_enemy_power * std::pow(config.floor_power_factor, floor - 1)));
        enemy.san = rng.uniform_int(config.enemy_san_min, config.enemy_san_max);

        constexpr std::array<Element, 3> elements = {
            Element::Fire, Element::Water, Element::Metal};
        std::vector<Element> pool(elements.begin(), elements.end());
        rng.shuffle(pool.begin(), pool.end());
        int weak_count = rng.uniform_int(1, 2);
        enemy.weaknesses.assign(pool.begin(), pool.begin() + weak_count);
        return enemy;
    }

    /**
     * @brief 生成一名 NPC。
     * @param rng 随机源。
     * @param config 地图参数。
     * @return 生成的 NPC。
     */
    inline Npc make_npc(Rng &rng, const WorldConfig &config)
    {
        Npc npc;
        npc.name = config.npc_names[rng.uniform_int(
            0, static_cast<int>(config.npc_names.size()) - 1)];
        npc.san = rng.uniform_int(config.enemy_san_min, config.enemy_san_max);
        return npc;
    }

    /**
     * @brief 生成一件奖励道具（消耗品）。
     * @param rng 随机源。
     * @param config 地图参数。
     * @return 生成的道具。
     */
    inline Item make_item(Rng &rng, const WorldConfig &config)
    {
        Item item;
        item.name = config.item_names[rng.uniform_int(
            0, static_cast<int>(config.item_names.size()) - 1)];
        item.kind = ItemKind::Consumable;
        if (item.name == config.item_names[0])
            item.effect.heal_hp = rng.uniform_int(10, 20);
        else
            item.effect.heal_mana = rng.uniform_int(10, 20);
        return item;
    }

    /**
     * @brief 生成一本奖励术式。
     *
     * 随机携带 1~2 种元素（等级 1~3）与冷却回合。稳定标识由名称与序号
     * 组成，保证同层内唯一。
     *
     * @param rng 随机源。
     * @param config 地图参数。
     * @param sequence 本层内的序号（用于生成唯一 id）。
     * @return 生成的术式。
     */
    inline Spell make_spell(Rng &rng, const WorldConfig &config, std::size_t sequence)
    {
        Spell spell;
        spell.name = config.spell_names[rng.uniform_int(
            0, static_cast<int>(config.spell_names.size()) - 1)];
        spell.id = spell.name + "#" + std::to_string(sequence);

        constexpr std::array<Element, 3> elements = {
            Element::Fire, Element::Water, Element::Metal};
        std::vector<Element> pool(elements.begin(), elements.end());
        rng.shuffle(pool.begin(), pool.end());
        int count = rng.uniform_int(1, 2);
        for (int i = 0; i < count; ++i)
            spell.elements.push_back(ElementAttr{pool[i], rng.uniform_int(1, 3)});

        spell.cooldown = rng.uniform_int(1, 3);
        spell.base_multiplier = 100;
        return spell;
    }

    /**
     * @brief 依据房间类型填充房间内容。
     *
     * - 精英怪房：驻守 1 名随楼层增强的精英敌人，并附高品质金币奖励；
     * - 剧情房：放置 1 名 NPC；
     * - 奖励房：金币 + 1~2 件道具，并有一定概率奖励新术式；
     * - 初始房 / 功能房：服务型房间，不预置数据。
     *
     * @param room 要填充的房间（可变引用）。
     * @param floor 楼层（从 1 开始）。
     * @param rng 随机源。
     * @param config 地图参数。
     * @param room_seq 本层内的房间序号（用于生成唯一术式 id）。
     */
    inline void populate_room(
        Room &room, int floor, Rng &rng, const WorldConfig &config, std::size_t room_seq)
    {
        switch (room.type)
        {
        case RoomType::Elite:
            room.content.enemies.push_back(make_enemy(rng, config, true, floor));
            room.content.gold_reward = reward_gold(floor, config) * 2;
            break;
        case RoomType::Story:
            room.content.npcs.push_back(make_npc(rng, config));
            break;
        case RoomType::Reward:
        {
            room.content.gold_reward = reward_gold(floor, config);
            int item_count = rng.uniform_int(1, 2);
            for (int i = 0; i < item_count; ++i)
                room.content.loot.push_back(make_item(rng, config));
            if (rng.chance(0.5))
                room.content.spell_rewards.push_back(make_spell(rng, config, room_seq));
            break;
        }
        case RoomType::Start:
        case RoomType::Function:
            break;
        }
    }

    /**
     * @brief 生成一层的 5×5 地图。
     *
     * 中央固定为初始房；其余格子随机分配房间类型，并按其类型填充内容。
     *
     * @param floor 楼层（从 1 开始）。
     * @param rng 随机源（相同种子产生相同地图）。
     * @param config 地图参数。
     * @return 地图网格；配置或模板无效时返回 WorldError。
     */
    inline pjh::result::Result<MapGrid, WorldError> generate_floor(
        int floor, Rng &rng, const WorldConfig &config = {})
    {
        using Res = pjh::result::Result<MapGrid, WorldError>;

        constexpr std::size_t total_cells = kMapSize * kMapSize;
        if (config.elite_rooms < 0 || config.function_rooms < 0 || config.story_rooms < 0)
            return Res::Err(WorldError::InvalidConfig);
        const std::size_t reserved = static_cast<std::size_t>(
            config.elite_rooms + config.function_rooms + config.story_rooms);
        if (reserved > total_cells - 1)
            return Res::Err(WorldError::InvalidConfig);

        if (config.enemy_names.empty() || config.elite_names.empty() ||
            config.spell_names.empty() || config.item_names.empty() ||
            config.npc_names.empty())
            return Res::Err(WorldError::EmptyPools);

        // 收集非中央格子并随机打乱
        std::vector<std::pair<std::size_t, std::size_t>> positions;
        positions.reserve(total_cells - 1);
        for (std::size_t row = 0; row < kMapSize; ++row)
            for (std::size_t col = 0; col < kMapSize; ++col)
                if (!is_center(row, col))
                    positions.push_back({row, col});
        rng.shuffle(positions.begin(), positions.end());

        MapGrid grid;
        std::size_t idx = 0;
        auto assign = [&](RoomType type, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                auto [row, col] = positions[idx++];
                grid[row][col].type = type;
            }
        };
        assign(RoomType::Elite, config.elite_rooms);
        assign(RoomType::Function, config.function_rooms);
        assign(RoomType::Story, config.story_rooms);
        while (idx < positions.size())
        {
            auto [row, col] = positions[idx++];
            grid[row][col].type = RoomType::Reward;
        }

        // 按房间类型填充内容
        std::size_t room_seq = 0;
        for (std::size_t row = 0; row < kMapSize; ++row)
            for (std::size_t col = 0; col < kMapSize; ++col)
                populate_room(grid[row][col], floor, rng, config, room_seq++);

        return Res::Ok(grid);
    }
}

#endif  // INCLUDE_TREELANG_WORLD_MAPGEN_HPP