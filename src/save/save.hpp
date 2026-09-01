/**
 * @file save.hpp
 * @brief 游戏状态 JSON 序列化与存档管理。
 *
 * 本模块是唯一直接接触 pjh_json 的模块：为 model 的全部类型提供
 * to_json / from_json 序列化，并提供 SaveManager 负责存档文件读写。
 * 存档分两个文件：
 * - game.json        当前进度（玩家、楼层、位置、地图）；
 * - achievements.json 成就解锁集合（永久性，全存档通用）。
 */

#ifndef INCLUDE_TREELANG_SAVE_SAVE_HPP
#define INCLUDE_TREELANG_SAVE_SAVE_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include <pjh_json.hpp>
#include <pjh_result.hpp>

#include "core/matrix.hpp"
#include "core/types.hpp"
#include "model/entity.hpp"
#include "model/item.hpp"
#include "model/room.hpp"
#include "model/spell.hpp"

namespace treelang
{
    /** 存档格式版本号 */
    inline constexpr int kSaveVersion = 1;

    /**
     * @brief 存档错误。
     */
    enum class SaveError : std::uint8_t
    {
        JsonParseFailed, /**< JSON 解析失败 */
        TypeMismatch,    /**< 字段类型与期望不符 */
        MissingField,    /**< 缺少必需字段 */
        InvalidValue,    /**< 枚举/版本等取值无效 */
        InvalidGrid,     /**< 地图网格尺寸不合法 */
        IoFailed,        /**< 文件读写失败 */
    };

    template <typename T>
    using SaveResult = pjh::result::Result<T, SaveError>;

    // ------------------------------------------------------------------
    // 枚举标识
    // ------------------------------------------------------------------

    /**
     * @brief 房间类型的稳定标识。
     * @param type 房间类型。
     * @return ASCII 标识，如 "start"。
     */
    inline std::string_view room_type_id(RoomType type)
    {
        switch (type)
        {
            case RoomType::Start: return "start";
            case RoomType::Reward: return "reward";
            case RoomType::Function: return "function";
            case RoomType::Elite: return "elite";
            case RoomType::Story: return "story";
        }
        return {};
    }

    /**
     * @brief 从稳定标识解析房间类型。
     * @param id ASCII 标识。
     * @return 解析成功返回对应类型；未知标识返回 nullopt。
     */
    inline std::optional<RoomType> room_type_from_id(std::string_view id)
    {
        if (id == "start") return RoomType::Start;
        if (id == "reward") return RoomType::Reward;
        if (id == "function") return RoomType::Function;
        if (id == "elite") return RoomType::Elite;
        if (id == "story") return RoomType::Story;
        return std::nullopt;
    }

    /**
     * @brief 道具类型的稳定标识。
     * @param kind 道具类型。
     * @return ASCII 标识，如 "consumable"。
     */
    inline std::string_view item_kind_id(ItemKind kind)
    {
        switch (kind)
        {
            case ItemKind::Consumable: return "consumable";
            case ItemKind::Tactical: return "tactical";
        }
        return {};
    }

    /**
     * @brief 从稳定标识解析道具类型。
     * @param id ASCII 标识。
     * @return 解析成功返回对应类型；未知标识返回 nullopt。
     */
    inline std::optional<ItemKind> item_kind_from_id(std::string_view id)
    {
        if (id == "consumable") return ItemKind::Consumable;
        if (id == "tactical") return ItemKind::Tactical;
        return std::nullopt;
    }

    namespace detail
    {
        /**
         * @brief 构造持有所有权的 JSON 字符串（防止借用悬垂）。
         * @param s 源字符串。
         * @return 拥有独立存储的字符串 Json。
         */
        inline pjh::json::Json json_string(std::string_view s)
        {
            pjh::json::String str(s);
            str.own();
            return pjh::json::Json(std::move(str));
        }

        /**
         * @brief 读取对象字段（须为对象；键须为字符串字面量以保证生命周期）。
         * @param obj 对象 Json。
         * @param key 字段名。
         * @return 指向字段值的指针；非对象或缺失返回错误。
         */
        inline SaveResult<const pjh::json::Json *> field(
            const pjh::json::Json &obj, std::string_view key)
        {
            if (!obj.is_object()) return SaveResult<const pjh::json::Json *>::Err(SaveError::TypeMismatch);
            try
            {
                return SaveResult<const pjh::json::Json *>::Ok(&obj.at(key));
            }
            catch (const std::out_of_range &)
            {
                return SaveResult<const pjh::json::Json *>::Err(SaveError::MissingField);
            }
        }

        /** @brief 读取整数字段。 */
        inline SaveResult<int> read_int(const pjh::json::Json &obj, std::string_view key)
        {
            auto f = field(obj, key);
            if (f.is_err()) return SaveResult<int>::Err(f.unwrap_err());
            auto v = f.unwrap()->try_as_int();
            if (!v) return SaveResult<int>::Err(SaveError::TypeMismatch);
            return SaveResult<int>::Ok(static_cast<int>(*v));
        }

