/**
 * @file battle.hpp
 * @brief 战斗回合编排。
 */

#ifndef INCLUDE_TREELANG_COMBAT_BATTLE_HPP
#define INCLUDE_TREELANG_COMBAT_BATTLE_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <pjh_result.hpp>

#include "core/rng.hpp"
#include "model/entity.hpp"
#include "model/item.hpp"
#include "combat/damage.hpp"
#include "combat/flow_state.hpp"
#include "combat/mana.hpp"

namespace treelang
{
    /**
     * @brief 战斗错误。
     */
    enum class CombatError : std::uint8_t
    {
        SpellOnCooldown,  /**< 术式处于冷却中 */
        InsufficientMana, /**< 咒力不足 */
        NoSuchSpell,      /**< 术式不在玩家列表中 */
        BattleOver,       /**< 战斗已结束 */
        ItemNotUsable,    /**< 道具不可用（非消耗品或不在道具栏中） */
    };

    /**
     * @class Battle
     * @brief 单场战斗的编排者。
     *
     * 负责玩家与单个敌人之间的回合循环：玩家施放术式 / 使用道具 / 逃跑，
     * 敌人回合造成伤害。护盾优先抵消伤害，剩余伤害扣除血量并造成理智损失
     * （损失量与损失血量及敌我理智差值有关）。
     *
     * 典型用法：
     * @code
     * while (!battle.is_over())
     * {
     *     battle.begin_round();                        // 回合开始
     *     auto r = battle.player_cast(spell);          // 玩家行动
     *     if (r.is_err() || battle.is_over()) continue;
     *     auto enemy = battle.enemy_act();             // 敌人行动
     * }
     * @endcode
     */
    class Battle
    {
    public:
        /** @brief 战斗可调参数 */
        struct Config
        {
            DamageConfig damage;             /**< 伤害参数 */
            ManaConfig mana;                 /**< 咒力参数 */
            double san_hp_factor = 0.5;      /**< 每损失 1 点血量扣除的理智 */
            double san_diff_factor = 0.2;    /**< 敌我理智差值对理智扣除的影响 */
            double flee_base = 0.5;          /**< 基础逃跑成功率 */
            double flee_agility_factor = 0.02; /**< 每点敏捷的逃跑成功率加成 */
            int enemy_damage_variance = 2;   /**< 敌人攻击浮动（±） */
        };

        /** @brief 玩家行动结果 */
        struct PlayerActionResult
        {
            bool turn_consumed = false;         /**< 是否消耗了玩家回合 */
            std::optional<DamageReport> damage; /**< 施术伤害明细（施术时有效） */
            int damage_dealt = 0;               /**< 实际造成伤害（含心流倍率） */
            int mana_cost = 0;                  /**< 本次消耗的咒力 */
            FlowState::TurnResult flow;         /**< 心流状态转变 */
            bool enemy_defeated = false;        /**< 敌人是否被击败 */
            bool fled = false;                  /**< 是否成功逃跑 */
        };

        /** @brief 敌人行动结果 */
        struct EnemyAction
        {
            int raw_damage = 0;       /**< 敌人造成的原始伤害 */
            int shield_absorbed = 0;  /**< 护盾吸收量 */
            int hp_lost = 0;          /**< 实际扣除血量 */
            int san_lost = 0;         /**< 理智损失 */
            bool player_died = false; /**< 玩家血量是否归零 */
        };

        /**
         * @brief 构造战斗（使用默认参数）。
         * @param player 玩家（可变引用）。
         * @param enemy 敌人（可变引用）。
         * @param rng 随机源（测试注入固定种子以获得确定性）。
         */
        Battle(Player &player, Enemy &enemy, Rng &rng)
            : m_player(player), m_enemy(enemy), m_rng(rng)
        {
        }

        /**
         * @brief 构造战斗。
         * @param player 玩家（可变引用）。
         * @param enemy 敌人（可变引用）。
         * @param rng 随机源（测试注入固定种子以获得确定性）。
         * @param config 战斗参数。
         */
        Battle(Player &player, Enemy &enemy, Rng &rng, const Config &config)
            : m_player(player), m_enemy(enemy), m_rng(rng), m_config(config)
        {
        }

        /**
         * @brief 回合开始：激活心流（按需）、递减冷却、恢复咒力。
         *
         * 每个玩家回合前必须调用一次。
         */
        void begin_round()
        {
            m_flow.begin_round();
            for (auto &[id, cd] : m_cooldowns)
                if (cd > 0) --cd;
            m_player.status.mana += mana_regen_per_turn(m_config.mana);
        }

        /**
         * @brief 玩家施放术式。
         * @param spell 要施放的术式（须在玩家列表中）。
         * @return 行动结果；失败见 CombatError。
         */
        pjh::result::Result<PlayerActionResult, CombatError> player_cast(const Spell &spell)
        {
            using Res = pjh::result::Result<PlayerActionResult, CombatError>;
            if (m_over) return Res::Err(CombatError::BattleOver);
            if (!m_player.spells.find(spell.id)) return Res::Err(CombatError::NoSuchSpell);
            if (cooldown_of(spell.id) > 0) return Res::Err(CombatError::SpellOnCooldown);

            int cost = spell_mana_cost(spell, m_flow, m_config.mana);
            if (m_player.status.mana < cost) return Res::Err(CombatError::InsufficientMana);
            m_player.status.mana -= cost;

            m_cooldowns[spell.id] = spell.cooldown;

            int attack = base_attack(m_player.special, m_config.damage);
            DamageReport report = resolve_spell_damage(spell, attack, m_enemy, m_config.damage);
            int dealt = scale_damage(report.total_damage, m_flow.damage_multiplier());

            m_enemy.hp = std::max(0, m_enemy.hp - dealt);

            // 推进心流状态；熔断时对参与链路术式施加冷却惩罚
            FlowState::TurnResult flow = m_flow.on_cast({&spell, report.activated});
            if (flow.kind == FlowState::Kind::Broken)
            {
                for (const std::string &id : flow.meltdown_targets)
                    m_cooldowns[id] += flow.extra_cooldown;
            }

            PlayerActionResult result;
            result.turn_consumed = true;
            result.damage = report;
            result.damage_dealt = dealt;
            result.mana_cost = cost;
            result.flow = flow;
            result.enemy_defeated = (m_enemy.hp == 0);
            if (result.enemy_defeated)
            {
                m_over = true;
                m_player_won = true;
            }
            return Res::Ok(result);
        }

