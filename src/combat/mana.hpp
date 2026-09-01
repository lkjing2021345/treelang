/**
 * @file mana.hpp
 * @brief 咒力消耗与恢复规则。
 */

#ifndef INCLUDE_TREELANG_COMBAT_MANA_HPP
#define INCLUDE_TREELANG_COMBAT_MANA_HPP

#include <cmath>

#include "model/spell.hpp"
#include "combat/flow_state.hpp"

namespace treelang
{
    /**
     * @brief 咒力规则可调参数。
     *
     * README 未给出具体数值，此处提供占位默认值，后续可集中调整。
     */
    struct ManaConfig
    {
        int base_cost = 5;            /**< 单属性条目的基础消耗 */
        double element_exponent = 2.0; /**< 属性条数指数：消耗 ∝ 条数^指数 */
        int flow_base = 2;            /**< 心流状态下的额外消耗基数 */
        int flow_per_turn = 1;        /**< 心流每层（连击回合）的额外消耗 */
        int regen_per_turn = 2;       /**< 每回合咒力恢复量 */
    };

    /**
     * @brief 计算施放术式的咒力消耗。
     *
     * 属性条数越多消耗呈指数增长（base × 条数^指数）；处于心流时，
     * 额外按连击回合数呈一次函数增长（flow_base + per_turn × 回合数）。
     *
     * @param spell 施放的术式。
     * @param flow 当前心流状态。
     * @param config 咒力参数。
     * @return 咒力消耗量。
     */
    inline int spell_mana_cost(const Spell &spell, const FlowState &flow,
        const ManaConfig &config = {})
    {
        double count = static_cast<double>(spell.elements.size());
        double complex_cost = config.base_cost * std::pow(count, config.element_exponent);
        double flow_cost = 0.0;
        if (flow.is_active())
            flow_cost =
                static_cast<double>(config.flow_base + config.flow_per_turn * flow.combo_turns());
        return static_cast<int>(std::round(complex_cost + flow_cost));
    }

    /**
     * @brief 每回合咒力恢复量（战斗与非战斗回合中缓慢恢复）。
     * @param config 咒力参数。
     * @return 恢复量。
     */
    inline int mana_regen_per_turn(const ManaConfig &config = {})
    {
        return config.regen_per_turn;
    }
}

#endif  // INCLUDE_TREELANG_COMBAT_MANA_HPP