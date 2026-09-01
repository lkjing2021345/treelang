/**
 * @file damage.hpp
 * @brief 伤害结算规则。
 */

#ifndef INCLUDE_TREELANG_COMBAT_DAMAGE_HPP
#define INCLUDE_TREELANG_COMBAT_DAMAGE_HPP

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "core/element.hpp"
#include "model/entity.hpp"
#include "model/spell.hpp"

namespace treelang
{
    /**
     * @brief 伤害结算可调参数。
     *
     * README 未给出具体数值，此处提供占位默认值，后续可集中调整。
     */
    struct DamageConfig
    {
        int base_attack = 10;              /**< 初始攻击基数 */
        int strength_factor = 2;           /**< 每点力量提供的攻击力 */
        double level_factor = 0.5;         /**< 属性等级系数：伤害 = 攻击 × (1 + 等级 × 系数) */
        int black_flash_multiplier = 120;  /**< 黑闪倍率（百分数） */
    };

    /**
     * @brief 由 S.P.E.C.I.A.L. 属性计算基础攻击力。
     *
     * 力量决定基础攻击力加成。
     *
     * @param special 属性。
     * @param config 伤害参数。
     * @return 基础攻击力。
     */
    inline int base_attack(const SpecialAttributes &special, const DamageConfig &config = {})
    {
        return config.base_attack + special.strength * config.strength_factor;
    }

    /**
     * @brief 单个元素属性对敌人的结算结果。
     */
    struct ElementDamage
    {
        Element element = Element::Fire; /**< 元素 */
        int level = 0;                   /**< 属性等级 */
        bool black_flash = false;        /**< 是否触发黑闪（元素 ∈ 敌人弱点） */
        int damage = 0;                  /**< 该元素造成的伤害 */
    };

    /**
     * @brief 一次术式命中的结算报告。
     */
    struct DamageReport
    {
        std::vector<ElementDamage> elements; /**< 各元素明细 */
        int total_damage = 0;                /**< 最终伤害（各元素取最大值） */
        bool black_flash = false;            /**< 本次攻击是否触发黑闪 */
        std::optional<Element> activated;    /**< 激活属性（触发黑闪的元素；未触发为 nullopt） */
    };

    /**
     * @brief 结算术式对敌人的一次伤害。
     *
     * 每个元素属性分别对敌人计算伤害，最终取最大值作为本次攻击伤害；
     * 属性等级与伤害呈一次函数关系（伤害 = 攻击 × (1 + 等级 × 系数)）。
     * 若某元素 ∈ 敌人弱点集合则触发黑闪，该元素伤害倍率提升至
     * black_flash_multiplier（默认 120%）。
     *
     * 激活属性取「触发黑闪且伤害最高」的元素；多个元素同时触发时取首个。
     *
     * @param spell 施放的术式。
     * @param attack 攻击方基础攻击力（见 base_attack）。
     * @param enemy 目标敌人。
     * @param config 伤害参数。
     * @return 结算报告。
     */
    inline DamageReport resolve_spell_damage(const Spell &spell, int attack,
        const Enemy &enemy, const DamageConfig &config = {})
    {
        DamageReport report;
        report.elements.reserve(spell.elements.size());
        for (const ElementAttr &attr : spell.elements)
        {
            ElementDamage elem;
            elem.element = attr.element;
            elem.level = attr.level;
            elem.black_flash = std::find(enemy.weaknesses.begin(), enemy.weaknesses.end(),
                attr.element) != enemy.weaknesses.end();

            double linear = 1.0 + attr.level * config.level_factor;
            int raw = static_cast<int>(std::round(attack * linear));
            int multiplier_pct =
                elem.black_flash ? config.black_flash_multiplier : spell.base_multiplier;
            elem.damage = static_cast<int>(std::round(raw * multiplier_pct / 100.0));
            report.elements.push_back(elem);
        }

        // 取各元素伤害最大值作为最终伤害
        int max_idx = -1;
        for (std::size_t i = 0; i < report.elements.size(); ++i)
        {
            if (max_idx < 0 || report.elements[i].damage > report.elements[max_idx].damage)
                max_idx = static_cast<int>(i);
        }
        if (max_idx >= 0) report.total_damage = report.elements[max_idx].damage;

        // 激活属性：触发黑闪且伤害最高的元素
        int act_idx = -1;
        for (std::size_t i = 0; i < report.elements.size(); ++i)
        {
            if (!report.elements[i].black_flash) continue;
            if (act_idx < 0 || report.elements[i].damage > report.elements[act_idx].damage)
                act_idx = static_cast<int>(i);
        }
        if (act_idx >= 0)
        {
            report.black_flash = true;
            report.activated = report.elements[act_idx].element;
        }
        return report;
    }

    /**
     * @brief 应用心流伤害倍率。
     * @param damage 基础伤害。
     * @param multiplier 心流倍率（见 FlowState::damage_multiplier）。
     * @return 缩放后的伤害（四舍五入）。
     */
    inline int scale_damage(int damage, double multiplier)
    {
        return static_cast<int>(std::round(damage * multiplier));
    }
}

#endif  // INCLUDE_TREELANG_COMBAT_DAMAGE_HPP