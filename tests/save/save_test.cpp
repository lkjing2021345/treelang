#include "save/save.hpp"

#include <doctest/doctest.h>

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>

#include "core/types.hpp"
#include "model/entity.hpp"
#include "model/item.hpp"
#include "model/spell.hpp"

using treelang::Element;
using treelang::ElementAttr;
using treelang::Enemy;
using treelang::GameSave;
using treelang::Item;
using treelang::ItemKind;
using treelang::kCenterCol;
using treelang::kCenterRow;
using treelang::MapGrid;
using treelang::Npc;
using treelang::Player;
using treelang::Room;
using treelang::RoomType;
using treelang::SaveError;
using treelang::SaveManager;
using treelang::Spell;
using treelang::SpellBook;

namespace fs = std::filesystem;

/**
 * @brief 泛型 round-trip：to_json → from_json。
 */
template <typename T>
static std::optional<T> roundtrip(const T &value)
{
    auto json = treelang::to_json(value);
    auto back = treelang::parse<T>(json);
    if (back.is_err())
        return std::nullopt;
    return back.unwrap();
}

static GameSave make_save()
{
    GameSave save;
    save.floor = 3;
    save.position_row = 2;
    save.position_col = 1;

    save.player.level = 4;
    save.player.attribute_points = 2;
    save.player.gold = 120;
    save.player.status.hp = 50;
    save.player.status.mana = 30;
    save.player.status.san = 70;
    save.player.status.shield = 5;
    save.player.special.strength = 8;
    save.player.special.luck = 3;

    Spell s;
    s.id = "s1";
    s.name = "火球";
    s.elements = {ElementAttr{Element::Fire, 2}};
    s.cooldown = 2;
    s.base_multiplier = 100;
    save.player.spells.add(s);

    Item potion;
    potion.name = "恢复药水";
    potion.kind = ItemKind::Consumable;
    potion.description = "恢复血量";
    potion.effect.heal_hp = 15;
    save.player.inventory.push_back(potion);

    save.grid[kCenterRow][kCenterCol].type = RoomType::Start;

    Enemy e;
    e.name = "哥布林";
    e.hp = 30;
    e.max_hp = 30;
    e.san = 40;
    e.power = 5;
    e.weaknesses = {Element::Fire};
    save.grid[0][0].type = RoomType::Elite;
    save.grid[0][0].content.enemies.push_back(e);
    return save;
}

TEST_CASE("save: element attr round trip")
{
    auto r = roundtrip(ElementAttr{Element::Water, 3});
    REQUIRE(r.has_value());
    CHECK(r->element == Element::Water);
    CHECK(r->level == 3);
}

TEST_CASE("save: special attributes round trip")
{
    treelang::SpecialAttributes s;
    s.strength = 1;
    s.perception = 2;
    s.endurance = 3;
    s.charisma = 4;
    s.intelligence = 5;
    s.agility = 6;
    s.luck = 7;
    auto r = roundtrip(s);
    REQUIRE(r.has_value());
    CHECK(r->agility == 6);
    CHECK(r->luck == 7);
}

TEST_CASE("save: vital status round trip")
{
    treelang::VitalStatus v;
    v.hp = 10;
    v.mana = 20;
    v.san = 30;
    v.shield = 40;
    auto r = roundtrip(v);
    REQUIRE(r.has_value());
    CHECK(r->hp == 10);
    CHECK(r->shield == 40);
}

TEST_CASE("save: item round trip")
{
    Item item;
    item.name = "咒力药剂";
    item.kind = ItemKind::Tactical;
    item.description = "回复咒力";
    item.effect.heal_mana = 25;
    auto r = roundtrip(item);
    REQUIRE(r.has_value());
    CHECK(r->name == "咒力药剂");
    CHECK(r->kind == ItemKind::Tactical);
    CHECK(r->effect.heal_mana == 25);
    CHECK(r->effect.heal_hp == 0);
}

