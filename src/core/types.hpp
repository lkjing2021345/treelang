/**
 * @file types.hpp
 * @brief 公共常量与基础领域类型。
 */

#ifndef INCLUDE_TREELANG_CORE_TYPES_HPP
#define INCLUDE_TREELANG_CORE_TYPES_HPP

#include <cstddef>
#include <cstdint>

#include "element.hpp"

namespace treelang
{
    /** 地图规格：5×5 网格 */
    inline constexpr std::size_t kMapSize = 5;

    /** 理智值范围下界 */
    inline constexpr int kSanMin = 0;

    /** 理智值范围上界 */
    inline constexpr int kSanMax = 100;

    /** S.P.E.C.I.A.L. 属性点上限 */
    inline constexpr int kAttributeCap = 20;

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
    };
}

#endif  // INCLUDE_TREELANG_CORE_TYPES_HPP