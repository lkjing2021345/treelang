#include <doctest/doctest.h>

#include "combat/battle.hpp"
#include "combat/damage.hpp"
#include "combat/flow_state.hpp"
#include "combat/mana.hpp"
#include "core/rng.hpp"
#include "model/entity.hpp"
#include "model/item.hpp"
#include "model/spell.hpp"

using treelang::base_attack;
using treelang::Battle;
using treelang::CombatError;
using treelang::DamageReport;
using treelang::Element;
using treelang::ElementAttr;
using treelang::Enemy;
using treelang::FlowState;
using treelang::Item;
using treelang::ItemKind;
using treelang::mana_regen_per_turn;
using treelang::Player;
using treelang::resolve_spell_damage;
using treelang::Rng;
using treelang::scale_damage;
using treelang::SpecialAttributes;
using treelang::Spell;
using treelang::spell_mana_cost;

TEST_CASE("combat: base_attack scales with strength")
{
    SpecialAttributes s;
    s.strength = 5;
    CHECK(base_attack(s) == 10 + 5 * 2);
}

TEST_CASE("combat: resolve damage takes max element and black flash")
{
    Spell spell;
    spell.id = "s1";
    spell.base_multiplier = 100;
    spell.elements = {ElementAttr{Element::Fire, 1}, ElementAttr{Element::Water, 2}};

    Enemy enemy;
    enemy.weaknesses = {Element::Water};  // 只有水是弱点

    DamageReport report = resolve_spell_damage(spell, 20, enemy);

    REQUIRE(report.elements.size() == 2);
    // 火：无黑闪，倍率 100%；伤害 = 20 × (1 + 1×0.5) × 100% = 30
    CHECK(report.elements[0].element == Element::Fire);
    CHECK(report.elements[0].black_flash == false);
    CHECK(report.elements[0].damage == 30);
    // 水：黑闪，倍率 120%；伤害 = 20 × (1 + 2×0.5) × 120% = 48
    CHECK(report.elements[1].element == Element::Water);
    CHECK(report.elements[1].black_flash == true);
    CHECK(report.elements[1].damage == 48);

    CHECK(report.total_damage == 48);  // 取最大值
    CHECK(report.black_flash == true);
    REQUIRE(report.activated.has_value());
    CHECK(report.activated == Element::Water);
}

TEST_CASE("combat: resolve no black flash gives no activated element")
{
    Spell spell;
    spell.id = "s1";
    spell.base_multiplier = 100;
    spell.elements = {ElementAttr{Element::Fire, 1}};

    Enemy enemy;  // 无弱点

    DamageReport report = resolve_spell_damage(spell, 20, enemy);
    CHECK(report.total_damage == 30);
    CHECK(report.black_flash == false);
    CHECK(!report.activated.has_value());
}

TEST_CASE("combat: scale_damage rounds flow multiplier")
{
    CHECK(scale_damage(100, 1.2) == 120);
    CHECK(scale_damage(33, 1.2) == 40);
    CHECK(scale_damage(100, 1.0) == 100);
}

TEST_CASE("combat: flow enter schedule then enter round")
{
    FlowState fs;
    Spell a;
    a.id = "a";
    a.elements = {ElementAttr{Element::Fire, 1}, ElementAttr{Element::Water, 1}};

    auto r = fs.on_cast({&a, Element::Fire});  // 激活火，传导水
    CHECK(r.kind == FlowState::Kind::EnterScheduled);
    CHECK(!fs.is_active());

    CHECK(fs.begin_round());  // 下回合进入
    CHECK(fs.is_active());
    CHECK(fs.combo_turns() == 1);
    CHECK(fs.damage_multiplier() == doctest::Approx(1.2));
    CHECK(fs.keys().size() == 1);
    CHECK(fs.keys()[0] == Element::Water);
    CHECK(fs.participants().size() == 1);
}

