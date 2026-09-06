#ifndef INCLUDE_TREELANG_MAP_EVENT_HPP
#define INCLUDE_TREELANG_MAP_EVENT_HPP

#include <string_view>

#include "core/event.hpp"
#include "core/types.hpp"

namespace treelang
{
    /**
     * @class RoomEnteredEvent
     * @brief 玩家进入房间时发布，内容层（商店/奖励/剧情等）自行订阅。
     */
    class RoomEnteredEvent : public Event
    {
    public:
        Point pos;
        RoomType type = RoomType::Empty;
        std::string_view type_tag() const noexcept override { return "RoomEnteredEvent"; }
    };

    /**
     * @class RoomClearedEvent
     * @brief 房间内容被处理完毕（战斗胜利/奖励领取等）时发布。
     */
    class RoomClearedEvent : public Event
    {
    public:
        Point pos;
        std::string_view type_tag() const noexcept override { return "RoomClearedEvent"; }
    };
}

#endif  // INCLUDE_TREELANG_MAP_EVENT_HPP