        /** @brief 读取字符串字段。 */
        inline SaveResult<std::string> read_string(const pjh::json::Json &obj, std::string_view key)
        {
            auto f = field(obj, key);
            if (f.is_err()) return SaveResult<std::string>::Err(f.unwrap_err());
            auto v = f.unwrap()->try_as_string();
            if (!v) return SaveResult<std::string>::Err(SaveError::TypeMismatch);
            return SaveResult<std::string>::Ok(std::string(*v));
        }

        /** @brief 读取数组字段；返回指向数组 Json 的指针。 */
        inline SaveResult<const pjh::json::Json *> read_array(
            const pjh::json::Json &obj, std::string_view key)
        {
            auto f = field(obj, key);
            if (f.is_err()) return SaveResult<const pjh::json::Json *>::Err(f.unwrap_err());
            if (!f.unwrap()->is_array())
                return SaveResult<const pjh::json::Json *>::Err(SaveError::TypeMismatch);
            return SaveResult<const pjh::json::Json *>::Ok(f.unwrap());
        }

        /** @brief 从 Json 值解析元素。 */
        inline SaveResult<Element> read_element(const pjh::json::Json &j)
        {
            auto sv = j.try_as_string();
            if (!sv) return SaveResult<Element>::Err(SaveError::TypeMismatch);
            auto e = element_from_id(*sv);
            if (!e) return SaveResult<Element>::Err(SaveError::InvalidValue);
            return SaveResult<Element>::Ok(*e);
        }

        /** @brief 从 Json 值解析房间类型。 */
        inline SaveResult<RoomType> read_room_type(const pjh::json::Json &j)
        {
            auto sv = j.try_as_string();
            if (!sv) return SaveResult<RoomType>::Err(SaveError::TypeMismatch);
            auto t = room_type_from_id(*sv);
            if (!t) return SaveResult<RoomType>::Err(SaveError::InvalidValue);
            return SaveResult<RoomType>::Ok(*t);
        }

        /** @brief 从 Json 值解析道具类型。 */
        inline SaveResult<ItemKind> read_item_kind(const pjh::json::Json &j)
        {
            auto sv = j.try_as_string();
            if (!sv) return SaveResult<ItemKind>::Err(SaveError::TypeMismatch);
            auto k = item_kind_from_id(*sv);
            if (!k) return SaveResult<ItemKind>::Err(SaveError::InvalidValue);
            return SaveResult<ItemKind>::Ok(*k);
        }
    }

    // ------------------------------------------------------------------
    // model 类型序列化
    // ------------------------------------------------------------------

    /**
     * @brief 类型安全的反序列化入口。
     *
     * 各类型的解析函数命名不同（如 parse_player），统一经 parse<T>()
     * 按目标类型分派，避免直接调用时依赖返回类型推导。
     */
    template <typename T>
    SaveResult<T> parse(const pjh::json::Json &json);

    /** @brief 序列化元素属性条目。 */
    inline pjh::json::Json to_json(const ElementAttr &attr)
    {
        pjh::json::Object obj;
        obj.insert("element", detail::json_string(element_id(attr.element)));
        obj.insert("level", attr.level);
        return pjh::json::Json(std::move(obj));
    }

    /** @brief 反序列化元素属性条目。 */
    inline SaveResult<ElementAttr> parse_element_attr(const pjh::json::Json &json)
    {
        if (!json.is_object()) return SaveResult<ElementAttr>::Err(SaveError::TypeMismatch);
        auto e = detail::read_element(json.at("element"));
        if (e.is_err()) return SaveResult<ElementAttr>::Err(e.unwrap_err());
        auto level = detail::read_int(json, "level");
        if (level.is_err()) return SaveResult<ElementAttr>::Err(level.unwrap_err());
        return SaveResult<ElementAttr>::Ok(ElementAttr{e.unwrap(), level.unwrap()});
    }

    /** @brief 序列化 S.P.E.C.I.A.L. 属性。 */
    inline pjh::json::Json to_json(const SpecialAttributes &s)
    {
        pjh::json::Object obj;
        obj.insert("strength", s.strength);
        obj.insert("perception", s.perception);
        obj.insert("endurance", s.endurance);
        obj.insert("charisma", s.charisma);
        obj.insert("intelligence", s.intelligence);
        obj.insert("agility", s.agility);
        obj.insert("luck", s.luck);
        return pjh::json::Json(std::move(obj));
    }

