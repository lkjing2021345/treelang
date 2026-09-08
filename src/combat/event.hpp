#ifndef INCLUDE_TREELANG_COMBAT_EVENT_HPP
#define INCLUDE_TREELANG_COMBAT_EVENT_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/element.hpp"
#include "core/event.hpp"
#include "core/marco.hpp"

namespace treelang
{
    enum class Faction : std::uint8_t
    {
        Player,
        Enemy,
        Neutral,
    };

    struct Combatant
    {
        Faction faction = Faction::Neutral;
        std::string id;
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
    Combatant combatant;
    int round = 0;
    DEFINE_EVENT_END(TurnStarted)

    DEFINE_EVENT_START(TurnEnded, CombatEvent)
public:
    Combatant combatant;
    int round = 0;
    DEFINE_EVENT_END(TurnEnded)

    DEFINE_EVENT_START(SpellCast, CombatEvent)
public:
    Combatant caster;
    std::string spell_id;
    int mana_cost = 0;
    DEFINE_EVENT_END(SpellCast)

    DEFINE_EVENT_START(ItemUsed, CombatEvent)
public:
    Combatant user;
    std::string item_id;
    DEFINE_EVENT_END(ItemUsed)

    DEFINE_EVENT_START(DamageDealt, CombatEvent)
public:
    Combatant source;
    Combatant target;
    int amount = 0;
    int shield_absorbed = 0;
    int hp_lost = 0;
    std::optional<Element> element;
    bool black_flash = false;
    DEFINE_EVENT_END(DamageDealt)

    DEFINE_EVENT_START(Healed, CombatEvent)
public:
    Combatant target;
    int amount = 0;
    DEFINE_EVENT_END(Healed)

    DEFINE_EVENT_START(ShieldGained, CombatEvent)
public:
    Combatant target;
    int amount = 0;
    DEFINE_EVENT_END(ShieldGained)

    DEFINE_EVENT_START(SanLost, CombatEvent)
public:
    Combatant target;
    int amount = 0;
    DEFINE_EVENT_END(SanLost)

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

    DEFINE_EVENT_START(EntityDied, CombatEvent)
public:
    Combatant combatant;
    DEFINE_EVENT_END(EntityDied)
}

#endif  // INCLUDE_TREELANG_COMBAT_EVENT_HPP
