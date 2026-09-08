#ifndef INCLUDE_TREELANG_COMBAT_EVENT_HPP
#define INCLUDE_TREELANG_COMBAT_EVENT_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/event.hpp"
#include "core/marco.hpp"

namespace treelang
{
    /**
     * @brief 战斗会话中的角色。
     * @note 仅 combat 域内部使用（回合调度、目标合法性），不进入任何事件字段；
     * 事件一律只携带实体 id。
     */
    enum class Faction : std::uint8_t
    {
        Player,
        Enemy,
        Neutral,
    };

    enum class CombatOutcome : std::uint8_t
    {
        Victory,
        Defeat,
        Fled,
    };

    DEFINE_EVENT_START(Combat, Event)
    DEFINE_EVENT_END(Combat)

    DEFINE_EVENT_START(CombatStarted, CombatEvent)
    public:
        std::vector<std::string> enemy_ids;
    DEFINE_EVENT_END(CombatStarted)

    DEFINE_EVENT_START(CombatEnded, CombatEvent)
    public:
        CombatOutcome outcome = CombatOutcome::Victory;
    DEFINE_EVENT_END(CombatEnded)

    DEFINE_EVENT_START(RoundStarted, CombatEvent)
    public:
        int round = 0;
    DEFINE_EVENT_END(RoundStarted)

    DEFINE_EVENT_START(RoundEnded, CombatEvent)
    public:
        int round = 0;
    DEFINE_EVENT_END(RoundEnded)

    DEFINE_EVENT_START(TurnStarted, CombatEvent)
    public:
        std::string entity_id;
        int round = 0;
    DEFINE_EVENT_END(TurnStarted)

    DEFINE_EVENT_START(TurnEnded, CombatEvent)
    public:
        std::string entity_id;
        int round = 0;
    DEFINE_EVENT_END(TurnEnded)

    DEFINE_EVENT_START(SpellCast, CombatEvent)
    public:
        std::string caster;
        std::string spell_id;
        int mana_cost = 0;
    DEFINE_EVENT_END(SpellCast)

    DEFINE_EVENT_START(ItemUsed, CombatEvent)
    public:
        std::string user;
        std::string item_id;
    DEFINE_EVENT_END(ItemUsed)

    DEFINE_EVENT_START(FlowEntered, CombatEvent)
    public:
        int combo_turns = 1;
    DEFINE_EVENT_END(FlowEntered)

    DEFINE_EVENT_START(FlowContinued, CombatEvent)
    public:
        int combo_turns = 0;
    DEFINE_EVENT_END(FlowContinued)

    DEFINE_EVENT_START(FlowBroken, CombatEvent)
    public:
        int combo_turns = 0;
        int extra_cooldown = 0;
        std::vector<std::string> meltdown_targets;
    DEFINE_EVENT_END(FlowBroken)
}

#endif  // INCLUDE_TREELANG_COMBAT_EVENT_HPP