    /** @brief 反序列化 S.P.E.C.I.A.L. 属性。 */
    inline SaveResult<SpecialAttributes> parse_special_attributes(const pjh::json::Json &json)
    {
        if (!json.is_object()) return SaveResult<SpecialAttributes>::Err(SaveError::TypeMismatch);
        SpecialAttributes s;
        auto a = detail::read_int(json, "strength");
        if (a.is_err()) return SaveResult<SpecialAttributes>::Err(a.unwrap_err());
        s.strength = a.unwrap();
        a = detail::read_int(json, "perception");
        if (a.is_err()) return SaveResult<SpecialAttributes>::Err(a.unwrap_err());
        s.perception = a.unwrap();
        a = detail::read_int(json, "endurance");
        if (a.is_err()) return SaveResult<SpecialAttributes>::Err(a.unwrap_err());
        s.endurance = a.unwrap();
        a = detail::read_int(json, "charisma");
        if (a.is_err()) return SaveResult<SpecialAttributes>::Err(a.unwrap_err());
        s.charisma = a.unwrap();
        a = detail::read_int(json, "intelligence");
        if (a.is_err()) return SaveResult<SpecialAttributes>::Err(a.unwrap_err());
        s.intelligence = a.unwrap();
        a = detail::read_int(json, "agility");
        if (a.is_err()) return SaveResult<SpecialAttributes>::Err(a.unwrap_err());
        s.agility = a.unwrap();
        a = detail::read_int(json, "luck");
        if (a.is_err()) return SaveResult<SpecialAttributes>::Err(a.unwrap_err());
        s.luck = a.unwrap();
        return SaveResult<SpecialAttributes>::Ok(s);
    }

    /** @brief 序列化生存状态。 */
    inline pjh::json::Json to_json(const VitalStatus &v)
    {
        pjh::json::Object obj;
        obj.insert("hp", v.hp);
        obj.insert("mana", v.mana);
        obj.insert("san", v.san);
        obj.insert("shield", v.shield);
        return pjh::json::Json(std::move(obj));
    }

    /** @brief 反序列化生存状态。 */
    inline SaveResult<VitalStatus> parse_vital_status(const pjh::json::Json &json)
    {
        if (!json.is_object()) return SaveResult<VitalStatus>::Err(SaveError::TypeMismatch);
        VitalStatus v;
        auto hp = detail::read_int(json, "hp");
        if (hp.is_err()) return SaveResult<VitalStatus>::Err(hp.unwrap_err());
        v.hp = hp.unwrap();
        auto mana = detail::read_int(json, "mana");
        if (mana.is_err()) return SaveResult<VitalStatus>::Err(mana.unwrap_err());
        v.mana = mana.unwrap();
        auto san = detail::read_int(json, "san");
        if (san.is_err()) return SaveResult<VitalStatus>::Err(san.unwrap_err());
        v.san = san.unwrap();
        auto shield = detail::read_int(json, "shield");
        if (shield.is_err()) return SaveResult<VitalStatus>::Err(shield.unwrap_err());
        v.shield = shield.unwrap();
        return SaveResult<VitalStatus>::Ok(v);
    }

    /** @brief 序列化道具效果。 */
    inline pjh::json::Json to_json(const ItemEffect &e)
    {
        pjh::json::Object obj;
        obj.insert("heal_hp", e.heal_hp);
        obj.insert("heal_mana", e.heal_mana);
        return pjh::json::Json(std::move(obj));
    }

    /** @brief 反序列化道具效果。 */
    inline SaveResult<ItemEffect> parse_item_effect(const pjh::json::Json &json)
    {
        if (!json.is_object()) return SaveResult<ItemEffect>::Err(SaveError::TypeMismatch);
        ItemEffect e;
        auto hp = detail::read_int(json, "heal_hp");
        if (hp.is_err()) return SaveResult<ItemEffect>::Err(hp.unwrap_err());
        e.heal_hp = hp.unwrap();
        auto mana = detail::read_int(json, "heal_mana");
        if (mana.is_err()) return SaveResult<ItemEffect>::Err(mana.unwrap_err());
        e.heal_mana = mana.unwrap();
        return SaveResult<ItemEffect>::Ok(e);
    }

    /** @brief 序列化道具。 */
    inline pjh::json::Json to_json(const Item &item)
    {
        pjh::json::Object obj;
        obj.insert("name", detail::json_string(item.name));
        obj.insert("kind", detail::json_string(item_kind_id(item.kind)));
        obj.insert("description", detail::json_string(item.description));
        obj.insert("effect", to_json(item.effect));
        return pjh::json::Json(std::move(obj));
    }

    /** @brief 反序列化道具。 */
    inline SaveResult<Item> parse_item(const pjh::json::Json &json)
    {
        if (!json.is_object()) return SaveResult<Item>::Err(SaveError::TypeMismatch);
        Item item;
        auto name = detail::read_string(json, "name");
        if (name.is_err()) return SaveResult<Item>::Err(name.unwrap_err());
        item.name = name.unwrap();
        auto kind = detail::read_item_kind(json.at("kind"));
        if (kind.is_err()) return SaveResult<Item>::Err(kind.unwrap_err());
        item.kind = kind.unwrap();
        auto desc = detail::read_string(json, "description");
        if (desc.is_err()) return SaveResult<Item>::Err(desc.unwrap_err());
        item.description = desc.unwrap();
        auto f = detail::field(json, "effect");
        if (f.is_err()) return SaveResult<Item>::Err(f.unwrap_err());
        auto effect = parse<ItemEffect>(*f.unwrap());
        if (effect.is_err()) return SaveResult<Item>::Err(effect.unwrap_err());
        item.effect = effect.unwrap();
        return SaveResult<Item>::Ok(item);
    }

