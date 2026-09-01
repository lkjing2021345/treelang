#ifndef INCLUDE_TREELANG_ENTITY_STATUS_HPP
#define INCLUDE_TREELANG_ENTITY_STATUS_HPP

#include <algorithm>

#include "core/marco.hpp"

namespace treelang
{
    namespace entity
    {
        class StatusCollectionBuilder;

        class SingleStatus
        {
        private:
            int cur;
            int tot;

        public:
            DEFAULT_CONSTRUCTOR(SingleStatus)
            SingleStatus(int val) : cur(val), tot(val) {}

        public:
            int get_cur() const noexcept { return cur; }
            bool set_cur(int val) noexcept
            {
                if (val < 0 || val > tot)
                    return false;
                cur = val;
                return true;
            }

            int get_tot() const noexcept { return tot; }
            bool set_tot(int val) noexcept
            {
                if (val < 0)
                    return false;
                tot = val;
                return true;
            }

            void add(int det) noexcept { cur = std::min(cur + det, tot); }
            void sub(int det) noexcept { cur = std::max(cur - det, 0); }
        };

        class StatusCollection
        {
            DEFINE_ATTRIBUTE(SingleStatus, hp)
            DEFINE_ATTRIBUTE(SingleStatus, atk)
            DEFINE_ATTRIBUTE(SingleStatus, def)
            DEFINE_ATTRIBUTE(SingleStatus, spd)
            DEFINE_ATTRIBUTE(SingleStatus, san)

        public:
            DEFAULT_CONSTRUCTOR(StatusCollection)

        public:
            static StatusCollectionBuilder create();
        };

        CLASS_BUILDER_START(StatusCollection)
        CLASS_BUILDER_ATTRIBUTE(StatusCollection, int, hp)
        CLASS_BUILDER_ATTRIBUTE(StatusCollection, int, atk)
        CLASS_BUILDER_ATTRIBUTE(StatusCollection, int, def)
        CLASS_BUILDER_ATTRIBUTE(StatusCollection, int, spd)
        CLASS_BUILDER_ATTRIBUTE(StatusCollection, int, san)
        CLASS_BUILDER_END(StatusCollection)

    }

}

#endif  // INCLUDE_TREELANG_ENTITY_STATUS_HPP