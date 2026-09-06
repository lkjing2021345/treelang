/**
 * @file types.hpp
 * @brief 公共常量与基础领域类型。
 */

#ifndef INCLUDE_TREELANG_CORE_TYPES_HPP
#define INCLUDE_TREELANG_CORE_TYPES_HPP

#include <cstddef>
#include <cstdint>

#include "direction.hpp"
#include "element.hpp"
#include "point.hpp"

namespace treelang
{
    /** 地图规格：5×5 网格 */
    inline constexpr std::size_t k_map_size = 5;

    /** 中央初始房的行下标 */
    inline constexpr std::size_t k_center_row = k_map_size / 2;

    /** 中央初始房的列下标 */
    inline constexpr std::size_t k_center_col = k_map_size / 2;

    /** 理智值范围下界 */
    inline constexpr int k_san_min = 0;

    /** 理智值范围上界 */
    inline constexpr int k_san_max = 100;

    /** S.P.E.C.I.A.L. 属性点上限 */
    inline constexpr int k_attribute_cap = 20;

    /**
     * @brief 术式属性条目：元素 + 属性等级。
     */
    struct ElementAttr
    {
        Element element = Element::Fire; /**< 元素 */
        int level = 0;                   /**< 属性等级 */
    };

    /**
     * @brief 房间类型。
     */
    enum class RoomType : std::uint8_t
    {
        Start,    /**< 初始房（中央，安全补给点） */
        Reward,   /**< 奖励房 */
        Function, /**< 功能房（术式融合） */
        Elite,    /**< 精英怪房 */
        Story,    /**< 剧情房 */
        Enemy,    /**< 普通敌人房 */
        Exit,     /**< 出口/楼梯房（下一层入口） */
        Empty,    /**< 空房（无特殊内容） */
    };
}

#endif  // INCLUDE_TREELANG_CORE_TYPES_HPP