/**
 * @file spell.hpp
 * @brief 术式（技能）数据模型。
 */

#ifndef INCLUDE_TREELANG_MODEL_SPELL_HPP
#define INCLUDE_TREELANG_MODEL_SPELL_HPP

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/types.hpp"

namespace treelang
{
    /**
     * @brief 术式。
     *
     * 游戏内的技能统称【术式】，由以下参数定义：
     * - 冷却回合：使用后需要等待的回合数；
     * - 元素属性列表：1 至多条元素属性，每条携带独立的属性等级；
     * - 基础伤害倍率：该术式造成的伤害系数。
     *
     * 本模型仅保存术式定义，不包含战斗中的运行时状态（如当前剩余冷却）。
     */
    struct Spell
    {
        std::string id;                    /**< 稳定标识，用于存档引用与熔断惩罚定位 */
        std::string name;                  /**< 显示名 */
        std::vector<ElementAttr> elements; /**< 元素属性列表（1 至多条） */
        int cooldown = 1;                  /**< 冷却回合数 */
        int base_multiplier = 100;         /**< 基础伤害倍率（百分数，100 = ×1.0） */
    };

    /**
     * @brief 术式融合。
     *
     * 将融合双方的属性列表合并累加：若存在同名元素，属性等级直接相加。
     * 例：A（火+1）+ B（火+2, 水+1）= 新术式（火+3, 水+1）。
     *
     * 融合所消耗的金币等费用规则不属于本函数职责，由游戏流程处理；
     * 返回术式的 id/name 为空、冷却与基础倍率取默认值，由调用方按需设置。
     *
     * @param a 融合方 A。
     * @param b 融合方 B。
     * @return 融合后的新术式。
     */
    inline Spell fuse_spells(const Spell &a, const Spell &b)
    {
        Spell result;
        result.elements = a.elements;
        for (const ElementAttr &attr : b.elements)
        {
            auto it = std::find_if(
                result.elements.begin(), result.elements.end(),
                [&](const ElementAttr &x) { return x.element == attr.element; });
            if (it != result.elements.end())
                it->level += attr.level;
            else
                result.elements.push_back(attr);
        }
        return result;
    }

    /**
     * @brief 玩家术式列表。
     *
     * 管理玩家持有的术式集合。战斗运行时（心流链路、熔断惩罚）经由
     * 术式的稳定标识在列表中定位目标术式。
     */
    class SpellBook
    {
    public:
        /**
         * @brief 添加一本术式。
         * @param spell 要添加的术式。
         */
        void add(Spell spell) { m_spells.push_back(std::move(spell)); }

        /**
         * @brief 按稳定标识移除术式。
         * @param id 术式稳定标识。
         * @return 存在并被移除返回 true；不存在返回 false。
         */
        bool remove(std::string_view id)
        {
            auto it = std::find_if(
                m_spells.begin(), m_spells.end(),
                [&](const Spell &s) { return s.id == id; });
            if (it == m_spells.end())
                return false;
            m_spells.erase(it);
            return true;
        }

        /**
         * @brief 按稳定标识查找术式（常量版本）。
         * @param id 术式稳定标识。
         * @return 指向术式的常量指针；不存在返回 nullptr。
         */
        const Spell *find(std::string_view id) const
        {
            auto it = std::find_if(
                m_spells.begin(), m_spells.end(),
                [&](const Spell &s) { return s.id == id; });
            return it == m_spells.end() ? nullptr : &*it;
        }

        /**
         * @brief 按稳定标识查找术式（可变版本）。
         * @param id 术式稳定标识。
         * @return 指向术式的可变指针；不存在返回 nullptr。
         */
        Spell *find(std::string_view id)
        {
            auto it = std::find_if(
                m_spells.begin(), m_spells.end(),
                [&](const Spell &s) { return s.id == id; });
            return it == m_spells.end() ? nullptr : &*it;
        }

        /** @brief 返回持有的术式数量。 */
        std::size_t size() const { return m_spells.size(); }

        /** @brief 返回术式集合起始迭代器。 */
        auto begin() { return m_spells.begin(); }
        /** @brief 返回术式集合终止迭代器。 */
        auto end() { return m_spells.end(); }
        /** @brief 返回术式集合起始迭代器（常量版本）。 */
        auto begin() const { return m_spells.begin(); }
        /** @brief 返回术式集合终止迭代器（常量版本）。 */
        auto end() const { return m_spells.end(); }

    private:
        std::vector<Spell> m_spells; /**< 术式集合 */
    };
}

#endif  // INCLUDE_TREELANG_MODEL_SPELL_HPP