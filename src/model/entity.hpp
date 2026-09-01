/**
 * @file entity.hpp
 * @brief 玩家 / 敌人 / NPC 数据模型。
 */

#ifndef INCLUDE_TREELANG_MODEL_ENTITY_HPP
#define INCLUDE_TREELANG_MODEL_ENTITY_HPP

#include <string>
#include <vector>

#include "core/element.hpp"
#include "item.hpp"
#include "spell.hpp"

namespace treelang
{
    /**
     * @brief S.P.E.C.I.A.L. 基础属性。
     *
     * 七项基础属性随等级提升获得属性点进行分配，每项属性上限为
     * kAttributeCap（20）。各项属性分别影响战斗、交互与奖励品质。
     */
    struct SpecialAttributes
    {
        int strength = 0;     /**< 力量：决定基础攻击力加成 */
        int perception = 0;   /**< 感知：决定能否查看敌人的具体数值 */
        int endurance = 0;    /**< 耐力：决定护盾上限与恢复速度 */
        int charisma = 0;     /**< 魅力：决定对话成功率与商店折扣比例 */
        int intelligence = 0; /**< 智力：决定 SAN 值上限与 SAN 恢复效率 */
        int agility = 0;      /**< 敏捷：决定对话失败闪避率与战斗逃跑成功率 */
        int luck = 0;         /**< 运气：影响奖励品质与随机事件倾向 */
    };

    /**
     * @brief 战斗/生存状态。
     *
     * 血量是唯一败北条件，归零立即结束游戏；理智归零不结束游戏但进入
     * 失控状态；护盾优先于血量抵消伤害。数值边界（如 SAN ∈ [kSanMin,
     * kSanMax]）由游戏规则维护，本模型仅保存数值。
     */
    struct VitalStatus
    {
        int hp = 0;     /**< 血量：归零即败北，不随时间自动恢复 */
        int mana = 0;   /**< 咒力：释放术式的消耗资源，可缓慢恢复 */
        int san = 0;    /**< 理智：范围 [0,100]，归零进入失控状态 */
        int shield = 0; /**< 护盾：与血量 1:1 抵消伤害，非战斗缓慢恢复 */
    };

    /**
     * @brief 玩家。
     *
     * 玩家是游戏中的唯一可操作实体，包含生存状态、S.P.E.C.I.A.L. 属性、
     * 等级与资源，以及持有的术式列表与道具栏。
     */
    struct Player
    {
        VitalStatus status;          /**< 生存状态 */
        SpecialAttributes special;   /**< S.P.E.C.I.A.L. 属性 */
        int level = 1;               /**< 等级 */
        int attribute_points = 0;    /**< 未分配的属性点 */
        int gold = 0;                /**< 金币 */
        SpellBook spells;            /**< 术式列表 */
        std::vector<Item> inventory; /**< 道具栏 */
    };

    /**
     * @brief 敌人。
     *
     * 是否触发【黑闪】取决于弱点元素集合：术式任一元素命中即触发。
     * 敌人强度随楼层提升，由地图生成阶段依据楼层调整 hp 与 power。
     */
    struct Enemy
    {
        std::string name;                /**< 名称 */
        int hp = 0;                      /**< 当前血量 */
        int max_hp = 0;                  /**< 血量上限 */
        int san = 0;                     /**< 理智：参与伤害造成的理智扣除 */
        int power = 0;                   /**< 攻击强度：决定造成伤害，随楼层增强 */
        std::vector<Element> weaknesses; /**< 弱点元素集合 */
    };

    /**
     * @brief NPC。
     *
     * 剧情房中的对话角色。交谈时理智高的一方会向低的一方转移少量理智，
     * 玩家与 NPC 的理智高低同时影响交谈成功率。
     */
    struct Npc
    {
        std::string name; /**< 名称 */
        int san = 0;      /**< 理智值 */
    };
}

#endif  // INCLUDE_TREELANG_MODEL_ENTITY_HPP