    /** @brief 序列化术式。 */
    inline pjh::json::Json to_json(const Spell &spell)
    {
        pjh::json::Object obj;
        obj.insert("id", detail::json_string(spell.id));
        obj.insert("name", detail::json_string(spell.name));
        pjh::json::Array elems;
        for (const ElementAttr &attr : spell.elements) elems.push_back(to_json(attr));
        obj.insert("elements", pjh::json::Json(std::move(elems)));
        obj.insert("cooldown", spell.cooldown);
        obj.insert("base_multiplier", spell.base_multiplier);
        return pjh::json::Json(std::move(obj));
    }

    /** @brief 反序列化术式。 */
    inline SaveResult<Spell> parse_spell(const pjh::json::Json &json)
    {
        if (!json.is_object()) return SaveResult<Spell>::Err(SaveError::TypeMismatch);
        Spell spell;
        auto id = detail::read_string(json, "id");
        if (id.is_err()) return SaveResult<Spell>::Err(id.unwrap_err());
        spell.id = id.unwrap();
        auto name = detail::read_string(json, "name");
        if (name.is_err()) return SaveResult<Spell>::Err(name.unwrap_err());
        spell.name = name.unwrap();
        auto ef = detail::read_array(json, "elements");
        if (ef.is_err()) return SaveResult<Spell>::Err(ef.unwrap_err());
        const pjh::json::Json *elems = ef.unwrap();
        for (std::size_t i = 0; i < elems->size(); ++i)
        {
            auto attr = parse<ElementAttr>(elems->at(i));
            if (attr.is_err()) return SaveResult<Spell>::Err(attr.unwrap_err());
            spell.elements.push_back(attr.unwrap());
        }
        auto cd = detail::read_int(json, "cooldown");
        if (cd.is_err()) return SaveResult<Spell>::Err(cd.unwrap_err());
        spell.cooldown = cd.unwrap();
        auto bm = detail::read_int(json, "base_multiplier");
        if (bm.is_err()) return SaveResult<Spell>::Err(bm.unwrap_err());
        spell.base_multiplier = bm.unwrap();
        return SaveResult<Spell>::Ok(spell);
    }

    /** @brief 序列化术式列表（数组）。 */
    inline pjh::json::Json to_json(const SpellBook &book)
    {
        pjh::json::Array arr;
        for (const Spell &spell : book) arr.push_back(to_json(spell));
        return pjh::json::Json(std::move(arr));
    }

    /** @brief 反序列化术式列表。 */
    inline SaveResult<SpellBook> parse_spellbook(const pjh::json::Json &json)
    {
        if (!json.is_array()) return SaveResult<SpellBook>::Err(SaveError::TypeMismatch);
        SpellBook book;
        for (std::size_t i = 0; i < json.size(); ++i)
        {
            auto spell = parse<Spell>(json.at(i));
            if (spell.is_err()) return SaveResult<SpellBook>::Err(spell.unwrap_err());
            book.add(spell.unwrap());
        }
        return SaveResult<SpellBook>::Ok(book);
    }

    /** @brief 序列化敌人。 */
    inline pjh::json::Json to_json(const Enemy &enemy)
    {
        pjh::json::Object obj;
        obj.insert("name", detail::json_string(enemy.name));
        obj.insert("hp", enemy.hp);
        obj.insert("max_hp", enemy.max_hp);
        obj.insert("san", enemy.san);
        obj.insert("power", enemy.power);
        pjh::json::Array weak;
        for (Element e : enemy.weaknesses) weak.push_back(detail::json_string(element_id(e)));
        obj.insert("weaknesses", pjh::json::Json(std::move(weak)));
        return pjh::json::Json(std::move(obj));
    }

    /** @brief 反序列化敌人。 */
    inline SaveResult<Enemy> parse_enemy(const pjh::json::Json &json)
    {
        if (!json.is_object()) return SaveResult<Enemy>::Err(SaveError::TypeMismatch);
        Enemy enemy;
        auto name = detail::read_string(json, "name");
        if (name.is_err()) return SaveResult<Enemy>::Err(name.unwrap_err());
        enemy.name = name.unwrap();
        auto hp = detail::read_int(json, "hp");
        if (hp.is_err()) return SaveResult<Enemy>::Err(hp.unwrap_err());
        enemy.hp = hp.unwrap();
        auto max_hp = detail::read_int(json, "max_hp");
        if (max_hp.is_err()) return SaveResult<Enemy>::Err(max_hp.unwrap_err());
        enemy.max_hp = max_hp.unwrap();
        auto san = detail::read_int(json, "san");
        if (san.is_err()) return SaveResult<Enemy>::Err(san.unwrap_err());
        enemy.san = san.unwrap();
        auto power = detail::read_int(json, "power");
        if (power.is_err()) return SaveResult<Enemy>::Err(power.unwrap_err());
        enemy.power = power.unwrap();
        auto wf = detail::read_array(json, "weaknesses");
        if (wf.is_err()) return SaveResult<Enemy>::Err(wf.unwrap_err());
        const pjh::json::Json *weak = wf.unwrap();
        for (std::size_t i = 0; i < weak->size(); ++i)
        {
            auto e = detail::read_element(weak->at(i));
            if (e.is_err()) return SaveResult<Enemy>::Err(e.unwrap_err());
            enemy.weaknesses.push_back(e.unwrap());
        }
        return SaveResult<Enemy>::Ok(enemy);
    }