TEST_CASE("combat: flow continues and scales damage")
{
    FlowState fs;
    Spell a;
    a.id = "a";
    a.elements = {ElementAttr{Element::Fire, 1}, ElementAttr{Element::Water, 1}};
    fs.on_cast({&a, Element::Fire});
    fs.begin_round();

    Spell b;
    b.id = "b";
    b.elements = {ElementAttr{Element::Water, 1}, ElementAttr{Element::Metal, 1}};
    auto r = fs.on_cast(
        {&b, std::nullopt});  // 本回合为进入心流的首回合：传导 {水,金} ∩ 钥匙 {水}
    CHECK(r.kind == FlowState::Kind::Entered);
    CHECK(fs.combo_turns() == 2);
    CHECK(fs.damage_multiplier() == doctest::Approx(1.2 * 1.4));
    CHECK(fs.participants().size() == 2);
    CHECK(fs.keys()[0] == Element::Water);
    CHECK(fs.keys()[1] == Element::Metal);
}

TEST_CASE("combat: flow breaks with meltdown penalty")
{
    FlowState fs;
    Spell a;
    a.id = "a";
    a.elements = {ElementAttr{Element::Fire, 1}, ElementAttr{Element::Water, 1}};
    fs.on_cast({&a, Element::Fire});
    fs.begin_round();

    Spell b;
    b.id = "b";
    b.elements = {ElementAttr{Element::Water, 1}, ElementAttr{Element::Metal, 1}};
    fs.on_cast({&b, std::nullopt});
    CHECK(fs.combo_turns() == 2);

    Spell c;
    c.id = "c";
    c.elements = {ElementAttr{Element::Fire, 1}};  // 传导 {火} ∩ 钥匙 {水,金} = ∅
    auto r = fs.on_cast({&c, std::nullopt});
    CHECK(r.kind == FlowState::Kind::Broken);
    REQUIRE(r.meltdown_targets.size() == 2);
    CHECK(r.meltdown_targets[0] == "a");
    CHECK(r.meltdown_targets[1] == "b");
    CHECK(r.extra_cooldown == 1);  // 参与术式数 2 - 1
    CHECK(!fs.is_active());
    CHECK(fs.participants().empty());
}

TEST_CASE("combat: black flash inside flow resets layers but keeps chain")
{
    FlowState fs;
    Spell a;
    a.id = "a";
    a.elements = {ElementAttr{Element::Fire, 1}, ElementAttr{Element::Water, 1}};
    fs.on_cast({&a, Element::Fire});
    fs.begin_round();
    fs.on_cast({&a, std::nullopt});
    CHECK(fs.combo_turns() == 2);

    Spell d;
    d.id = "d";
    d.elements = {ElementAttr{Element::Water, 1}, ElementAttr{Element::Fire, 1}};
    auto r = fs.on_cast({&d, Element::Fire});  // 再暴击
    CHECK(r.kind == FlowState::Kind::ResetByCrit);
    CHECK(fs.is_active());         // 心流不中断
    CHECK(fs.combo_turns() == 1);  // 层数清零，下一回合从 ×1.2 重新开始
    CHECK(fs.damage_multiplier() == doctest::Approx(1.2));
    CHECK(fs.participants().size() == 3);  // 链路持续累积
}

TEST_CASE("combat: enter round can break immediately")
{
    FlowState fs;
    Spell a;
    a.id = "a";
    a.elements = {ElementAttr{Element::Fire, 1}, ElementAttr{Element::Water, 1}};
    fs.on_cast({&a, Element::Fire});
    fs.begin_round();

    Spell e;
    e.id = "e";
    e.elements = {ElementAttr{Element::Fire, 1}, ElementAttr{Element::Metal, 1}};
    auto r = fs.on_cast({&e, std::nullopt});  // 传导 {金} ∩ 钥匙 {水} = ∅
    CHECK(r.kind == FlowState::Kind::Broken);
    REQUIRE(r.meltdown_targets.size() == 1);
    CHECK(r.meltdown_targets[0] == "a");
    CHECK(r.extra_cooldown == 0);
}

