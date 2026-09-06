/**
 * @file point.hpp
 * @brief 二维整型坐标类型。
 */

#ifndef INCLUDE_TREELANG_CORE_POINT_HPP
#define INCLUDE_TREELANG_CORE_POINT_HPP

namespace treelang
{
    /**
     * @brief 二维整型坐标（row, col），兼作位移向量。
     */
    struct Point
    {
        int row = 0;
        int col = 0;

        Point operator+(Point o) const { return Point{row + o.row, col + o.col}; }
        Point operator-(Point o) const { return Point{row - o.row, col - o.col}; }
        Point &operator+=(Point o)
        {
            row += o.row;
            col += o.col;
            return *this;
        }
        Point &operator-=(Point o)
        {
            row -= o.row;
            col -= o.col;
            return *this;
        }
        bool operator==(Point o) const { return row == o.row && col == o.col; }
        bool operator!=(Point o) const { return !(*this == o); }
        bool operator<(Point o) const { return row != o.row ? row < o.row : col < o.col; }
    };

    constexpr Point make_point(int row, int col) { return Point{row, col}; }
}

#endif  // INCLUDE_TREELANG_CORE_POINT_HPP
