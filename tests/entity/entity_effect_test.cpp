#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/element.hpp"
#include "core/instance.hpp"
#include "entity/base.hpp"
#include "entity/event.hpp"
#include "entity/status.hpp"

using treelang::Element;
using treelang::EntityDamagedEvent;
using treelang::EntityDiedEvent;
using treelang::EntityHealedEvent;
using treelang::EntitySanLostEvent;
using treelang::EntityStatusChangedEvent;
using treelang::EventBusInstance;

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

    entity::Entity make_entity(const char *id, int hp, int san = 10)
    {
        entity::StatusCollection st;
        st.get_hp() = entity::SingleStatus(hp);
        st.get_atk() = entity::SingleStatus(5);
        st.get_def() = entity::SingleStatus(3);
        st.get_spd() = entity::SingleStatus(4);
        st.get_san() = entity::SingleStatus(san);
        return entity::Entity(std::string(id), std::move(st));
    }
}

TEST_CASE("effect: take_damage publishes reason event before state event")
{
    static const char *const id = "fx_damage";
    static std::vector<DamageRecord> dmg;
    static std::vector<ChangedRecord> seen;
    auto &bus = EventBusInstance::instance().data();
    bus.subscribe<EntityDamagedEvent>(
        [](EntityDamagedEvent *e)
        {
            if (e->target == id)
                dmg.push_back(
                    {e->source, e->target, e->amount, e->shield_absorbed, e->hp_lost,
                     e->black_flash, e->get_sequence()});
        });
    bus.subscribe<EntityStatusChangedEvent>(
        [](EntityStatusChangedEvent *e)
        {
            if (e->entity_id == id)
                seen.push_back({e->attribute, e->old_cur, e->new_cur, e->get_sequence()});
        });

    auto e = make_entity(id, 20);
    CHECK(e.take_damage("goblin_1", 8, 3, 5, Element::Fire, true) == 5);

    REQUIRE(dmg.size() == 1);
    CHECK(dmg.back().source == "goblin_1");
    CHECK(dmg.back().target == id);
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
    static const char *const id = "fx_heal";
    static std::vector<int> healed;
    static std::vector<ChangedRecord> seen;
    auto &bus = EventBusInstance::instance().data();
    bus.subscribe<EntityHealedEvent>(
        [](EntityHealedEvent *e)
        {
            if (e->target == id)
                healed.push_back(e->amount);
        });
    bus.subscribe<EntityStatusChangedEvent>(
        [](EntityStatusChangedEvent *e)
        {
            if (e->entity_id == id)
                seen.push_back({e->attribute, e->old_cur, e->new_cur, e->get_sequence()});
        });

    auto e = make_entity(id, 20);
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

TEST_CASE("effect: lose_san clamps at zero, silent when empty")
{
    static const char *const id = "fx_san";
    static std::vector<int> san_lost;
    static std::vector<ChangedRecord> seen;
    auto &bus = EventBusInstance::instance().data();
    bus.subscribe<EntitySanLostEvent>(
        [](EntitySanLostEvent *e)
        {
            if (e->target == id)
                san_lost.push_back(e->amount);
        });
    bus.subscribe<EntityStatusChangedEvent>(
        [](EntityStatusChangedEvent *e)
        {
            if (e->entity_id == id)
                seen.push_back({e->attribute, e->old_cur, e->new_cur, e->get_sequence()});
        });

    auto e = make_entity(id, 20, 10);
    CHECK(e.lose_san(-1) == 0);  // 负值非法输入，静默
    CHECK(e.lose_san(3) == 3);
    CHECK(e.lose_san(99) == 7);  // 实际只损失 7
    CHECK(e.lose_san(1) == 0);  // 已为 0，静默

    REQUIRE(san_lost.size() == 2);
    CHECK(san_lost[0] == 3);
    CHECK(san_lost[1] == 7);
    REQUIRE(seen.size() == 2);
    CHECK(seen[0].attr == "san");
    CHECK(seen[0].old_cur == 10);
    CHECK(seen[0].new_cur == 7);
    CHECK(seen[1].old_cur == 7);
    CHECK(seen[1].new_cur == 0);
    CHECK(e.get_status().get_san().get_cur() == 0);
}

TEST_CASE("effect: lethal take_damage orders reason, changed, died")
{
    static const char *const id = "fx_lethal";
    static std::vector<DamageRecord> dmg;
    static std::vector<ChangedRecord> seen;
    static std::vector<std::uint64_t> died_seq;
    auto &bus = EventBusInstance::instance().data();
    bus.subscribe<EntityDamagedEvent>(
        [](EntityDamagedEvent *e)
        {
            if (e->target == id)
                dmg.push_back(
                    {e->source, e->target, e->amount, e->shield_absorbed, e->hp_lost,
                     e->black_flash, e->get_sequence()});
        });
    bus.subscribe<EntityStatusChangedEvent>(
        [](EntityStatusChangedEvent *e)
        {
            if (e->entity_id == id)
                seen.push_back({e->attribute, e->old_cur, e->new_cur, e->get_sequence()});
        });
    bus.subscribe<EntityDiedEvent>(
        [](EntityDiedEvent *e)
        {
            if (e->entity_id == id)
                died_seq.push_back(e->get_sequence());
        });

    auto e = make_entity(id, 3);
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
    static const char *const thorns_id = "fx_thorns";
    static const char *const atk_id = "fx_thorns_atk";
    static std::vector<int> atk_hp_lost;
    static entity::Entity *hunter = nullptr;

    auto &bus = EventBusInstance::instance().data();
    bus.subscribe<EntityDamagedEvent>(
        [](EntityDamagedEvent *e)
        {
            if (e->target == atk_id)
                atk_hp_lost.push_back(e->hp_lost);
        });
    // 荆棘之鳞：本实体受伤时反弹 33%（向下取整）。hunter 仅在 target 命中时解引用，
    // 其他用例不会发布 target 为 fx_thorns 的 Damage 事件，故用例结束后不悬垂。
    bus.subscribe<EntityDamagedEvent>(
        [](EntityDamagedEvent *e)
        {
            if (e->target != thorns_id)
                return;
            const int back = e->amount * 33 / 100;
            if (back > 0)
                hunter->take_damage(thorns_id, back, 0, back, std::nullopt, false);
        });

    auto beetle = make_entity(thorns_id, 20);
    auto attacker = make_entity(atk_id, 30);
    hunter = &attacker;

    beetle.take_damage(atk_id, 12, 0, 12, std::nullopt, false);

    REQUIRE(atk_hp_lost.size() == 1);
    CHECK(atk_hp_lost.back() == 3);  // floor(12 * 33%)
    CHECK(attacker.get_status().get_hp().get_cur() == 27);
    CHECK(beetle.get_status().get_hp().get_cur() == 8);
}
