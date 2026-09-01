/**
 * @file element.hpp
 * @brief 元素属性定义与元素工具函数。
 */

#ifndef INCLUDE_TREELANG_CORE_ELEMENT_HPP
#define INCLUDE_TREELANG_CORE_ELEMENT_HPP

#include <cstdint>
#include <optional>
#include <string_view>

namespace treelang
{
    /**
     * @brief 元素属性。
     *
     * 元素之间不存在克制关系表；是否触发【黑闪】仅取决于敌人的弱点
     * 元素集合是否包含该元素。
     */
    enum class Element : std::uint8_t
    {
        Fire,   /**< 火 */
        Water,  /**< 水 */
        Metal,  /**< 金 */
    };

    /**
     * @brief 获取元素在存档/配置中的稳定标识。
     *
     * 使用 ASCII 字符串，便于 JSON 序列化。
     *
     * @param e 元素。
     * @return 元素对应的 ASCII 标识，如 "fire"。
     */
    constexpr std::string_view element_id(Element e)
    {
        switch (e)
        {
            case Element::Fire: return "fire";
            case Element::Water: return "water";
            case Element::Metal: return "metal";
        }
        return {};
    }

    /**
     * @brief 从稳定标识解析元素。
     * @param id ASCII 标识，如 "water"。
     * @return 解析成功返回对应元素；未知标识返回 nullopt。
     */
    constexpr std::optional<Element> element_from_id(std::string_view id)
    {
        if (id == "fire") return Element::Fire;
        if (id == "water") return Element::Water;
        if (id == "metal") return Element::Metal;
        return std::nullopt;
    }

    /**
     * @brief 获取元素的显示名（中文）。
     * @param e 元素。
     * @return 元素的显示名，如 "火"。
     */
    constexpr std::string_view element_name(Element e)
    {
        switch (e)
        {
            case Element::Fire: return "火";
            case Element::Water: return "水";
            case Element::Metal: return "金";
        }
        return {};
    }
}

#endif  // INCLUDE_TREELANG_CORE_ELEMENT_HPP