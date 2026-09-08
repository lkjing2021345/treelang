#ifndef INCLUDE_TREELANG_ENTITY_BASE_HPP
#define INCLUDE_TREELANG_ENTITY_BASE_HPP

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/element.hpp"
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

            /**
             * @brief 承受伤害（战斗结算入口）：发布 EntityDamagedEvent（原因层），
             * 再按 hp_lost 扣血（状态层自动发布 Changed / Died）。
             * amount/shield_absorbed/hp_lost 为战斗层算好的结算分解，本层不做盾计算。
             */
            /** @return 实际血量损失（超杀时可能小于 hp_lost，已死时为 0）。 */
            int take_damage(
                const std::string &source,
                int amount,
                int shield_absorbed,
                int hp_lost,
                std::optional<Element> element,
                bool black_flash)
            {
                auto ev = std::make_shared<EntityDamagedEvent>();
                ev->source = source;
                ev->target = id;
                ev->amount = amount;
                ev->shield_absorbed = shield_absorbed;
                ev->hp_lost = hp_lost;
                ev->element = element;
                ev->black_flash = black_flash;
                EventBusInstance::instance().data().publish(ev);

                return status.get_hp().sub(hp_lost);
            }

            /**
             * @brief 恢复 hp：按上限钳制后发布 EntityHealedEvent，amount 记实际恢复量。
             * @note 先改状态后发事件（Changed 早于 Healed）：实际恢复量只有改动后才知道。
             * @return 实际恢复量（满血或 amount<=0 时为 0）。
             */
            int heal(int amount)
            {
                if (amount <= 0)
                    return 0;
                const int real = status.get_hp().add(amount);
                if (real <= 0)
                    return 0;
                auto ev = std::make_shared<EntityHealedEvent>();
                ev->target = id;
                ev->amount = real;
                EventBusInstance::instance().data().publish(ev);
                return real;
            }

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
