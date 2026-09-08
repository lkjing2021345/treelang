#ifndef INCLUDE_TREELANG_ENTITY_STATUS_HPP
#define INCLUDE_TREELANG_ENTITY_STATUS_HPP

#include <algorithm>
#include <functional>
#include <string_view>

#include "core/marco.hpp"

namespace treelang
{
    namespace entity
    {
        class StatusCollectionBuilder;

        /**
         * @class SingleStatus
         * @brief 单个属性值（cur/tot）。所有修改路径收敛到 set_cur/set_tot，
         * 值真正变化时触发 on_change / on_max_change 回调（无变化不触发）。
         */
        class SingleStatus
        {
        private:
            int cur = 0;
            int tot = 0;
            std::function<void(int, int, int)> m_on_change;      // (old_cur, cur, tot)
            std::function<void(int, int, int)> m_on_max_change;  // (old_tot, tot, cur)

        public:
            DEFAULT_CONSTRUCTOR(SingleStatus)
            SingleStatus(int val) : cur(val), tot(val) {}

        public:
            int get_cur() const noexcept { return cur; }
            bool set_cur(int val)
            {
                if (val < 0 || val > tot)
                    return false;
                if (val == cur)
                    return true;
                const int old = cur;
                cur = val;
                if (m_on_change)
                    m_on_change(old, cur, tot);
                return true;
            }

            int get_tot() const noexcept { return tot; }
            bool set_tot(int val)
            {
                if (val < 0)
                    return false;
                if (val == tot)
                    return true;
                const int old_tot = tot;
                tot = val;
                const int old_cur = cur;
                if (cur > tot)
                    cur = tot;
                if (m_on_max_change)
                    m_on_max_change(old_tot, tot, cur);
                if (cur != old_cur && m_on_change)
                    m_on_change(old_cur, cur, tot);
                return true;
            }

            void add(int det) { set_cur(std::min(cur + det, tot)); }
            void sub(int det) { set_cur(std::max(cur - det, 0)); }

            void on_change(std::function<void(int, int, int)> cb)
            {
                m_on_change = std::move(cb);
            }
            void on_max_change(std::function<void(int, int, int)> cb)
            {
                m_on_max_change = std::move(cb);
            }
        };

        /**
         * @class StatusCollection
         * @brief 实体全属性集合。set_change_handler / set_max_change_handler 把
         * 五个属性一次性接到统一回调上，回调携带属性名。
         */
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

            using CurChangedHandler =
                std::function<void(std::string_view attr, int old_cur, int cur, int tot)>;
            using MaxChangedHandler = std::function<void(
                std::string_view attr, int old_tot, int new_tot, int cur)>;

            void set_change_handler(CurChangedHandler handler)
            {
                m_cur_handler = std::move(handler);
                wire_cur(hp, "hp");
                wire_cur(atk, "atk");
                wire_cur(def, "def");
                wire_cur(spd, "spd");
                wire_cur(san, "san");
            }

            void set_max_change_handler(MaxChangedHandler handler)
            {
                m_max_handler = std::move(handler);
                wire_max(hp, "hp");
                wire_max(atk, "atk");
                wire_max(def, "def");
                wire_max(spd, "spd");
                wire_max(san, "san");
            }

        private:
            CurChangedHandler m_cur_handler;
            MaxChangedHandler m_max_handler;

            void wire_cur(SingleStatus &st, std::string_view attr)
            {
                st.on_change(
                    [this, attr](int old_cur, int cur, int tot)
                    {
                        if (m_cur_handler)
                            m_cur_handler(attr, old_cur, cur, tot);
                    });
            }

            void wire_max(SingleStatus &st, std::string_view attr)
            {
                st.on_max_change(
                    [this, attr](int old_tot, int tot, int cur)
                    {
                        if (m_max_handler)
                            m_max_handler(attr, old_tot, tot, cur);
                    });
            }
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
