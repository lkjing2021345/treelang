/**
 * @file achievement.hpp
 * @brief 成就定义与解锁管理。
 *
 * 玩家达成特定条件时解锁成就；成就奖励为永久性，全存档通用。
 * 本模块仅负责定义与解锁记录，奖励的实际应用（额外行动、称号属性
 * 加成、装扮表现）由游戏流程按查询结果处理。
 */

#ifndef INCLUDE_TREELANG_ACHIEVE_ACHIEVEMENT_HPP
#define INCLUDE_TREELANG_ACHIEVE_ACHIEVEMENT_HPP

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace treelang
{
    /**
     * @brief 成就触发类型。
     */
    enum class AchievementTrigger : std::uint8_t
    {
        FirstEliteKill, /**< 首次击杀精英怪 */
        ComboCount,     /**< 心流连击数达到 target */
        FloorReached,   /**< 到达第 target 层 */
        FirstFusion,    /**< 首次进行术式融合 */
    };

    /**
     * @brief 成就奖励类型。
     */
    enum class AchievementRewardKind : std::uint8_t
    {
        ExtraAction, /**< 额外可执行行动（如开局额外术式） */
        Title,       /**< 特殊称号（影响玩家属性系统） */
        Cosmetic,    /**< 装扮/特效（视觉表现层） */
    };

    /**
     * @brief 成就奖励。
     *
     * 由 kind 决定哪个字段有效：ExtraAction 使用 extra_spells，
     * Title 使用 title，Cosmetic 使用 cosmetic。
     */
    struct AchievementReward
    {
        AchievementRewardKind kind = AchievementRewardKind::Title; /**< 奖励类型 */
        std::string title;    /**< 称号名（kind == Title 时有效） */
        std::string cosmetic; /**< 装扮/特效标识（kind == Cosmetic 时有效） */
        int extra_spells = 0; /**< 开局额外术式数量（kind == ExtraAction 时有效） */
    };

    /**
     * @brief 成就定义。
     */
    struct AchievementDef
    {
        std::string id;          /**< 稳定标识，用于解锁记录与存档 */
        std::string name;        /**< 显示名 */
        std::string description; /**< 描述 */
        AchievementTrigger trigger = AchievementTrigger::FirstEliteKill; /**< 触发类型 */
        int target = 0;           /**< 触发参数：连击数/楼层数阈值 */
        AchievementReward reward; /**< 解锁奖励 */
    };

    /**
     * @brief 游戏事件，用于成就条件判定。
     */
    struct AchievementEvent
    {
        AchievementTrigger trigger = AchievementTrigger::FirstEliteKill; /**< 事件类型 */
        int value = 0; /**< 事件参数（如连击数 / 楼层数） */
    };

    /**
     * @class AchievementManager
     * @brief 成就管理器。
     *
     * 持有成就定义与已解锁集合。游戏流程通过 report() 上报事件，
     * 满足条件的成就自动解锁并返回本次新解锁的 id；已解锁集合
     * 通过 unlocked_ids() 暴露给存档模块持久化（永久性、全存档通用）。
     */
    class AchievementManager
    {
    public:
        /**
         * @brief 注册成就定义；id 已存在时覆盖定义（保留解锁状态）。
         * @param def 成就定义。
         */
        void register_achievement(AchievementDef def)
        {
            auto it = std::find_if(
                m_defs.begin(), m_defs.end(),
                [&](const AchievementDef &d) { return d.id == def.id; });
            if (it != m_defs.end())
                *it = std::move(def);
            else
                m_defs.push_back(std::move(def));
        }

        /**
         * @brief 上报游戏事件，判定并解锁匹配的成就。
         * @param event 事件。
         * @return 本次新解锁的成就 id 列表（可为空）。
         */
        std::vector<std::string> report(const AchievementEvent &event)
        {
            std::vector<std::string> unlocked;
            for (const AchievementDef &def : m_defs)
            {
                if (m_unlocked.count(def.id) > 0)
                    continue;
                if (def.trigger != event.trigger)
                    continue;

                bool meets = false;
                switch (def.trigger)
                {
                case AchievementTrigger::FirstEliteKill:
                case AchievementTrigger::FirstFusion:
                    meets = true;
                    break;
                case AchievementTrigger::ComboCount:
                case AchievementTrigger::FloorReached:
                    meets = event.value >= def.target;
                    break;
                }
                if (meets)
                {
                    m_unlocked.insert(def.id);
                    unlocked.push_back(def.id);
                }
            }
            return unlocked;
        }

        /**
         * @brief 直接解锁指定成就（用于存档恢复等场景）。
         * @param id 成就稳定标识。
         * @return 未知 id 或已解锁返回 false；本次新解锁返回 true。
         */
        bool unlock(const std::string &id)
        {
            if (!find(id))
                return false;
            return m_unlocked.insert(id).second;
        }

        /**
         * @brief 成就是否已解锁。
         * @param id 成就稳定标识。
         */
        bool is_unlocked(const std::string &id) const { return m_unlocked.count(id) > 0; }

        /**
         * @brief 按稳定标识查找成就定义。
         * @param id 成就稳定标识。
         * @return 指向定义的常量指针；不存在返回 nullptr。
         */
        const AchievementDef *find(const std::string &id) const
        {
            auto it = std::find_if(
                m_defs.begin(), m_defs.end(),
                [&](const AchievementDef &d) { return d.id == id; });
            return it == m_defs.end() ? nullptr : &*it;
        }

        /** @brief 全部成就定义。 */
        const std::vector<AchievementDef> &definitions() const { return m_defs; }

        /** @brief 已解锁成就 id 集合（供存档持久化）。 */
        const std::unordered_set<std::string> &unlocked_ids() const { return m_unlocked; }

        /** @brief 已解锁数量。 */
        std::size_t unlocked_count() const { return m_unlocked.size(); }

        /** @brief 全部成就数量。 */
        std::size_t total_count() const { return m_defs.size(); }

    private:
        std::vector<AchievementDef> m_defs;         /**< 成就定义 */
        std::unordered_set<std::string> m_unlocked; /**< 已解锁 id 集合 */
    };

    /**
     * @brief 内置成就定义集（基于 README 示例）。
     *
     * 首次击杀精英怪与首次融合奖励额外术式；连击/楼层成就授予称号或装扮。
     *
     * @return 成就定义列表。
     */
    inline std::vector<AchievementDef> default_achievements()
    {
        return {
            AchievementDef{
                "first_elite_kill", "猎首", "首次击败精英怪",
                AchievementTrigger::FirstEliteKill, 0,
                AchievementReward{AchievementRewardKind::ExtraAction, {}, {}, 1}},
            AchievementDef{
                "combo_3", "流之舞者", "达成 3 连击", AchievementTrigger::ComboCount, 3,
                AchievementReward{AchievementRewardKind::Title, "流之舞者", {}, 0}},
            AchievementDef{
                "combo_5", "心流大师", "达成 5 连击", AchievementTrigger::ComboCount, 5,
                AchievementReward{AchievementRewardKind::Title, "心流大师", {}, 0}},
            AchievementDef{
                "floor_3", "深渊旅人", "抵达第 3 层", AchievementTrigger::FloorReached, 3,
                AchievementReward{
                    AchievementRewardKind::Cosmetic, {}, "crimson_aura", 0}},
            AchievementDef{
                "first_fusion", "炼术师", "首次进行术式融合",
                AchievementTrigger::FirstFusion, 0,
                AchievementReward{AchievementRewardKind::ExtraAction, {}, {}, 1}},
        };
    }
}

#endif  // INCLUDE_TREELANG_ACHIEVE_ACHIEVEMENT_HPP