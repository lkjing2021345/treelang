#ifndef INCLUDE_TREELANG_CORE_EVENT_HPP
#define INCLUDE_TREELANG_CORE_EVENT_HPP

#include <cstdint>
#include <string_view>

#include "core/marco.hpp"

namespace treelang
{
    class Event
    {
        DEFINE_ATTRIBUTE(uint64_t, sequence);
        void set_sequence(uint64_t seq) noexcept { sequence = seq; }

    public:
        virtual ~Event() = default;
        virtual std::string_view type_tag() const noexcept { return "Event"; }
    };
}

#endif  // INCLUDE_TREELANG_CORE_EVENT_HPP