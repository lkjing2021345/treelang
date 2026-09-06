/**
 * @file direction.hpp
 * @brief 四向方向类型与 Point 几何工具。
 */

#ifndef INCLUDE_TREELANG_CORE_DIRECTION_HPP
#define INCLUDE_TREELANG_CORE_DIRECTION_HPP

#include <array>
#include <cstdint>

#include "point.hpp"

namespace treelang
{
    using DirectionCollection = uint8_t;

    enum class Direction : DirectionCollection
    {
        Left = 1,
        Up = 2,
        Right = 4,
        Down = 8,
    };

    /** 全部四个正交方向。 */
    inline constexpr std::array<Direction, 4> k_all_directions{
        Direction::Left, Direction::Up, Direction::Right, Direction::Down
    };

    /** 相反方向。 */
    inline constexpr Direction opposite(Direction d)
    {
        switch (d)
        {
        case Direction::Left: return Direction::Right;
        case Direction::Right: return Direction::Left;
        case Direction::Up: return Direction::Down;
        default: return Direction::Up;
        }
    }

    /** 指定方向对应的 (row, col) 位移。 */
    inline constexpr Point offset_for(Direction d)
    {
        switch (d)
        {
        case Direction::Left: return make_point(0, -1);
        case Direction::Up: return make_point(-1, 0);
        case Direction::Right: return make_point(0, 1);
        default: return make_point(1, 0);
        }
    }

    /** 将点 p 沿方向 d 平移一格。 */
    inline constexpr Point move_point(Point p, Direction d)
    {
        return p + offset_for(d);
    }

    /** 从 a 指向正交相邻的 b 的方向。 */
    inline constexpr Direction direction_between(Point a, Point b)
    {
        Point d = b - a;
        if (d.row < 0) return Direction::Up;
        if (d.row > 0) return Direction::Down;
        if (d.col < 0) return Direction::Left;
        return Direction::Right;
    }
}

#endif  // INCLUDE_TREELANG_CORE_DIRECTION_HPP