TEST_CASE("combat: single-element spell cannot sustain flow")
{
    FlowState fs;
    Spell f;
    f.id = "f";
    f.elements = {ElementAttr{Element::Fire, 1}};
    fs.on_cast({&f, Element::Fire});  // 传导属性为空
    fs.begin_round();

    Spell g;
    g.id = "g";
    g.elements = {ElementAttr{Element::Water, 1}};
    auto r = fs.on_cast({&g, std::nullopt});
    CHECK(r.kind == FlowState::Kind::Broken);
}

TEST_CASE("combat: mana cost grows exponentially with elements and linearly in flow")
{
    Spell s;
    s.elements = {ElementAttr{Element::Fire, 1}};
    CHECK(spell_mana_cost(s, FlowState{}) == 5);  // 5 × 1^2

    s.elements = {ElementAttr{Element::Fire, 1}, ElementAttr{Element::Water, 1}};
    CHECK(spell_mana_cost(s, FlowState{}) == 20);  // 5 × 2^2

    s.elements = {
        ElementAttr{Element::Fire, 1}, ElementAttr{Element::Water, 1},
        ElementAttr{Element::Metal, 1}};
    CHECK(spell_mana_cost(s, FlowState{}) == 45);  // 5 × 3^2

    // 心流下按连击回合数线性增加
    FlowState fs;
    Spell a;
    a.elements = {ElementAttr{Element::Fire, 1}, ElementAttr{Element::Water, 1}};
    fs.on_cast({&a, Element::Fire});
    fs.begin_round();
    CHECK(fs.combo_turns() == 1);
    CHECK(spell_mana_cost(s, fs) == 45 + (2 + 1 * 1));

    CHECK(mana_regen_per_turn() == 2);
}

TEST_CASE("combat: battle kills enemy with black flash")
{
    Player p;
    p.special.strength = 5;
    p.status.mana = 100;
    p.status.hp = 100;
    p.status.san = 50;

    Spell s;
    s.id = "s1";
    s.name = "火球";
    s.elements = {ElementAttr{Element::Fire, 2}};
    s.base_multiplier = 100;
    s.cooldown = 2;
    p.spells.add(s);

    Enemy e;
    e.name = "哥布林";
    e.hp = 30;
    e.max_hp = 30;
    e.power = 5;
    e.san = 30;
    e.weaknesses = {Element::Fire};

    Rng rng{1};
    Battle::Config cfg;
    cfg.enemy_damage_variance = 0;
    Battle battle(p, e, rng, cfg);

    battle.begin_round();
    auto r = battle.player_cast(s);
    REQUIRE(r.is_ok());
    auto &result = r.unwrap();
    CHECK(result.turn_consumed);
    CHECK(result.damage_dealt == 48);  // 攻击 20 × (1+2×0.5) × 120% = 48
    CHECK(result.enemy_defeated);
    CHECK(battle.is_over());
    CHECK(battle.player_won());
    CHECK(p.status.mana == 97);  // 100 + 恢复2 - 消耗5
    CHECK(battle.cooldown_of("s1") == 2);
}

TEST_CASE("combat: battle rejects cooldown / unknown spell / insufficient mana")
{
    Player p;
    p.special.strength = 0;
    p.status.mana = 100;

    Spell s;
    s.id = "s1";
    s.elements = {ElementAttr{Element::Fire, 1}};
    s.cooldown = 2;
    p.spells.add(s);

    Enemy e;
    e.hp = 500;
    e.power = 0;

    Rng rng{1};
    Battle::Config cfg;
    cfg.enemy_damage_variance = 0;
    Battle battle(p, e, rng, cfg);

    Spell ghost;
    ghost.id = "ghost";
    CHECK(battle.player_cast(ghost).is_err());
    CHECK(battle.player_cast(ghost).unwrap_err() == CombatError::NoSuchSpell);

    battle.begin_round();
    CHECK(battle.player_cast(s).is_ok());

    // 冷却中
    auto r = battle.player_cast(s);
    CHECK(r.is_err());
    CHECK(r.unwrap_err() == CombatError::SpellOnCooldown);

    // 咒力不足（等冷却结束）
    p.status.mana = 0;
    battle.begin_round();  // 冷却 2→1，咒力 +2
    battle.begin_round();  // 冷却 1→0，咒力 +4
    CHECK(battle.player_cast(s).unwrap_err() == CombatError::InsufficientMana);
}

