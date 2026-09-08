#ifndef INCLUDE_TREELANG_ENTITY_BASE_HPP
#define INCLUDE_TREELANG_ENTITY_BASE_HPP

#include <memory>
#include <string>
#include <utility>

#include "core/instance.hpp"
#include "core/marco.hpp"
#include "entity/event.hpp"
#include "entity/status.hpp"

namespace treelang
{
    namespace entity
    {
        /**
         * @class Entity
         * @brief 构造时自动绑定状态监听：属性 cur/tot 变化经 EventBusInstance
         * 发布 EntityStatusChangedEvent / EntityStatusMaxChangedEvent；
         * hp 降为 0 时追加发布 EntityDiedEvent。
         */
        class Entity
        {
        private:
            std::string id;
            DEFINE_ATTRIBUTE(StatusCollection, status)

        public:
            Entity(std::string eid, StatusCollection stus) :
                id(std::move(eid)), status(std::move(stus))
            {
                bind_status_events();
            }

            const std::string &get_id() const noexcept { return id; }

        private:
            void bind_status_events()
            {
                const std::string entity_id = id;
                status.set_change_handler(
                    [entity_id](std::string_view attr, int old_cur, int cur, int tot)
                    {
                        auto ev = std::make_shared<EntityStatusChangedEvent>();
                        ev->entity_id = entity_id;
                        ev->attribute = attr;
                        ev->old_cur = old_cur;
                        ev->new_cur = cur;
                        ev->tot = tot;
                        EventBusInstance::instance().data().publish(ev);

                        if (attr == "hp" && old_cur > 0 && cur <= 0)
                        {
                            auto died = std::make_shared<EntityDiedEvent>();
                            died->entity_id = entity_id;
                            EventBusInstance::instance().data().publish(died);
                        }
                    });
                status.set_max_change_handler(
                    [entity_id](std::string_view attr, int old_tot, int new_tot, int cur)
                    {
                        auto ev = std::make_shared<EntityStatusMaxChangedEvent>();
                        ev->entity_id = entity_id;
                        ev->attribute = attr;
                        ev->old_tot = old_tot;
                        ev->new_tot = new_tot;
                        ev->cur = cur;
                        EventBusInstance::instance().data().publish(ev);
                    });
            }
        };
    }
}

#endif  // INCLUDE_TREELANG_ENTITY_BASE_HPP
