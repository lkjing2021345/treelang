/**
 * @file flow_state.hpp
 * @brief 心流（连击）状态机与术式熔断规则。
 */

#ifndef INCLUDE_TREELANG_COMBAT_FLOW_STATE_HPP
#define INCLUDE_TREELANG_COMBAT_FLOW_STATE_HPP

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "core/element.hpp"
#include "model/spell.hpp"

namespace treelang
{
    /** 心流伤害阶梯系数：第 n 回合倍率 = Π(1 + kFlowFactor × i)，i = 1..n */
    inline constexpr double kFlowFactor = 0.2;

    /**
     * @class FlowState
     * @brief 心流状态机。
     *
     * 心流是黑闪触发的连击机制：
     * - 进入：任意术式触发黑闪后的下一回合，玩家自动获得心流；
     * - 延续：本回合术式的传导属性与上一回合钥匙存在任意交集；
     * - 再暴击：心流中再次触发黑闪，伤害层数清零但状态不中断；
     * - 熔断：传导属性不匹配导致心流中断，惩罚所有参与链路的术式。
     *
     * 传导属性 = 术式元素集合中除激活属性之外的元素（去重后的集合）。
     * 钥匙 = 上一回合维持心流的术式的传导属性。
     *
     * 典型用法（每个回合）：
     * @code
     * battle_flow.begin_round();                 // 回合开始：按需进入心流
     * auto report = resolve_spell_damage(...);   // 结算伤害（用当前倍率）
     * battle_flow.on_cast({&spell, report.activated});  // 推进状态
     * @endcode
     */
    class FlowState
    {
    public:
        /** @brief 心流状态转变类型 */
        enum class Kind
        {
            None,           /**< 无变化 */
            EnterScheduled, /**< 触发黑闪，下一回合进入心流 */
            Entered,        /**< 进入心流（首回合伤害 ×1.2） */
            Continued,      /**< 成功延续 */
            ResetByCrit,    /**< 心流中再暴击，层数清零但状态延续 */
            Broken,         /**< 传导不匹配，心流中断并触发熔断 */
        };

        /** @brief 本回合术式的施放信息 */
        struct Cast
        {
            const Spell *spell = nullptr;     /**< 本回合施放的术式 */
            std::optional<Element> activated; /**< 激活属性（触发黑闪的元素） */
        };

        /** @brief 单回合转变结果 */
        struct TurnResult
        {
            Kind kind = Kind::None;                 /**< 转变类型 */
            std::vector<std::string> meltdown_targets; /**< 熔断惩罚的术式 id */
            int extra_cooldown = 0;                 /**< 熔断附加冷却回合数 */
        };

        /** @brief 是否处于心流状态。 */
        bool is_active() const { return m_active; }

        /**
         * @brief 当前连击回合数 n。
         * @return 第 n 回合，伤害倍率为 Π(1 + kFlowFactor × i)，i = 1..n。
         */
        int combo_turns() const { return m_combo_turns; }

        /**
         * @brief 当前回合应应用的心流伤害倍率。
         * @return 未处于心流时返回 1.0。
         */
        double damage_multiplier() const
        {
            if (!m_active) return 1.0;
            double m = 1.0;
            for (int i = 1; i <= m_combo_turns; ++i) m *= (1.0 + kFlowFactor * i);
            return m;
        }

        /** @brief 当前钥匙（上一回合维持心流的传导属性）。 */
        const std::vector<Element> &keys() const { return m_keys; }

        /** @brief 参与本轮心流链路的术式 id（含触发首轮黑闪者）。 */
        const std::vector<std::string> &participants() const { return m_participants; }

        /**
         * @brief 回合开始调用：若上一回合触发了黑闪，本回合自动进入心流。
         * @return 本回合是否进入心流。
         */
        bool begin_round()
        {
            m_just_entered = false;
            if (m_pending_enter)
            {
                m_pending_enter = false;
                m_active = true;
                m_combo_turns = 1;
                m_just_entered = true;
                return true;
            }
            return false;
        }

        /**
         * @brief 本回合术式施放后调用，推进心流状态。
         * @param cast 施放信息（含术式与激活属性）。
         * @return 状态转变结果；熔断时附带惩罚信息。
         */
        TurnResult on_cast(const Cast &cast)
        {
            bool entered = m_just_entered;
            m_just_entered = false;

            // 未进入心流
            if (!m_active)
            {
                if (cast.activated.has_value())
                {
                    m_pending_enter = true;
                    m_keys = conduction(*cast.spell, *cast.activated);
                    m_participants = {cast.spell->id};
                    m_combo_turns = 0;
                    return TurnResult{Kind::EnterScheduled, {}, 0};
                }
                return TurnResult{Kind::None, {}, 0};
            }

            // 处于心流：延续判定
            std::vector<Element> cond = conduction(*cast.spell, cast.activated);
            if (!intersect(cond, m_keys))
            {
                TurnResult result;
                result.kind = Kind::Broken;
                result.meltdown_targets = m_participants;
                result.extra_cooldown =
                    m_participants.empty() ? 0 : static_cast<int>(m_participants.size()) - 1;
                reset();
                return result;
            }

            // 延续成功：更新钥匙与参与链路
            m_keys = cond;
            m_participants.push_back(cast.spell->id);
            if (cast.activated.has_value())
            {
                m_combo_turns = 1;  // 再暴击：伤害层数清零，下一回合从 ×1.2 重新开始
                return TurnResult{Kind::ResetByCrit, {}, 0};
            }
            m_combo_turns++;
            return TurnResult{entered ? Kind::Entered : Kind::Continued, {}, 0};
        }

    private:
        /** @brief 传导属性 = 术式元素集合去重后，除激活属性之外的元素。 */
        static std::vector<Element> conduction(const Spell &spell, std::optional<Element> activated)
        {
            std::vector<Element> cond;
            for (const ElementAttr &attr : spell.elements)
            {
                if (activated.has_value() && attr.element == *activated) continue;
                if (std::find(cond.begin(), cond.end(), attr.element) == cond.end())
                    cond.push_back(attr.element);
            }
            return cond;
        }

        /** @brief 两个元素集合是否存在交集。 */
        static bool intersect(const std::vector<Element> &a, const std::vector<Element> &b)
        {
            for (Element e : a)
                if (std::find(b.begin(), b.end(), e) != b.end()) return true;
            return false;
        }

        void reset()
        {
            m_active = false;
            m_pending_enter = false;
            m_combo_turns = 0;
            m_keys.clear();
            m_participants.clear();
            m_just_entered = false;
        }

        bool m_active = false;         /**< 是否处于心流 */
        bool m_pending_enter = false;  /**< 已触发黑闪，下回合进入 */
        bool m_just_entered = false;   /**< 本回合刚进入（供 on_cast 判定） */
        int m_combo_turns = 0;         /**< 连击回合数 */
        std::vector<Element> m_keys;   /**< 当前钥匙 */
        std::vector<std::string> m_participants; /**< 参与链路术式 */
    };
}

#endif  // INCLUDE_TREELANG_COMBAT_FLOW_STATE_HPP