    /** @brief 序列化 NPC。 */
    inline pjh::json::Json to_json(const Npc &npc)
    {
        pjh::json::Object obj;
        obj.insert("name", detail::json_string(npc.name));
        obj.insert("san", npc.san);
        return pjh::json::Json(std::move(obj));
    }

    /** @brief 反序列化 NPC。 */
    inline SaveResult<Npc> parse_npc(const pjh::json::Json &json)
    {
        if (!json.is_object()) return SaveResult<Npc>::Err(SaveError::TypeMismatch);
        Npc npc;
        auto name = detail::read_string(json, "name");
        if (name.is_err()) return SaveResult<Npc>::Err(name.unwrap_err());
        npc.name = name.unwrap();
        auto san = detail::read_int(json, "san");
        if (san.is_err()) return SaveResult<Npc>::Err(san.unwrap_err());
        npc.san = san.unwrap();
        return SaveResult<Npc>::Ok(npc);
    }

    /** @brief 序列化房间内容。 */
    inline pjh::json::Json to_json(const RoomContent &content)
    {
        pjh::json::Object obj;
        pjh::json::Array enemies;
        for (const Enemy &e : content.enemies) enemies.push_back(to_json(e));
        obj.insert("enemies", pjh::json::Json(std::move(enemies)));
        pjh::json::Array npcs;
        for (const Npc &n : content.npcs) npcs.push_back(to_json(n));
        obj.insert("npcs", pjh::json::Json(std::move(npcs)));
        pjh::json::Array loot;
        for (const Item &i : content.loot) loot.push_back(to_json(i));
        obj.insert("loot", pjh::json::Json(std::move(loot)));
        pjh::json::Array spells;
        for (const Spell &s : content.spell_rewards) spells.push_back(to_json(s));
        obj.insert("spell_rewards", pjh::json::Json(std::move(spells)));
        obj.insert("gold_reward", content.gold_reward);
        return pjh::json::Json(std::move(obj));
    }

    /** @brief 反序列化房间内容。 */
    inline SaveResult<RoomContent> parse_room_content(const pjh::json::Json &json)
    {
        if (!json.is_object()) return SaveResult<RoomContent>::Err(SaveError::TypeMismatch);
        RoomContent content;
        auto ef = detail::read_array(json, "enemies");
        if (ef.is_err()) return SaveResult<RoomContent>::Err(ef.unwrap_err());
        {
            const pjh::json::Json *arr = ef.unwrap();
            for (std::size_t i = 0; i < arr->size(); ++i)
            {
                auto v = parse<Enemy>(arr->at(i));
                if (v.is_err()) return SaveResult<RoomContent>::Err(v.unwrap_err());
                content.enemies.push_back(v.unwrap());
            }
        }
        auto nf = detail::read_array(json, "npcs");
        if (nf.is_err()) return SaveResult<RoomContent>::Err(nf.unwrap_err());
        {
            const pjh::json::Json *arr = nf.unwrap();
            for (std::size_t i = 0; i < arr->size(); ++i)
            {
                auto v = parse<Npc>(arr->at(i));
                if (v.is_err()) return SaveResult<RoomContent>::Err(v.unwrap_err());
                content.npcs.push_back(v.unwrap());
            }
        }
        auto lf = detail::read_array(json, "loot");
        if (lf.is_err()) return SaveResult<RoomContent>::Err(lf.unwrap_err());
        {
            const pjh::json::Json *arr = lf.unwrap();
            for (std::size_t i = 0; i < arr->size(); ++i)
            {
                auto v = parse<Item>(arr->at(i));
                if (v.is_err()) return SaveResult<RoomContent>::Err(v.unwrap_err());
                content.loot.push_back(v.unwrap());
            }
        }
        auto sf = detail::read_array(json, "spell_rewards");
        if (sf.is_err()) return SaveResult<RoomContent>::Err(sf.unwrap_err());
        {
            const pjh::json::Json *arr = sf.unwrap();
            for (std::size_t i = 0; i < arr->size(); ++i)
            {
                auto v = parse<Spell>(arr->at(i));
                if (v.is_err()) return SaveResult<RoomContent>::Err(v.unwrap_err());
                content.spell_rewards.push_back(v.unwrap());
            }
        }
        auto gold = detail::read_int(json, "gold_reward");
        if (gold.is_err()) return SaveResult<RoomContent>::Err(gold.unwrap_err());
        content.gold_reward = gold.unwrap();
        return SaveResult<RoomContent>::Ok(content);
    }

