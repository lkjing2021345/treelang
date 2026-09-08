#ifndef INCLUDE_TREELANG_ENTITY_EVENT_HPP
#define INCLUDE_TREELANG_ENTITY_EVENT_HPP

#include <optional>
#include <string>
#include <string_view>

#include "core/element.hpp"
#include "core/event.hpp"
#include "core/marco.hpp"

namespace treelang
{
    /**
     * @class EntityEvent
     * @brief 实体域事件基类：描述实体自身状态/身份的变化，与具体战斗流程解耦。
     * @note 事件只携带实体 id；敌我关系等会话角色由 combat 域按 id 自行维护。
     */
    DEFINE_EVENT_START(Entity, Event)
    DEFINE_EVENT_END(Entity)

    /**
     * @class EntityStatusChangedEvent
     * @brief 实体某属性的 cur 值发生变化（hp/atk/def），由状态监听器自动发布。
     */
    DEFINE_EVENT_START(EntityStatusChanged, EntityEvent)
    public:
        std::string entity_id;
        std::string_view attribute;
        int old_cur = 0;
        int new_cur = 0;
        int tot = 0;
    DEFINE_EVENT_END(EntityStatusChanged)

    /**
     * @class EntityStatusMaxChangedEvent
     * @brief 实体某属性的 tot（上限）发生变化，如升级成长。
     */
    DEFINE_EVENT_START(EntityStatusMaxChanged, EntityEvent)
    public:
        std::string entity_id;
        std::string_view attribute;
        int old_tot = 0;
        int new_tot = 0;
        int cur = 0;
    DEFINE_EVENT_END(EntityStatusMaxChanged)

    /**
     * @class EntityDiedEvent
     * @brief 实体 hp 降为 0，由状态监听器自动发布。
     */
    DEFINE_EVENT_START(EntityDied, EntityEvent)
    public:
        std::string entity_id;
    DEFINE_EVENT_END(EntityDied)

    /**
     * @class EntityDamagedEvent
     * @brief 实体受到伤害（原因层：来源、总量、护盾吸收、血量损失）。
     */
    DEFINE_EVENT_START(EntityDamaged, EntityEvent)
    public:
        std::string source;
        std::string target;
        int amount = 0;
        int shield_absorbed = 0;
        int hp_lost = 0;
        std::optional<Element> element;
        bool black_flash = false;
    DEFINE_EVENT_END(EntityDamaged)

    /**
     * @class EntityHealedEvent
     * @brief 实体恢复 hp（原因层）。
     */
    DEFINE_EVENT_START(EntityHealed, EntityEvent)
    public:
        std::string target;
        int amount = 0;
    DEFINE_EVENT_END(EntityHealed)

    /**
     * @class EntityShieldGainedEvent
     * @brief 实体获得护盾（原因层）。
     */
    DEFINE_EVENT_START(EntityShieldGained, EntityEvent)
    public:
        std::string target;
        int amount = 0;
    DEFINE_EVENT_END(EntityShieldGained)

}

#endif  // INCLUDE_TREELANG_ENTITY_EVENT_HPP
