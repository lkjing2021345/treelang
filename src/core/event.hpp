#ifndef INCLUDE_TREELANG_CORE_EVENT_HPP
#define INCLUDE_TREELANG_CORE_EVENT_HPP

#include <cstdint>
#include <string>

#include "core/marco.hpp"

namespace treelang
{
    class Event
    {
        DEFINE_ATTRIBUTE(uint64_t, id)
        DEFINE_ATTRIBUTE(std::string, msg)
    };
}

#endif  // INCLUDE_TREELANG_CORE_EVENT_HPP