    /** @brief 序列化房间。 */
    inline pjh::json::Json to_json(const Room &room)
    {
        pjh::json::Object obj;
        obj.insert("type", detail::json_string(room_type_id(room.type)));
        obj.insert("content", to_json(room.content));
        return pjh::json::Json(std::move(obj));
    }

    /** @brief 反序列化房间。 */
    inline SaveResult<Room> parse_room(const pjh::json::Json &json)
    {
        if (!json.is_object()) return SaveResult<Room>::Err(SaveError::TypeMismatch);
        Room room;
        auto type = detail::read_room_type(json.at("type"));
        if (type.is_err()) return SaveResult<Room>::Err(type.unwrap_err());
        room.type = type.unwrap();
        auto f = detail::field(json, "content");
        if (f.is_err()) return SaveResult<Room>::Err(f.unwrap_err());
        auto content = parse<RoomContent>(*f.unwrap());
        if (content.is_err()) return SaveResult<Room>::Err(content.unwrap_err());
        room.content = content.unwrap();
        return SaveResult<Room>::Ok(room);
    }

    /** @brief 序列化 5×5 地图网格（二维数组）。 */
    inline pjh::json::Json to_json(const MapGrid &grid)
    {
        pjh::json::Array rows;
        for (std::size_t r = 0; r < kMapSize; ++r)
        {
            pjh::json::Array row;
            for (std::size_t c = 0; c < kMapSize; ++c) row.push_back(to_json(grid[r][c]));
            rows.push_back(pjh::json::Json(std::move(row)));
        }
        return pjh::json::Json(std::move(rows));
    }

    /** @brief 反序列化 5×5 地图网格。 */
    inline SaveResult<MapGrid> parse_map_grid(const pjh::json::Json &json)
    {
        if (!json.is_array()) return SaveResult<MapGrid>::Err(SaveError::TypeMismatch);
        if (json.size() != kMapSize) return SaveResult<MapGrid>::Err(SaveError::InvalidGrid);
        MapGrid grid;
        for (std::size_t r = 0; r < kMapSize; ++r)
        {
            const pjh::json::Json &row = json.at(r);
            if (!row.is_array() || row.size() != kMapSize)
                return SaveResult<MapGrid>::Err(SaveError::InvalidGrid);
            for (std::size_t c = 0; c < kMapSize; ++c)
            {
                auto room = parse<Room>(row.at(c));
                if (room.is_err()) return SaveResult<MapGrid>::Err(room.unwrap_err());
                grid[r][c] = room.unwrap();
            }
        }
        return SaveResult<MapGrid>::Ok(grid);
    }

    /** @brief 序列化玩家。 */
    inline pjh::json::Json to_json(const Player &player)
    {
        pjh::json::Object obj;
        obj.insert("status", to_json(player.status));
        obj.insert("special", to_json(player.special));
        obj.insert("level", player.level);
        obj.insert("attribute_points", player.attribute_points);
        obj.insert("gold", player.gold);
        obj.insert("spells", to_json(player.spells));
        pjh::json::Array inv;
        for (const Item &item : player.inventory) inv.push_back(to_json(item));
        obj.insert("inventory", pjh::json::Json(std::move(inv)));
        return pjh::json::Json(std::move(obj));
    }

    /** @brief 反序列化玩家。 */
    inline SaveResult<Player> parse_player(const pjh::json::Json &json)
    {
        if (!json.is_object()) return SaveResult<Player>::Err(SaveError::TypeMismatch);
        Player player;
        auto f = detail::field(json, "status");
        if (f.is_err()) return SaveResult<Player>::Err(f.unwrap_err());
        auto status = parse<VitalStatus>(*f.unwrap());
        if (status.is_err()) return SaveResult<Player>::Err(status.unwrap_err());
        player.status = status.unwrap();
        f = detail::field(json, "special");
        if (f.is_err()) return SaveResult<Player>::Err(f.unwrap_err());
        auto special = parse<SpecialAttributes>(*f.unwrap());
        if (special.is_err()) return SaveResult<Player>::Err(special.unwrap_err());
        player.special = special.unwrap();
        auto level = detail::read_int(json, "level");
        if (level.is_err()) return SaveResult<Player>::Err(level.unwrap_err());
        player.level = level.unwrap();
        auto points = detail::read_int(json, "attribute_points");
        if (points.is_err()) return SaveResult<Player>::Err(points.unwrap_err());
        player.attribute_points = points.unwrap();
        auto gold = detail::read_int(json, "gold");
        if (gold.is_err()) return SaveResult<Player>::Err(gold.unwrap_err());
        player.gold = gold.unwrap();
        auto sf = detail::read_array(json, "spells");
        if (sf.is_err()) return SaveResult<Player>::Err(sf.unwrap_err());
        {
            const pjh::json::Json *arr = sf.unwrap();
            for (std::size_t i = 0; i < arr->size(); ++i)
            {
                auto spell = parse<Spell>(arr->at(i));
                if (spell.is_err()) return SaveResult<Player>::Err(spell.unwrap_err());
                player.spells.add(spell.unwrap());
            }
        }
        auto iv = detail::read_array(json, "inventory");
        if (iv.is_err()) return SaveResult<Player>::Err(iv.unwrap_err());
        {
            const pjh::json::Json *arr = iv.unwrap();
            for (std::size_t i = 0; i < arr->size(); ++i)
            {
                auto item = parse<Item>(arr->at(i));
                if (item.is_err()) return SaveResult<Player>::Err(item.unwrap_err());
                player.inventory.push_back(item.unwrap());
            }
        }
        return SaveResult<Player>::Ok(player);
    }

