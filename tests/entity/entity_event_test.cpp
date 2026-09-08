#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <string_view>

#include "core/event.hpp"
#include "core/event_bus.hpp"
#include "core/handler.hpp"
#include "entity/event.hpp"

using treelang::Element;
using treelang::Event;
using treelang::EventBus;
using treelang::Handler;
using treelang::HandlerContext;
using treelang::EntityDiedEvent;
using treelang::EntityDamagedEvent;
using treelang::EntityStatusChangedEvent;
using treelang::EntityStatusMaxChangedEvent;

TEST_CASE("entity_event: status changed carries old/new/tot and tag")
{
    EventBus bus;
    const EntityStatusChangedEvent *received = nullptr;
    auto h = bus.subscribe(Handler<EntityStatusChangedEvent>(
        [&](HandlerContext<EntityStatusChangedEvent> &ctx) { received = &ctx.event; }));

    auto ev = std::make_shared<EntityStatusChangedEvent>();
    ev->entity_id = "player";
    ev->attribute = "hp";
    ev->old_cur = 10;
    ev->new_cur = 7;
    ev->tot = 20;
    bus.publish(ev);

    REQUIRE(received != nullptr);
    CHECK(received->entity_id == "player");
    CHECK(received->attribute == "hp");
    CHECK(received->old_cur == 10);
    CHECK(received->new_cur == 7);
    CHECK(received->tot == 20);
    CHECK(received->type_tag() == "EntityStatusChangedEvent");
}

TEST_CASE("entity_event: type tag resolvable through base Event pointer")
{
    auto died = std::make_shared<EntityDiedEvent>();
    died->entity_id = "goblin_1";

    const Event *base = died.get();
    CHECK(base->type_tag() == "EntityDiedEvent");
}

TEST_CASE("entity_event: status max changed carries old/new tot and current")
{
    EventBus bus;
    const EntityStatusMaxChangedEvent *received = nullptr;
    auto h = bus.subscribe(Handler<EntityStatusMaxChangedEvent>(
        [&](HandlerContext<EntityStatusMaxChangedEvent> &ctx) { received = &ctx.event; }));

    auto ev = std::make_shared<EntityStatusMaxChangedEvent>();
    ev->entity_id = "player";
    ev->attribute = "atk";
    ev->old_tot = 3;
    ev->new_tot = 5;
    ev->cur = 4;
    bus.publish(ev);

    REQUIRE(received != nullptr);
    CHECK(received->entity_id == "player");
    CHECK(received->attribute == "atk");
    CHECK(received->old_tot == 3);
    CHECK(received->new_tot == 5);
    CHECK(received->cur == 4);
}

TEST_CASE("entity_event: reason event does not leak into state listeners")
{
    EventBus bus;
    int status_calls = 0;
    const EntityDamagedEvent *dmg = nullptr;
    auto h1 = bus.subscribe(Handler<EntityStatusChangedEvent>(
        [&](HandlerContext<EntityStatusChangedEvent> &) { ++status_calls; }));
    auto h2 = bus.subscribe(Handler<EntityDamagedEvent>(
        [&](HandlerContext<EntityDamagedEvent> &ctx) { dmg = &ctx.event; }));

    auto ev = std::make_shared<EntityDamagedEvent>();
    ev->source = "goblin_1";
    ev->target = "player";
    ev->amount = 5;
    ev->shield_absorbed = 2;
    ev->hp_lost = 3;
    ev->element = Element::Fire;
    ev->black_flash = true;
    bus.publish(ev);

    REQUIRE(dmg != nullptr);
    CHECK(dmg->source == "goblin_1");
    CHECK(dmg->target == "player");
    CHECK(dmg->amount == 5);
    CHECK(dmg->shield_absorbed == 2);
    CHECK(dmg->hp_lost == 3);
    CHECK(dmg->element == Element::Fire);
    CHECK(dmg->black_flash);
    CHECK(status_calls == 0);
}

TEST_CASE("entity_event: publishes are ordered by sequence")
{
    EventBus bus;
    int first = -1;
    int second = -1;
    int n = 0;
    auto h = bus.subscribe(Handler<EntityDiedEvent>(
        [&](HandlerContext<EntityDiedEvent> &ctx)
        {
            if (n++ == 0) first = ctx.event.get_sequence();
            else second = ctx.event.get_sequence();
        }));

    auto a = std::make_shared<EntityDiedEvent>();
    a->entity_id = "goblin_1";
    bus.publish(a);
    auto b = std::make_shared<EntityDiedEvent>();
    b->entity_id = "goblin_2";
    bus.publish(b);

    CHECK(first == 1);
    CHECK(second == 2);
}
