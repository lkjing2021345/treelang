#ifndef INCLUDE_TREELANG_ENTITY_BASE_HPP
#define INCLUDE_TREELANG_ENTITY_BASE_HPP

#include "core/marco.hpp"
#include "entity/status.hpp"

namespace treelang
{
    namespace entity
    {
        class Entity
        {
        private:
            DEFINE_ATTRIBUTE(StatusCollection, status)

        public:
            DEFAULT_CONSTRUCTOR(Entity)
            Entity(const StatusCollection &stus) : status(stus) {}
            Entity(StatusCollection &&stus) noexcept : status(stus) {}
        };
    }
}

#endif  // INCLUDE_TREELANG_ENTITY_BASE_HPP