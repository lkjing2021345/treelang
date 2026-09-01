/**
 * @file item.hpp
 * @brief 道具数据模型。
 */

#ifndef INCLUDE_TREELANG_MODEL_ITEM_HPP
#define INCLUDE_TREELANG_MODEL_ITEM_HPP

#include <cstdint>
#include <string>

namespace treelang
{
    /**
     * @brief 道具类型。
     */
    enum class ItemKind : std::uint8_t
    {
        Consumable, /**< 消耗品：回血 / 回蓝等 */
        Tactical,   /**< 战术道具：赋予临时属性等 */
    };

    /**
     * @brief 道具。
     *
     * 道具系统尚在设计阶段（README 标注待补充）：消耗品拟提供回血/回蓝，
     * 战术道具拟赋予临时属性。具体数值效果待需求明确后补充字段。
     */
    struct Item
    {
        std::string name;                     /**< 名称 */
        ItemKind kind = ItemKind::Consumable; /**< 类型 */
        std::string description;              /**< 描述 */
    };
}

#endif  // INCLUDE_TREELANG_MODEL_ITEM_HPP