TEST_CASE("save: spell round trip")
{
    Spell s;
    s.id = "s1";
    s.name = "烈焰冲击";
    s.elements = {ElementAttr{Element::Fire, 2}, ElementAttr{Element::Metal, 1}};
    s.cooldown = 3;
    s.base_multiplier = 150;
    auto r = roundtrip(s);
    REQUIRE(r.has_value());
    CHECK(r->id == "s1");
    CHECK(r->name == "烈焰冲击");
    CHECK(r->cooldown == 3);
    CHECK(r->base_multiplier == 150);
    REQUIRE(r->elements.size() == 2);
    CHECK(r->elements[1].element == Element::Metal);
    CHECK(r->elements[1].level == 1);
}

TEST_CASE("save: spellbook round trip")
{
    SpellBook book;
    Spell a;
    a.id = "a";
    a.name = "火球";
    a.elements = {ElementAttr{Element::Fire, 2}};
    book.add(a);
    Spell b;
    b.id = "b";
    b.name = "水刃";
    b.elements = {ElementAttr{Element::Water, 1}, ElementAttr{Element::Metal, 1}};
    book.add(b);

    auto r = roundtrip(book);
    REQUIRE(r.has_value());
    CHECK(r->size() == 2);
    CHECK(r->find("b") != nullptr);
    CHECK(r->find("b")->elements.size() == 2);
    CHECK(r->find("missing") == nullptr);
}

TEST_CASE("save: enemy round trip")
{
    Enemy e;
    e.name = "精英·哥布林王";
    e.hp = 45;
    e.max_hp = 45;
    e.san = 55;
    e.power = 8;
    e.weaknesses = {Element::Water, Element::Metal};
    auto r = roundtrip(e);
    REQUIRE(r.has_value());
    CHECK(r->name == "精英·哥布林王");
    CHECK(r->max_hp == 45);
    CHECK(r->power == 8);
    REQUIRE(r->weaknesses.size() == 2);
    CHECK(r->weaknesses[1] == Element::Metal);
}

TEST_CASE("save: npc and room round trip")
{
    Npc n;
    n.name = "旅商";
    n.san = 60;
    auto rn = roundtrip(n);
    REQUIRE(rn.has_value());
    CHECK(rn->name == "旅商");

    Room room;
    room.type = RoomType::Story;
    room.content.npcs.push_back(n);
    room.content.gold_reward = 99;
    auto rr = roundtrip(room);
    REQUIRE(rr.has_value());
    CHECK(rr->type == RoomType::Story);
    CHECK(rr->content.gold_reward == 99);
    REQUIRE(rr->content.npcs.size() == 1);
    CHECK(rr->content.npcs[0].san == 60);
}

TEST_CASE("save: map grid round trip")
{
    MapGrid grid;
    grid[kCenterRow][kCenterCol].type = RoomType::Start;
    grid[0][0].type = RoomType::Reward;
    grid[0][0].content.gold_reward = 50;
    grid[4][4].type = RoomType::Function;

    auto r = roundtrip(grid);
    REQUIRE(r.has_value());
    CHECK((*r)[kCenterRow][kCenterCol].type == RoomType::Start);
    CHECK((*r)[0][0].type == RoomType::Reward);
    CHECK((*r)[0][0].content.gold_reward == 50);
    CHECK((*r)[4][4].type == RoomType::Function);
}

TEST_CASE("save: player round trip")
{
    auto save = make_save();
    auto r = roundtrip(save.player);
    REQUIRE(r.has_value());
    CHECK(r->level == 4);
    CHECK(r->gold == 120);
    CHECK(r->special.strength == 8);
    CHECK(r->status.san == 70);
    CHECK(r->spells.size() == 1);
    CHECK(r->spells.find("s1") != nullptr);
    REQUIRE(r->inventory.size() == 1);
    CHECK(r->inventory[0].effect.heal_hp == 15);
}

TEST_CASE("save: game save round trip and dump/parse")
{
    auto save = make_save();

    // 内存 round-trip
    auto r = roundtrip(save);
    REQUIRE(r.has_value());
    CHECK(r->floor == 3);
    CHECK(r->position_col == 1);
    CHECK((*r).grid[0][0].type == RoomType::Elite);
    REQUIRE((*r).grid[0][0].content.enemies.size() == 1);
    CHECK((*r).grid[0][0].content.enemies[0].name == "哥布林");

    // 序列化文本再解析（验证 JSON 格式）
    std::pmr::string text = pjh::json::dump(treelang::to_json(save));
    CHECK(text.find("\"version\":1") != std::string::npos);
    auto doc = pjh::json::parse_copy(text);
    auto back = treelang::parse<GameSave>(doc.root());
    REQUIRE(back.is_ok());
    CHECK(back.unwrap().floor == 3);
    CHECK(back.unwrap().player.spells.size() == 1);
}

