#include <doctest/doctest.h>

#include "model/entity.hpp"
#include "model/item.hpp"
#include "model/room.hpp"
#include "model/spell.hpp"

using treelang::Element;
using treelang::ElementAttr;
using treelang::Enemy;
using treelang::fuse_spells;
using treelang::Item;
using treelang::ItemKind;
using treelang::Npc;
using treelang::Player;
using treelang::Room;
using treelang::RoomType;
using treelang::SpecialAttributes;
using treelang::Spell;
using treelang::SpellBook;
using treelang::VitalStatus;

TEST_CASE("model: spell basics")
{
    Spell s;
    s.id = "s1";
    s.name = "火球";
    s.elements = {ElementAttr{Element::Fire, 2}};
    s.cooldown = 3;
    s.base_multiplier = 120;

    CHECK(s.id == "s1");
    CHECK(s.name == "火球");
    CHECK(s.cooldown == 3);
    CHECK(s.base_multiplier == 120);
    REQUIRE(s.elements.size() == 1);
    CHECK(s.elements[0].element == Element::Fire);
    CHECK(s.elements[0].level == 2);
}

TEST_CASE("model: fuse_spells merges elements and sums levels")
{
    Spell a;
    a.elements = {ElementAttr{Element::Fire, 1}};
    Spell b;
    b.elements = {ElementAttr{Element::Fire, 2}, ElementAttr{Element::Water, 1}};

    Spell fused = fuse_spells(a, b);
    REQUIRE(fused.elements.size() == 2);

    CHECK(fused.elements[0].element == Element::Fire);
    CHECK(fused.elements[0].level == 3);
    CHECK(fused.elements[1].element == Element::Water);
    CHECK(fused.elements[1].level == 1);
}

TEST_CASE("model: fuse_spells keeps disjoint elements")
{
    Spell a;
    a.elements = {ElementAttr{Element::Metal, 5}};
    Spell b;
    b.elements = {ElementAttr{Element::Water, 2}};

    Spell fused = fuse_spells(a, b);
    REQUIRE(fused.elements.size() == 2);
    CHECK(fused.elements[0].level == 5);
    CHECK(fused.elements[1].level == 2);
}

TEST_CASE("model: spellbook add/find/remove/iterate")
{
    SpellBook book;
    CHECK(book.size() == 0);

    Spell s;
    s.id = "s1";
    s.name = "火球";
    book.add(s);
    s.id = "s2";
    s.name = "水刃";
    book.add(s);

    CHECK(book.size() == 2);

    const Spell *found = book.find("s1");
    REQUIRE(found != nullptr);
    CHECK(found->name == "火球");
    CHECK(book.find("missing") == nullptr);

    Spell *mutable_found = book.find("s2");
    REQUIRE(mutable_found != nullptr);
    mutable_found->base_multiplier = 150;
    CHECK(book.find("s2")->base_multiplier == 150);

    CHECK(book.remove("missing") == false);
    CHECK(book.remove("s1") == true);
    CHECK(book.size() == 1);

    int count = 0;
    for (const Spell &spell : book) ++count;
    CHECK(count == 1);
}

TEST_CASE("model: entity defaults")
{
    Player p;
    CHECK(p.level == 1);
    CHECK(p.attribute_points == 0);
    CHECK(p.gold == 0);
    CHECK(p.spells.size() == 0);
    CHECK(p.inventory.empty());

    VitalStatus v;
    CHECK(v.hp == 0);
    CHECK(v.mana == 0);
    CHECK(v.san == 0);
    CHECK(v.shield == 0);

    SpecialAttributes attrs;
    CHECK(attrs.strength == 0);
    CHECK(attrs.perception == 0);
    CHECK(attrs.endurance == 0);
    CHECK(attrs.charisma == 0);
    CHECK(attrs.intelligence == 0);
    CHECK(attrs.agility == 0);
    CHECK(attrs.luck == 0);
}

TEST_CASE("model: enemy weaknesses")
{
    Enemy e;
    e.name = "熔岩魔像";
    e.hp = 50;
    e.max_hp = 50;
    e.power = 8;
    e.weaknesses = {Element::Water, Element::Metal};

    CHECK(e.name == "熔岩魔像");
    CHECK(e.max_hp == 50);
    REQUIRE(e.weaknesses.size() == 2);
    CHECK(e.weaknesses[0] == Element::Water);
    CHECK(e.weaknesses[1] == Element::Metal);
}

TEST_CASE("model: npc san")
{
    Npc n;
    n.name = "旅商";
    n.san = 40;
    CHECK(n.name == "旅商");
    CHECK(n.san == 40);
}

TEST_CASE("model: room content by type")
{
    Room room;
    CHECK(room.type == RoomType::Start);
    CHECK(room.content.gold_reward == 0);

    room.type = RoomType::Elite;
    room.content.enemies.push_back(Enemy{});
    CHECK(room.content.enemies.size() == 1);

    room.type = RoomType::Reward;
    room.content.loot.push_back(Item{});
    room.content.spell_rewards.push_back(Spell{});
    room.content.gold_reward = 100;
    CHECK(room.content.loot.size() == 1);
    CHECK(room.content.spell_rewards.size() == 1);
    CHECK(room.content.gold_reward == 100);
}

TEST_CASE("model: item kind")
{
    Item potion;
    CHECK(potion.kind == ItemKind::Consumable);
    potion.kind = ItemKind::Tactical;
    CHECK(potion.kind == ItemKind::Tactical);
    CHECK(static_cast<int>(ItemKind::Consumable) == 0);
    CHECK(static_cast<int>(ItemKind::Tactical) == 1);
}