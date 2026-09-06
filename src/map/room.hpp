#ifndef INCLUDE_TREELANG_MAP_ROOM_HPP
#define INCLUDE_TREELANG_MAP_ROOM_HPP

#include <bit>

#include "core/marco.hpp"
#include "core/types.hpp"

namespace treelang
{
    /**
     * @class Room
     * @brief 单个房间：类型、坐标与四向连通掩码。
     */
    class Room
    {
    private:
        DEFINE_DEFAULT_VALUE_ATTRIBUTE(RoomType, type, RoomType::Empty)
        DEFINE_DEFAULT_VALUE_ATTRIBUTE(Point, pos, make_point(0, 0))
        DEFINE_DEFAULT_VALUE_ATTRIBUTE(DirectionCollection, conn, 0)
        DEFINE_DEFAULT_VALUE_ATTRIBUTE(bool, visited, false)
        DEFINE_DEFAULT_VALUE_ATTRIBUTE(bool, cleared, false)

    public:
        DEFAULT_CONSTRUCTOR(Room)
        Room(RoomType t, Point p) : type(t), pos(p) {}

    public:
        /** @brief 是否已在指定方向集合上全部连通。 */
        bool connected(DirectionCollection dir) const { return (conn & dir) == dir; }
        bool connected(Direction dir) const { return connected(static_cast<DirectionCollection>(dir)); }

        /** @brief 在指定方向上建立连通（单向）。 */
        void connect(Direction dir) { conn |= static_cast<DirectionCollection>(dir); }

        /** @brief 连通边数（0~4）。 */
        int degree() const { return std::popcount(conn); }

        /** @brief 是否为叶子房间（恰好一条连通边）。 */
        bool is_leaf() const { return degree() == 1; }
    };
}

#endif  // INCLUDE_TREELANG_MAP_ROOM_HPP