    // ------------------------------------------------------------------
    // 存档封装
    // ------------------------------------------------------------------

    /**
     * @brief 一次可存档的游戏进度快照。
     */
    struct GameSave
    {
        Player player;                      /**< 玩家状态 */
        int floor = 1;                      /**< 当前楼层 */
        std::size_t position_row = kCenterRow; /**< 玩家所在行 */
        std::size_t position_col = kCenterCol; /**< 玩家所在列 */
        MapGrid grid;                       /**< 当前楼层地图 */
    };

    /** @brief 序列化存档快照。 */
    inline pjh::json::Json to_json(const GameSave &save)
    {
        pjh::json::Object obj;
        obj.insert("version", kSaveVersion);
        obj.insert("floor", save.floor);
        obj.insert("position_row", static_cast<std::int64_t>(save.position_row));
        obj.insert("position_col", static_cast<std::int64_t>(save.position_col));
        obj.insert("player", to_json(save.player));
        obj.insert("grid", to_json(save.grid));
        return pjh::json::Json(std::move(obj));
    }

    /** @brief 反序列化存档快照。 */
    inline SaveResult<GameSave> parse_game_save(const pjh::json::Json &json)
    {
        if (!json.is_object()) return SaveResult<GameSave>::Err(SaveError::TypeMismatch);
        auto version = detail::read_int(json, "version");
        if (version.is_err()) return SaveResult<GameSave>::Err(version.unwrap_err());
        if (version.unwrap() != kSaveVersion) return SaveResult<GameSave>::Err(SaveError::InvalidValue);

        GameSave save;
        auto floor = detail::read_int(json, "floor");
        if (floor.is_err()) return SaveResult<GameSave>::Err(floor.unwrap_err());
        save.floor = floor.unwrap();
        auto pr = detail::read_int(json, "position_row");
        if (pr.is_err()) return SaveResult<GameSave>::Err(pr.unwrap_err());
        save.position_row = static_cast<std::size_t>(pr.unwrap());
        auto pc = detail::read_int(json, "position_col");
        if (pc.is_err()) return SaveResult<GameSave>::Err(pc.unwrap_err());
        save.position_col = static_cast<std::size_t>(pc.unwrap());
        auto pf = detail::field(json, "player");
        if (pf.is_err()) return SaveResult<GameSave>::Err(pf.unwrap_err());
        auto player = parse<Player>(*pf.unwrap());
        if (player.is_err()) return SaveResult<GameSave>::Err(player.unwrap_err());
        save.player = player.unwrap();
        auto gf = detail::field(json, "grid");
        if (gf.is_err()) return SaveResult<GameSave>::Err(gf.unwrap_err());
        auto grid = parse<MapGrid>(*gf.unwrap());
        if (grid.is_err()) return SaveResult<GameSave>::Err(grid.unwrap_err());
        save.grid = grid.unwrap();
        return SaveResult<GameSave>::Ok(save);
    }

    /**
     * @brief parse<T>() 定义：按目标类型分派到对应的解析函数。
     *
     * 须置于全部解析函数之后，以保证名称完整可见。
     */
    template <typename T>
    inline SaveResult<T> parse(const pjh::json::Json &json)
    {
        if constexpr (std::is_same_v<T, ElementAttr>)
            return parse_element_attr(json);
        else if constexpr (std::is_same_v<T, SpecialAttributes>)
            return parse_special_attributes(json);
        else if constexpr (std::is_same_v<T, VitalStatus>)
            return parse_vital_status(json);
        else if constexpr (std::is_same_v<T, ItemEffect>)
            return parse_item_effect(json);
        else if constexpr (std::is_same_v<T, Item>)
            return parse_item(json);
        else if constexpr (std::is_same_v<T, Spell>)
            return parse_spell(json);
        else if constexpr (std::is_same_v<T, SpellBook>)
            return parse_spellbook(json);
        else if constexpr (std::is_same_v<T, Enemy>)
            return parse_enemy(json);
        else if constexpr (std::is_same_v<T, Npc>)
            return parse_npc(json);
        else if constexpr (std::is_same_v<T, RoomContent>)
            return parse_room_content(json);
        else if constexpr (std::is_same_v<T, Room>)
            return parse_room(json);
        else if constexpr (std::is_same_v<T, MapGrid>)
            return parse_map_grid(json);
        else if constexpr (std::is_same_v<T, Player>)
            return parse_player(json);
        else if constexpr (std::is_same_v<T, GameSave>)
            return parse_game_save(json);
        else
            static_assert(sizeof(T) == 0, "parse<T>: 不支持该类型");
    }