        /**
         * @brief 玩家使用道具（消耗品）。
         * @param index 道具栏下标。
         * @return 行动结果；失败见 CombatError。
         */
        pjh::result::Result<PlayerActionResult, CombatError> player_use_item(std::size_t index)
        {
            using Res = pjh::result::Result<PlayerActionResult, CombatError>;
            if (m_over) return Res::Err(CombatError::BattleOver);
            if (index >= m_player.inventory.size()) return Res::Err(CombatError::ItemNotUsable);

            const Item &item = m_player.inventory[index];
            if (item.kind != ItemKind::Consumable) return Res::Err(CombatError::ItemNotUsable);

            m_player.status.hp += item.effect.heal_hp;
            m_player.status.mana += item.effect.heal_mana;
            m_player.inventory.erase(m_player.inventory.begin() + static_cast<std::ptrdiff_t>(index));

            PlayerActionResult result;
            result.turn_consumed = true;
            return Res::Ok(result);
        }

        /**
         * @brief 玩家尝试逃跑。
         *
         * 成功率由敏捷决定（基础成功率 + 敏捷加成，封顶 100%）；成功则
         * 战斗直接结束。
         *
         * @return 行动结果；失败见 CombatError。
         */
        pjh::result::Result<PlayerActionResult, CombatError> player_flee()
        {
            using Res = pjh::result::Result<PlayerActionResult, CombatError>;
            if (m_over) return Res::Err(CombatError::BattleOver);

            double rate = m_config.flee_base
                + m_player.special.agility * m_config.flee_agility_factor;
            rate = std::clamp(rate, 0.0, 1.0);

            PlayerActionResult result;
            result.turn_consumed = true;
            if (m_rng.chance(rate))
            {
                result.fled = true;
                m_over = true;
            }
            return Res::Ok(result);
        }

        /**
         * @brief 敌人行动：对玩家造成伤害并结算护盾、血量与理智。
         * @return 行动结果；战斗已结束时返回空结果。
         */
        EnemyAction enemy_act()
        {
            EnemyAction action;
            if (m_over) return action;

            int variance =
                m_rng.uniform_int(-m_config.enemy_damage_variance, m_config.enemy_damage_variance);
            action.raw_damage = std::max(0, m_enemy.power + variance);

            // 护盾优先抵消，剩余伤害直接扣除血量
            action.shield_absorbed = std::min(m_player.status.shield, action.raw_damage);
            action.hp_lost = action.raw_damage - action.shield_absorbed;
            m_player.status.shield -= action.shield_absorbed;
            m_player.status.hp = std::max(0, m_player.status.hp - action.hp_lost);

            // 理智扣除：仅在受到血量伤害时扣除，扣除量受损失血量与敌我理智差值影响
            if (action.hp_lost > 0)
            {
                double san_loss = action.hp_lost * m_config.san_hp_factor
                    + std::max(0, m_enemy.san - m_player.status.san) * m_config.san_diff_factor;
                action.san_lost = static_cast<int>(std::round(san_loss));
                m_player.status.san = std::max(0, m_player.status.san - action.san_lost);
            }

            if (m_player.status.hp == 0)
            {
                m_over = true;
                m_player_died = true;
                action.player_died = true;
            }
            return action;
        }

        /** @brief 战斗是否已结束（胜负已分或逃跑成功）。 */
        bool is_over() const { return m_over; }

        /** @brief 玩家是否获胜（敌人被击败）。 */
        bool player_won() const { return m_player_won; }

        /** @brief 玩家是否死亡（血量归零）。 */
        bool player_died() const { return m_player_died; }

        /** @brief 当前心流状态。 */
        const FlowState &flow() const { return m_flow; }

        /** @brief 返回当前参数。 */
        const Config &config() const { return m_config; }

        /**
         * @brief 查询术式剩余冷却回合。
         * @param id 术式稳定标识。
         * @return 剩余冷却回合数；未在冷却中返回 0。
         */
        int cooldown_of(const std::string &id) const
        {
            auto it = m_cooldowns.find(id);
            return it == m_cooldowns.end() ? 0 : it->second;
        }

    private:
        Player &m_player;                  /**< 玩家 */
        Enemy &m_enemy;                    /**< 敌人 */
        Rng &m_rng;                        /**< 随机源 */
        Config m_config;                   /**< 战斗参数 */
        FlowState m_flow;                  /**< 心流状态 */
        std::unordered_map<std::string, int> m_cooldowns; /**< 术式剩余冷却 */
        bool m_over = false;               /**< 战斗是否结束 */
        bool m_player_won = false;         /**< 玩家是否获胜 */
        bool m_player_died = false;        /**< 玩家是否死亡 */
    };
}

#endif  // INCLUDE_TREELANG_COMBAT_BATTLE_HPP