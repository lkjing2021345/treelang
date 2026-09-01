/**
 * @file room.hpp
 * @brief 房间数据模型。
 */

#ifndef INCLUDE_TREELANG_MODEL_ROOM_HPP
#define INCLUDE_TREELANG_MODEL_ROOM_HPP

#include <vector>

#include "core/matrix.hpp"
#include "core/types.hpp"
#include "entity.hpp"
#include "item.hpp"
#include "spell.hpp"

namespace treelang
{
    /**
     * @brief 房间内容。
     *
     * 由房间类型决定哪些字段非空：
     * - 精英怪房：enemies 驻守敌人；
     * - 剧情房：npcs 触发对话；
     * - 奖励房：loot / spell_rewards / gold_reward。
     *
     * 初始房（商店）与功能房（术式融合）属于服务型内容，由游戏流程
     * 按房间类型触发，无需持久化数据。
     */
    struct RoomContent
    {
        std::vector<Enemy> enemies;       /**< 驻守敌人（精英怪房） */
        std::vector<Npc> npcs;            /**< NPC（剧情房） */
        std::vector<Item> loot;           /**< 奖励道具（奖励房） */
        std::vector<Spell> spell_rewards; /**< 奖励术式（奖励房） */
        int gold_reward = 0;              /**< 金币奖励（奖励房） */
    };

    /**
     * @brief 房间。
     *
     * 5×5 网格中的一个格子，由房间类型与内容组成。
     */
    struct Room
    {
        RoomType type = RoomType::Start; /**< 房间类型 */
        RoomContent content;             /**< 房间内容 */
    };

    /** 一层楼的地图网格：5×5 的房间矩阵 */
    using MapGrid = Matrix<Room, kMapSize, kMapSize>;
}

#endif  // INCLUDE_TREELANG_MODEL_ROOM_HPP