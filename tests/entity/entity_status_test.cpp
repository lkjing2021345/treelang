#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/instance.hpp"
#include "entity/base.hpp"
#include "entity/event.hpp"
#include "entity/status.hpp"

using treelang::EntityDiedEvent;
using treelang::EntityStatusChangedEvent;
using treelang::EntityStatusMaxChangedEvent;
using treelang::EventBusInstance;

namespace entity = treelang::entity;

namespace
{
    // attr 指向 StatusCollection 内的静态字面量，存 string_view 即可
    struct ChangedRecord
    {
        std::string_view attr;
        int old_cur = 0;
        int new_cur = 0;
        int tot = 0;
    };

    struct MaxRecord
    {
        std::string_view attr;
        int old_tot = 0;
        int new_tot = 0;
        int cur = 0;
    };

    // 单例总线跨 TEST_CASE 存活：用函数内 static 承载记录与 id，
    // 无捕获 lambda 保证监听器永不悬垂，各用例以唯一 id 隔离。
    entity::Entity make_entity(const char *id, int hp)
    {
        entity::StatusCollection st;
        st.get_hp() = entity::SingleStatus(hp);
        st.get_atk() = entity::SingleStatus(5);
        st.get_def() = entity::SingleStatus(3);
        st.get_spd() = entity::SingleStatus(4);
        st.get_san() = entity::SingleStatus(10);
        return entity::Entity(std::string(id), std::move(st));
    }
}

TEST_CASE("status: set_cur fires EntityStatusChangedEvent with old/new/tot")
{
    static const char *const id = "st_set_cur";
    static std::vector<ChangedRecord> seen;
    auto &bus = EventBusInstance::instance().data();
    bus.subscribe<EntityStatusChangedEvent>(
        [](EntityStatusChangedEvent *e)
        {
            if (e->entity_id == id)
                seen.push_back({e->attribute, e->old_cur, e->new_cur, e->tot});
        });

    auto e = make_entity(id, 20);
    CHECK(e.get_status().get_hp().set_cur(7));

    REQUIRE(!seen.empty());
    const ChangedRecord &r = seen.back();
    CHECK(r.attr == "hp");
    CHECK(r.old_cur == 20);
    CHECK(r.new_cur == 7);
    CHECK(r.tot == 20);
}

TEST_CASE("status: no-op set_cur fires nothing")
{
    static const char *const id = "st_noop";
    static std::vector<ChangedRecord> seen;
    auto &bus = EventBusInstance::instance().data();
    bus.subscribe<EntityStatusChangedEvent>(
        [](EntityStatusChangedEvent *e)
        {
            if (e->entity_id == id)
                seen.push_back({e->attribute, e->old_cur, e->new_cur, e->tot});
        });

    auto e = make_entity(id, 20);
    CHECK(e.get_status().get_hp().set_cur(20));  // 与当前值相同

    CHECK(seen.empty());
}

TEST_CASE("status: invalid set_cur fires nothing")
{
    static const char *const id = "st_invalid";
    static std::vector<ChangedRecord> seen;
    auto &bus = EventBusInstance::instance().data();
    bus.subscribe<EntityStatusChangedEvent>(
        [](EntityStatusChangedEvent *e)
        {
            if (e->entity_id == id)
                seen.push_back({e->attribute, e->old_cur, e->new_cur, e->tot});
        });

    auto e = make_entity(id, 20);
    CHECK(!e.get_status().get_hp().set_cur(-1));
    CHECK(!e.get_status().get_hp().set_cur(21));

    CHECK(seen.empty());
    CHECK(e.get_status().get_hp().get_cur() == 20);
}

TEST_CASE("status: add/sub route through set_cur and clamp")
{
    static const char *const id = "st_addsub";
    static std::vector<ChangedRecord> seen;
    auto &bus = EventBusInstance::instance().data();
    bus.subscribe<EntityStatusChangedEvent>(
        [](EntityStatusChangedEvent *e)
        {
            if (e->entity_id == id)
                seen.push_back({e->attribute, e->old_cur, e->new_cur, e->tot});
        });

    auto e = make_entity(id, 20);
    CHECK(e.get_status().get_hp().sub(5) == 5);
    REQUIRE(seen.size() == 1);
    CHECK(seen.back().old_cur == 20);
    CHECK(seen.back().new_cur == 15);

    CHECK(e.get_status().get_hp().add(3) == 3);
    REQUIRE(seen.size() == 2);
    CHECK(seen.back().old_cur == 15);
    CHECK(seen.back().new_cur == 18);

    CHECK(e.get_status().get_hp().add(99) == 2);  // 夹到 tot，只加 2
    REQUIRE(seen.size() == 3);
    CHECK(seen.back().old_cur == 18);
    CHECK(seen.back().new_cur == 20);

    CHECK(e.get_status().get_hp().add(1) == 0);  // 已满，无变化
    CHECK(seen.size() == 3);

    CHECK(e.get_status().get_hp().sub(99) == 20);  // 夹到 0
    REQUIRE(seen.size() == 4);
    CHECK(seen.back().old_cur == 20);
    CHECK(seen.back().new_cur == 0);
}

