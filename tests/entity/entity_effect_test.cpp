#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/element.hpp"
#include "core/event_bus.hpp"
#include "core/handler.hpp"
#include "entity/base.hpp"
#include "entity/event.hpp"
#include "entity/status.hpp"

using treelang::Element;
using treelang::EntityDamagedEvent;
using treelang::EntityDiedEvent;
using treelang::EntityHealedEvent;
using treelang::EntityStatusChangedEvent;
using treelang::EventBus;
using treelang::Handler;
using treelang::HandlerContext;
using treelang::HandlerPriority;

namespace entity = treelang::entity;

namespace
{
    struct DamageRecord
    {
        std::string source;
        std::string target;
        int amount = 0;
        int shield_absorbed = 0;
        int hp_lost = 0;
        bool black_flash = false;
        std::uint64_t seq = 0;
    };

    struct ChangedRecord
    {
        std::string_view attr;
        int old_cur = 0;
        int new_cur = 0;
        std::uint64_t seq = 0;
    };

    entity::Entity make_entity(const char *id, int hp, EventBus &bus)
    {
        entity::StatusCollection st;
        st.get_hp() = entity::SingleStatus(hp);
        st.get_atk() = entity::SingleStatus(5);
        st.get_def() = entity::SingleStatus(3);
        return entity::Entity(std::string(id), std::move(st), bus);
    }
}

TEST_CASE("effect: take_damage publishes reason event before state event")
{
    EventBus bus;
    std::vector<DamageRecord> dmg;
    std::vector<ChangedRecord> seen;
    auto dmg_watch = bus.subscribe(Handler<EntityDamagedEvent>(
        [&](HandlerContext<EntityDamagedEvent> &ctx)
        {
            const auto &e = ctx.event;
            dmg.push_back({e.source, e.target, e.amount, e.shield_absorbed, e.hp_lost,
                           e.black_flash, e.get_sequence()});
        }));
    auto chg_watch = bus.subscribe(Handler<EntityStatusChangedEvent>(
        [&](HandlerContext<EntityStatusChangedEvent> &ctx)
        {
            seen.push_back(
                {ctx.event.attribute, ctx.event.old_cur, ctx.event.new_cur,
                 ctx.event.get_sequence()});
        }));

    auto e = make_entity("fx_damage", 20, bus);
    CHECK(e.take_damage("goblin_1", 8, 3, 5, Element::Fire, true) == 5);

    REQUIRE(dmg.size() == 1);
    CHECK(dmg.back().source == "goblin_1");
    CHECK(dmg.back().target == "fx_damage");
    CHECK(dmg.back().amount == 8);
    CHECK(dmg.back().shield_absorbed == 3);
    CHECK(dmg.back().hp_lost == 5);
    CHECK(dmg.back().black_flash);
    REQUIRE(seen.size() == 1);
    CHECK(seen.back().attr == "hp");
    CHECK(seen.back().old_cur == 20);
    CHECK(seen.back().new_cur == 15);
    CHECK(dmg.back().seq < seen.back().seq);  // 原因层先于状态层
    CHECK(e.get_status().get_hp().get_cur() == 15);

    CHECK(e.take_damage("goblin_2", 4, 4, 0, std::nullopt, false) == 0);  // 全额被盾吸收
    CHECK(dmg.size() == 2);
    CHECK(dmg.back().hp_lost == 0);
    CHECK(seen.size() == 1);  // 无状态变化
    CHECK(e.get_status().get_hp().get_cur() == 15);
}

TEST_CASE("effect: heal clamps at max hp, silent when full")
{
    EventBus bus;
    std::vector<int> healed;
    std::vector<ChangedRecord> seen;
    auto heal_watch = bus.subscribe(Handler<EntityHealedEvent>(
        [&](HandlerContext<EntityHealedEvent> &ctx) { healed.push_back(ctx.event.amount); }));
    auto chg_watch = bus.subscribe(Handler<EntityStatusChangedEvent>(
        [&](HandlerContext<EntityStatusChangedEvent> &ctx)
        {
            seen.push_back(
                {ctx.event.attribute, ctx.event.old_cur, ctx.event.new_cur,
                 ctx.event.get_sequence()});
        }));

    auto e = make_entity("fx_heal", 20, bus);
    CHECK(e.heal(-1) == 0);  // 负值非法输入，静默
    CHECK(e.heal(5) == 0);  // 已满，静默
    CHECK(healed.empty());
    CHECK(seen.empty());
    CHECK(e.get_status().get_hp().get_cur() == 20);

    CHECK(e.take_damage("x", 5, 0, 5, std::nullopt, false) == 5);  // 20 -> 15
    CHECK(e.heal(9) == 5);  // 实际只恢复 5
    REQUIRE(healed.size() == 1);
    CHECK(healed.back() == 5);
    REQUIRE(seen.size() == 2);  // 扣血 + 回血各一次
    CHECK(seen.back().old_cur == 15);
    CHECK(seen.back().new_cur == 20);
    CHECK(e.get_status().get_hp().get_cur() == 20);
}