TEST_CASE("combat: item use heals and consumes")
{
    Player p;
    p.status.hp = 50;
    p.status.mana = 10;

    Item potion;
    potion.name = "恢复药水";
    potion.kind = ItemKind::Consumable;
    potion.effect.heal_hp = 20;
    potion.effect.heal_mana = 15;
    p.inventory.push_back(potion);

    Enemy e;
    e.hp = 500;
    e.power = 0;

    Rng rng{1};
    Battle::Config cfg;
    cfg.enemy_damage_variance = 0;
    Battle battle(p, e, rng, cfg);

    battle.begin_round();
    auto r = battle.player_use_item(0);
    REQUIRE(r.is_ok());
    CHECK(p.status.hp == 70);
    CHECK(p.status.mana == 27);  // 10 + 恢复2 + 回复15
    CHECK(p.inventory.empty());

    CHECK(battle.player_use_item(0).unwrap_err() == CombatError::ItemNotUsable);
}

TEST_CASE("combat: flee success depends on agility")
{
    Player p;
    p.special.agility = 30;  // 逃跑率 ≥ 100%

    Enemy e;
    e.hp = 100;
    e.power = 0;

    Rng rng{1};
    Battle::Config cfg;
    cfg.enemy_damage_variance = 0;
    Battle battle(p, e, rng, cfg);

    battle.begin_round();
    auto r = battle.player_flee();
    REQUIRE(r.is_ok());
    CHECK(r.unwrap().fled);
    CHECK(battle.is_over());
    CHECK(!battle.player_won());
}

TEST_CASE("combat: shield absorbs damage then hp, san lost from damage and san gap")
{
    Player p;
    p.status.hp = 100;
    p.status.san = 50;
    p.status.shield = 20;

    Enemy e;
    e.power = 10;
    e.san = 80;

    Rng rng{1};
    Battle::Config cfg;
    cfg.enemy_damage_variance = 0;
    Battle battle(p, e, rng, cfg);

    auto action = battle.enemy_act();
    CHECK(action.raw_damage == 10);
    CHECK(action.shield_absorbed == 10);
    CHECK(action.hp_lost == 0);
    CHECK(p.status.shield == 10);
    CHECK(action.san_lost == 0);  // 无血量损失

    auto action2 = battle.enemy_act();
    CHECK(action2.shield_absorbed == 10);
    CHECK(action2.hp_lost == 0);
    CHECK(p.status.shield == 0);

    // 破盾后扣血并造成理智损失：10×0.5 + (80-50)×0.2 = 11
    auto action3 = battle.enemy_act();
    CHECK(action3.shield_absorbed == 0);
    CHECK(action3.hp_lost == 10);
    CHECK(action3.san_lost == 11);
    CHECK(p.status.hp == 90);
    CHECK(p.status.san == 39);
}

TEST_CASE("combat: enemy can kill player")
{
    Player p;
    p.status.hp = 3;
    p.status.shield = 0;
    p.status.san = 50;

    Enemy e;
    e.power = 10;
    e.san = 50;

    Rng rng{1};
    Battle::Config cfg;
    cfg.enemy_damage_variance = 0;
    Battle battle(p, e, rng, cfg);

    auto action = battle.enemy_act();
    CHECK(action.player_died);
    CHECK(battle.is_over());
    CHECK(battle.player_died());
    CHECK(!battle.player_won());
}