TEST_CASE("status: set_tot fires MaxChanged, clamp fires extra Changed")
{
    static const char *const id = "st_set_tot";
    static std::vector<MaxRecord> max_seen;
    static std::vector<ChangedRecord> seen;
    auto &bus = EventBusInstance::instance().data();
    bus.subscribe<EntityStatusMaxChangedEvent>(
        [](EntityStatusMaxChangedEvent *e)
        {
            if (e->entity_id == id)
                max_seen.push_back({e->attribute, e->old_tot, e->new_tot, e->cur});
        });
    bus.subscribe<EntityStatusChangedEvent>(
        [](EntityStatusChangedEvent *e)
        {
            if (e->entity_id == id)
                seen.push_back({e->attribute, e->old_cur, e->new_cur, e->tot});
        });

    auto e = make_entity(id, 20);
    CHECK(e.get_status().get_hp().set_tot(10));  // cur 20 被夹到 10

    REQUIRE(max_seen.size() == 1);
    CHECK(max_seen.back().attr == "hp");
    CHECK(max_seen.back().old_tot == 20);
    CHECK(max_seen.back().new_tot == 10);
    CHECK(max_seen.back().cur == 10);
    REQUIRE(seen.size() == 1);  // 夹紧伴随一次 cur 变化
    CHECK(seen.back().old_cur == 20);
    CHECK(seen.back().new_cur == 10);
    CHECK(seen.back().tot == 10);

    CHECK(e.get_status().get_hp().set_tot(30));  // 只涨上限，cur 不变
    REQUIRE(max_seen.size() == 2);
    CHECK(max_seen.back().old_tot == 10);
    CHECK(max_seen.back().new_tot == 30);
    CHECK(max_seen.back().cur == 10);
    CHECK(seen.size() == 1);

    CHECK(!e.get_status().get_hp().set_tot(-1));
    CHECK(max_seen.size() == 2);
}

TEST_CASE("status: hp zeroing transition fires EntityDiedEvent once, after changed")
{
    static const char *const id = "st_died";
    static std::vector<ChangedRecord> seen;
    static std::vector<std::uint64_t> died_seq;
    static std::vector<std::uint64_t> last_changed_seq;
    auto &bus = EventBusInstance::instance().data();
    bus.subscribe<EntityStatusChangedEvent>(
        [](EntityStatusChangedEvent *e)
        {
            if (e->entity_id == id)
            {
                seen.push_back({e->attribute, e->old_cur, e->new_cur, e->tot});
                last_changed_seq.push_back(e->get_sequence());
            }
        });
    bus.subscribe<EntityDiedEvent>(
        [](EntityDiedEvent *e)
        {
            if (e->entity_id == id)
                died_seq.push_back(e->get_sequence());
        });

    auto e = make_entity(id, 1);
    e.get_status().get_hp().sub(1);  // 1 -> 0

    REQUIRE(seen.size() == 1);
    CHECK(seen.back().old_cur == 1);
    CHECK(seen.back().new_cur == 0);
    REQUIRE(died_seq.size() == 1);
    CHECK(died_seq[0] > last_changed_seq[0]);  // 先状态变化，后死亡

    e.get_status().get_hp().set_cur(1);  // 复活
    e.get_status().get_hp().sub(99);     // 再死一次
    CHECK(died_seq.size() == 2);

    e.get_status().get_hp().sub(5);  // 已是 0，无变化，不再触发
    CHECK(died_seq.size() == 2);
    CHECK(e.get_status().get_hp().get_cur() == 0);
}

TEST_CASE("status_collection: builder sets cur and tot to the given value")
{
    entity::StatusCollection st =
        entity::StatusCollection::create().hp(20).atk(5).def(3).spd(4).san(10).build();

    CHECK(st.get_hp().get_cur() == 20);
    CHECK(st.get_hp().get_tot() == 20);
    CHECK(st.get_atk().get_cur() == 5);
    CHECK(st.get_atk().get_tot() == 5);
    CHECK(st.get_def().get_cur() == 3);
    CHECK(st.get_def().get_tot() == 3);
    CHECK(st.get_spd().get_cur() == 4);
    CHECK(st.get_spd().get_tot() == 4);
    CHECK(st.get_san().get_cur() == 10);
    CHECK(st.get_san().get_tot() == 10);
}

TEST_CASE("status: each attribute wired independently with its own name")
{
    static const char *const id = "st_attrs";
    static std::vector<ChangedRecord> seen;
    auto &bus = EventBusInstance::instance().data();
    bus.subscribe<EntityStatusChangedEvent>(
        [](EntityStatusChangedEvent *e)
        {
            if (e->entity_id == id)
                seen.push_back({e->attribute, e->old_cur, e->new_cur, e->tot});
        });

    auto e = make_entity(id, 20);
    e.get_status().get_atk().sub(2);
    e.get_status().get_def().sub(1);
    e.get_status().get_spd().sub(2);
    e.get_status().get_san().sub(3);

    REQUIRE(seen.size() == 4);
    CHECK(seen[0].attr == "atk");
    CHECK(seen[0].new_cur == 3);
    CHECK(seen[1].attr == "def");
    CHECK(seen[1].new_cur == 2);
    CHECK(seen[2].attr == "spd");
    CHECK(seen[2].new_cur == 2);
    CHECK(seen[3].attr == "san");
    CHECK(seen[3].new_cur == 7);
}
