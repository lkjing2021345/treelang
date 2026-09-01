#include <doctest/doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/event.hpp"
#include "core/event_bus.hpp"
#include "core/instance.hpp"

using treelang::Event;
using treelang::EventBus;
using treelang::EventBusInstance;

namespace
{
    struct SpellCastEvent : Event
    {
        std::string spell_id;
        std::string_view type_tag() const noexcept override { return "SpellCastEvent"; }
    };

    struct EnemyHitEvent : Event
    {
        int damage = 0;
        std::string_view type_tag() const noexcept override { return "EnemyHitEvent"; }
    };

    struct PlainEvent : Event
    {
    };
}

TEST_CASE("event: default sequence and round-trip")
{
    Event e;
    CHECK(e.get_sequence() == 0);

    e.set_sequence(42);
    CHECK(e.get_sequence() == 42);
}

TEST_CASE("event: derived event overrides tag and works via base pointer")
{
    auto cast = std::make_shared<SpellCastEvent>();
    cast->spell_id = "fireball";
    cast->set_sequence(7);

    const Event *base = cast.get();
    CHECK(base->type_tag() == "SpellCastEvent");
    CHECK(base->get_sequence() == 7);
}

TEST_CASE("event: derived event without override inherits base tag")
{
    PlainEvent p;
    CHECK(p.type_tag() == "Event");
}

TEST_CASE("event_bus: publish invokes matching listener with same object")
{
    EventBus bus;
    bool called = false;
    const SpellCastEvent *received = nullptr;

    bus.subscribe<SpellCastEvent>([&](SpellCastEvent *event) {
        called = true;
        received = event;
    });

    auto event = std::make_shared<SpellCastEvent>();
    event->spell_id = "fireball";
    bus.publish(event);

    CHECK(called);
    REQUIRE(received != nullptr);
    CHECK(received == event.get());
    CHECK(received->spell_id == "fireball");
}

TEST_CASE("event_bus: exact type dispatch, no cross-type delivery")
{
    EventBus bus;
    int cast_calls = 0;
    int hit_calls = 0;

    bus.subscribe<SpellCastEvent>([&](SpellCastEvent *) { ++cast_calls; });
    bus.subscribe<EnemyHitEvent>([&](EnemyHitEvent *) { ++hit_calls; });

    bus.publish(std::make_shared<SpellCastEvent>());
    CHECK(cast_calls == 1);
    CHECK(hit_calls == 0);

    bus.publish(std::make_shared<EnemyHitEvent>());
    CHECK(cast_calls == 1);
    CHECK(hit_calls == 1);
}

TEST_CASE("event_bus: subscribing to base does not receive derived events")
{
    EventBus bus;
    int base_calls = 0;
    bus.subscribe<Event>([&](Event *) { ++base_calls; });

    bus.publish(std::make_shared<SpellCastEvent>());
    CHECK(base_calls == 0);  // 精确类型分发，基类订阅不命中细化事件
}

TEST_CASE("event_bus: multiple listeners all invoked")
{
    EventBus bus;
    int count = 0;
    bus.subscribe<SpellCastEvent>([&](SpellCastEvent *) { ++count; });
    bus.subscribe<SpellCastEvent>([&](SpellCastEvent *) { ++count; });

    bus.publish(std::make_shared<SpellCastEvent>());
    CHECK(count == 2);
}

TEST_CASE("event_bus: publish without subscribers still assigns sequence")
{
    EventBus bus;
    bus.publish(std::make_shared<EnemyHitEvent>());
    CHECK(bus.sequence() == 1);
}

TEST_CASE("event_bus: sequence increments globally across types")
{
    EventBus bus;
    std::vector<std::uint64_t> seen;
    bus.subscribe<SpellCastEvent>([&](SpellCastEvent *e) { seen.push_back(e->get_sequence()); });
    bus.subscribe<EnemyHitEvent>([&](EnemyHitEvent *e) { seen.push_back(e->get_sequence()); });

    bus.publish(std::make_shared<SpellCastEvent>());
    bus.publish(std::make_shared<EnemyHitEvent>());
    bus.publish(std::make_shared<SpellCastEvent>());

    REQUIRE(seen.size() == 3);
    CHECK(seen[0] == 1);
    CHECK(seen[1] == 2);
    CHECK(seen[2] == 3);
    CHECK(bus.sequence() == 3);
}

TEST_CASE("event_bus: independent buses have independent counters and listeners")
{
    EventBus a;
    EventBus b;
    int a_calls = 0;
    a.subscribe<SpellCastEvent>([&](SpellCastEvent *) { ++a_calls; });

    a.publish(std::make_shared<SpellCastEvent>());
    b.publish(std::make_shared<SpellCastEvent>());

    CHECK(a_calls == 1);  // b 的发布不影响 a 的监听者
    CHECK(a.sequence() == 1);
    CHECK(b.sequence() == 1);
}

TEST_CASE("instance: EventBusInstance is a singleton")
{
    auto &a = EventBusInstance::instance();
    auto &b = EventBusInstance::instance();
    CHECK(&a == &b);
    CHECK(&a.data() == &b.data());
}

TEST_CASE("instance: publish/subscribe through singleton works")
{
    static int calls = 0;
    auto &bus = EventBusInstance::instance().data();
    bus.subscribe<EnemyHitEvent>([&](EnemyHitEvent *e) {
        ++calls;
        CHECK(e->damage == 5);
    });

    auto hit = std::make_shared<EnemyHitEvent>();
    hit->damage = 5;
    bus.publish(hit);

    CHECK(calls == 1);
    CHECK(bus.sequence() >= 1);  // 单例计数器持续累加
}