TEST_CASE("save: achievement unlocked set round trip")
{
    std::unordered_set<std::string> unlocked = {"combo_3", "floor_3"};
    auto json = treelang::to_json_unlocked(unlocked);
    auto back = treelang::from_json_unlocked(json);
    REQUIRE(back.is_ok());
    CHECK(back.unwrap() == unlocked);
}

TEST_CASE("save: parse errors")
{
    // 缺失字段
    pjh::json::Object missing;
    missing.insert("hp", 10);
    auto r1 = treelang::parse<treelang::VitalStatus>(pjh::json::Json(std::move(missing)));
    CHECK(r1.is_err());
    CHECK(r1.unwrap_err() == SaveError::MissingField);

    // 类型不匹配（hp 为字符串）
    pjh::json::Object bad;
    bad.insert("hp", "not_int");
    bad.insert("mana", 1);
    bad.insert("san", 1);
    bad.insert("shield", 1);
    auto r2 = treelang::parse<treelang::VitalStatus>(pjh::json::Json(std::move(bad)));
    CHECK(r2.is_err());
    CHECK(r2.unwrap_err() == SaveError::TypeMismatch);

    // 未知元素标识
    pjh::json::Object attr;
    attr.insert("element", "earth");
    attr.insert("level", 1);
    auto r3 = treelang::parse<treelang::ElementAttr>(pjh::json::Json(std::move(attr)));
    CHECK(r3.is_err());
    CHECK(r3.unwrap_err() == SaveError::InvalidValue);

    // 版本不匹配
    pjh::json::Object gs;
    gs.insert("version", 99);
    gs.insert("floor", 1);
    gs.insert("position_row", 2);
    gs.insert("position_col", 2);
    gs.insert("player", treelang::to_json(Player{}));
    gs.insert("grid", treelang::to_json(MapGrid{}));
    auto r4 = treelang::parse<GameSave>(pjh::json::Json(std::move(gs)));
    CHECK(r4.is_err());
    CHECK(r4.unwrap_err() == SaveError::InvalidValue);

    // 网格尺寸错误
    pjh::json::Array rows;
    auto r5 = treelang::parse<MapGrid>(pjh::json::Json(std::move(rows)));
    CHECK(r5.is_err());
    CHECK(r5.unwrap_err() == SaveError::InvalidGrid);
}

TEST_CASE("save: manager writes and reads files")
{
    fs::path dir = fs::temp_directory_path() / "treelang_save_test";
    std::error_code ec;
    fs::remove_all(dir, ec);

    SaveManager mgr(dir.string());

    GameSave save;
    save.floor = 2;
    save.player.level = 3;
    save.player.status.hp = 42;

    REQUIRE(mgr.save_game(save).is_ok());
    auto rd = mgr.load_game();
    REQUIRE(rd.is_ok());
    CHECK(rd.unwrap().floor == 2);
    CHECK(rd.unwrap().player.level == 3);
    CHECK(rd.unwrap().player.status.hp == 42);

    std::unordered_set<std::string> unlocked = {"combo_3", "floor_3"};
    REQUIRE(mgr.save_achievements(unlocked).is_ok());
    auto rd_ach = mgr.load_achievements();
    REQUIRE(rd_ach.is_ok());
    CHECK(rd_ach.unwrap() == unlocked);

    fs::remove_all(dir, ec);
}

TEST_CASE("save: load missing file returns io error")
{
    fs::path dir = fs::temp_directory_path() / "treelang_save_missing";
    std::error_code ec;
    fs::remove_all(dir, ec);

    SaveManager mgr(dir.string());
    auto rd = mgr.load_game();
    CHECK(rd.is_err());
    CHECK(rd.unwrap_err() == SaveError::IoFailed);
    fs::remove_all(dir, ec);
}