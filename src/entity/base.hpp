#ifndef INCLUDE_TREELANG_ENTITY_BASE_HPP
#define INCLUDE_TREELANG_ENTITY_BASE_HPP

#include <string>
#include <utility>

#include "core/marco.hpp"
#include "entity/status.hpp"

namespace treelang
{
    namespace entity
    {
        class Entity
        {
        private:
            std::string id;
            DEFINE_ATTRIBUTE(StatusCollection, status)

        public:
            Entity(std::string eid, StatusCollection stus) : id(std::move(eid)), status(std::move(stus)) {}
            Entity(std::string eid, StatusCollection &&stus) noexcept : id(std::move(eid)), status(std::move(stus)) {}

            const std::string &get_id() const noexcept { return id; }
        };
    }
}

#endif  // INCLUDE_TREELANG_ENTITY_BASE_HPP
