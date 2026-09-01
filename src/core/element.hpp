#ifndef INCLUDE_TREELANG_CORE_ELEMENT_HPP
#define INCLUDE_TREELANG_CORE_ELEMENT_HPP

#include <cstdint>
#include <optional>
#include <string_view>

namespace treelang
{
    enum class Element : std::uint8_t
    {
        Fire,
        Water,
        Metal,
        Wood,
        Dirt
    };

    constexpr std::string_view element_id(Element e)
    {
        switch (e)
        {
        case Element::Fire:
            return "fire";
        case Element::Water:
            return "water";
        case Element::Metal:
            return "metal";
        case Element::Wood:
            return "wood";
        case Element::Dirt:
            return "dirt";
        }
        return {};
    }

    constexpr std::optional<Element> element_from_id(std::string_view id)
    {
        if (id == "fire")
            return Element::Fire;
        if (id == "water")
            return Element::Water;
        if (id == "metal")
            return Element::Metal;
        if (id == "wood")
            return Element::Wood;
        if (id == "dirt")
            return Element::Dirt;
        return std::nullopt;
    }

    constexpr std::string_view element_name(Element e)
    {
        switch (e)
        {
        case Element::Fire:
            return "火";
        case Element::Water:
            return "水";
        case Element::Metal:
            return "金";
        case Element::Wood:
            return "木";
        case Element::Dirt:
            return "土";
        }
        return {};
    }
}

#endif  // INCLUDE_TREELANG_CORE_ELEMENT_HPP