    /** @brief 序列化成就解锁集合。 */
    inline pjh::json::Json to_json_unlocked(const std::unordered_set<std::string> &unlocked)
    {
        pjh::json::Object obj;
        pjh::json::Array arr;
        for (const std::string &id : unlocked) arr.push_back(detail::json_string(id));
        obj.insert("unlocked", pjh::json::Json(std::move(arr)));
        return pjh::json::Json(std::move(obj));
    }

    /** @brief 反序列化成就解锁集合。 */
    inline SaveResult<std::unordered_set<std::string>> from_json_unlocked(
        const pjh::json::Json &json)
    {
        if (!json.is_object()) return SaveResult<std::unordered_set<std::string>>::Err(SaveError::TypeMismatch);
        auto f = detail::read_array(json, "unlocked");
        if (f.is_err()) return SaveResult<std::unordered_set<std::string>>::Err(f.unwrap_err());
        const pjh::json::Json *arr = f.unwrap();
        std::unordered_set<std::string> set;
        for (std::size_t i = 0; i < arr->size(); ++i)
        {
            auto s = arr->at(i).try_as_string();
            if (!s) return SaveResult<std::unordered_set<std::string>>::Err(SaveError::TypeMismatch);
            set.insert(std::string(*s));
        }
        return SaveResult<std::unordered_set<std::string>>::Ok(set);
    }

    /**
     * @class SaveManager
     * @brief 存档文件读写。
     *
     * game.json 保存当前进度；achievements.json 保存成就解锁集合
     * （永久性、全存档通用）。
     */
    class SaveManager
    {
    public:
        /**
         * @brief 构造存档管理器。
         * @param save_dir 存档目录（不存在时自动创建）。
         */
        explicit SaveManager(std::string save_dir = "save") : m_dir(std::move(save_dir)) {}

        /**
         * @brief 写入当前进度存档。
         * @param save 进度快照。
         * @return 写入失败返回 IoFailed。
         */
        SaveResult<void> save_game(const GameSave &save)
        {
            try
            {
                std::filesystem::create_directories(m_dir);
                pjh::json::dump_file(path("game.json"), to_json(save), {.pretty = true});
            }
            catch (const std::exception &)
            {
                return SaveResult<void>::Err(SaveError::IoFailed);
            }
            return SaveResult<void>::Ok();
        }

        /**
         * @brief 读取当前进度存档。
         * @return 进度快照；文件不存在或解析失败返回对应错误。
         */
        SaveResult<GameSave> load_game()
        {
            if (!std::filesystem::exists(path("game.json")))
                return SaveResult<GameSave>::Err(SaveError::IoFailed);
            try
            {
                pjh::json::Document doc = pjh::json::parse_file(path("game.json"));
                return parse<GameSave>(doc.root());
            }
            catch (const pjh::json::ParseError &)
            {
                return SaveResult<GameSave>::Err(SaveError::JsonParseFailed);
            }
            catch (const std::exception &)
            {
                return SaveResult<GameSave>::Err(SaveError::IoFailed);
            }
        }

        /**
         * @brief 写入成就解锁集合（全局，跨存档）。
         * @param unlocked 已解锁成就 id 集合。
         * @return 写入失败返回 IoFailed。
         */
        SaveResult<void> save_achievements(const std::unordered_set<std::string> &unlocked)
        {
            try
            {
                std::filesystem::create_directories(m_dir);
                pjh::json::dump_file(
                    path("achievements.json"), to_json_unlocked(unlocked), {.pretty = true});
            }
            catch (const std::exception &)
            {
                return SaveResult<void>::Err(SaveError::IoFailed);
            }
            return SaveResult<void>::Ok();
        }

        /**
         * @brief 读取成就解锁集合（全局，跨存档）。
         * @return 已解锁成就 id 集合；文件不存在或解析失败返回对应错误。
         */
        SaveResult<std::unordered_set<std::string>> load_achievements()
        {
            if (!std::filesystem::exists(path("achievements.json")))
                return SaveResult<std::unordered_set<std::string>>::Err(SaveError::IoFailed);
            try
            {
                pjh::json::Document doc = pjh::json::parse_file(path("achievements.json"));
                return from_json_unlocked(doc.root());
            }
            catch (const pjh::json::ParseError &)
            {
                return SaveResult<std::unordered_set<std::string>>::Err(SaveError::JsonParseFailed);
            }
            catch (const std::exception &)
            {
                return SaveResult<std::unordered_set<std::string>>::Err(SaveError::IoFailed);
            }
        }

    private:
        std::string path(const std::string &file) const { return m_dir + "/" + file; }

        std::string m_dir; /**< 存档目录 */
    };
}

#endif  // INCLUDE_TREELANG_SAVE_SAVE_HPP