TEST_CASE("effect: lethal take_damage orders reason, changed, died")
{
    EventBus bus;
    std::vector<DamageRecord> dmg;
    std::vector<ChangedRecord> seen;
    std::vector<std::uint64_t> died_seq;
    auto dmg_watch = bus.subscribe(Handler<EntityDamagedEvent>(
        [&](HandlerContext<EntityDamagedEvent> &ctx)
        {
            const auto &e = ctx.event;
            dmg.push_back({e.source, e.target, e.amount, e.shield_absorbed, e.hp_lost,
                           e.black_flash, e.get_sequence()});
        }));
    auto chg_watch = bus.subscribe(Handler<EntityStatusChangedEvent>(
        [&](HandlerContext<EntityStatusChangedEvent> &ctx)
        {
            seen.push_back(
                {ctx.event.attribute, ctx.event.old_cur, ctx.event.new_cur,
                 ctx.event.get_sequence()});
        }));
    auto died_watch = bus.subscribe(Handler<EntityDiedEvent>(
        [&](HandlerContext<EntityDiedEvent> &ctx) { died_seq.push_back(ctx.event.get_sequence()); }));

    auto e = make_entity("fx_lethal", 3, bus);
    CHECK(e.take_damage("boss", 5, 0, 5, Element::Water, false) == 3);  // 3 -> 0，超杀只记 3

    REQUIRE(dmg.size() == 1);
    REQUIRE(seen.size() == 1);
    CHECK(seen.back().old_cur == 3);
    CHECK(seen.back().new_cur == 0);
    REQUIRE(died_seq.size() == 1);
    CHECK(dmg.back().seq < seen.back().seq);
    CHECK(seen.back().seq < died_seq[0]);
}

TEST_CASE("effect: thorns-style logic binds to damage event (33% reflect)")
{
    EventBus bus;
    std::vector<int> atk_hp_lost;

    auto beetle = make_entity("fx_thorns", 20, bus);
    auto attacker = make_entity("fx_thorns_atk", 30, bus);

    // 关注点一：记录 attacker 受到的伤害
    auto log_watch = bus.subscribe(Handler<EntityDamagedEvent>(
        [&](const EntityDamagedEvent &e) { return e.target == attacker.get_id(); },
        [&](HandlerContext<EntityDamagedEvent> &ctx) { atk_hp_lost.push_back(ctx.event.hp_lost); }));

    // 关注点二：荆棘之鳞——独立 handler，只关心「自己」受伤，只做反弹一件事。
    // 反弹伤害经 take_damage 再发布 EntityDamagedEvent（重入分发），
    // 上面第一个 handler 负责把它记下来。
    auto thorns = bus.subscribe(Handler<EntityDamagedEvent>(
        [&](const EntityDamagedEvent &e) { return e.target == beetle.get_id(); },
        [&](HandlerContext<EntityDamagedEvent> &ctx)
        {
            const int back = ctx.event.amount * 33 / 100;
            if (back > 0)
                attacker.take_damage(beetle.get_id(), back, 0, back, std::nullopt, false);
        },
        HandlerPriority::Last));

    beetle.take_damage(attacker.get_id(), 12, 0, 12, std::nullopt, false);

    REQUIRE(atk_hp_lost.size() == 1);
    CHECK(atk_hp_lost.back() == 3);  // floor(12 * 33%)
    CHECK(attacker.get_status().get_hp().get_cur() == 27);
    CHECK(beetle.get_status().get_hp().get_cur() == 8);

    // 句柄退订后，反弹不再发生
    thorns.reset();
    beetle.take_damage(attacker.get_id(), 10, 0, 10, std::nullopt, false);
    CHECK(atk_hp_lost.size() == 1);
    CHECK(attacker.get_status().get_hp().get_cur() == 27);
}
