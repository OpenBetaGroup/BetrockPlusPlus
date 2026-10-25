/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, jomner <github.com/jomner>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "gamerules.h"
#include "config/config.h"
#include "logger.h"
#include <charconv>
#include <map>
#include <string>
#include <vector>

namespace Gamerule {
namespace {

struct Registry {
    std::vector<Entry> rules;
    std::map<std::string, Id, std::less<>> byName;
    int nextCustom = static_cast<int>(Id::Max);

    void Register(Id _id, const std::string& _name, const std::string& _description, Type _value) {
        rules[static_cast<size_t>(_id)] = Entry{_name, _description, std::move(_value)};
        byName[_name] = _id;
    }

    Registry() {
        rules.resize(static_cast<size_t>(Id::Max));
        // Drops
        //Register(Id::BlockDrops, "block_drops", true);
        //Register(Id::EntityDrops, "entity_drops", true);
        //Register(Id::MobDrops, "mob_drops", true);
        // Misc
        //Register(Id::MaxMinecartSpeed, "max_minecart_speed", "The limit of how fast a minecart can move per tick", 8);
        Register(Id::TntExplodes, "tnt_explodes", "If TNT can explode", true);
        Register(Id::Nightmares, "nightmares", "If monsters can spawn near a sleeping player", true);
        Register(Id::NetherBedsExplode, "nether_beds_explode", "If beds should explode in the nether", true);
        Register(Id::SpawnProtectionRadius, "spawn_protection_radius", "How large the radius in which blocks can't be placed is", 16);
        // Mobs
        //Register(Id::ForgiveDeadPlayers, "forgive_dead_players", "If aggro'd mobs forgive a player after dying", true);
        Register(Id::MobGriefing, "mob_griefing", "If mobs can damage the environment (e.g. creeper explosions destroying blocks)", true);
        Register(Id::AccurateDespawnRules, "accurate_despawn_rules", "If squids, ghasts and slimes should never despawn", false);
        Register(Id::SpawnChunks, "spawn_chunks", "If spawn chunks should be and remain loaded (requires restart to take effect)", true);
        // Player
        Register(Id::AllowEnteringNetherUsingPortals, "allow_entering_nether_using_portals", "If players should be able to travel to the Nether via a Portal from the Overworld", true);
        Register(Id::DrowningDamage, "drowning_damage", "If players should take drowning damage", true);
        Register(Id::FallDamage, "fall_damage", "If players should take fall damage", true);
        Register(Id::FireDamage, "fire_damage", "If players should take fire damage", true);
        //Register(Id::ImmediateRespawn, "immediate_respawn", "If a player should be respawned immediately instead of getting the death screen first", false);
        Register(Id::KeepInventory, "keep_inventory", "If players should keep their inventory after dying instead of dropping it", false);
        Register(Id::PlayerMovementCheck, "player_movement_check", "If the server should do movement checks on players (anti-cheat)", true);
        //Register(Id::PlayersSleepingPercentage, "players_sleeping_percentage", "What percentage of players needs to be asleep for the server to skip to the next morning", 100);
        //Register(Id::Pvp, "pvp", "If players are able to damage one another", true);
        Register(Id::RespawnRadius, "respawn_radius", "In what radius from world spawn players are able to spawn at", 10);
        // Spawning
        Register(Id::SpawnMobs, "spawn_mobs", "If mobs should spawn (e.g. pigs, sheep, cows)", true);
        Register(Id::SpawnMonsters, "spawn_monsters", "If monsters should spawn (e.g. zombies, creepers)", true);
        // World Updates
        //Register(Id::AdvanceTime, "advance_time", "If the time of day should advance", true);
        Register(Id::AdvanceWeather, "advance_weather", "If the weather ticks should advance", true);
        Register(Id::FireSpread, "fire_spread", "If fire should spread", true);
        //Register(Id::LavaSourceConversion, "lava_source_conversion", "If lava should be able to form new source blocks", false);
        //Register(Id::MaxSnowAccumulationHeight, "max_snow_accumulation_height", "How much snow layers are allowed to accumulate", 3);
        //Register(Id::RandomTickSpeed, "random_tick_speed", 3);
        //Register(Id::WaterSourceConversion, "water_source_conversion", "If lava should be able to form new source blocks", true);
    }

    Entry* Find(Id _id) {
        const auto i = static_cast<size_t>(_id);
        if (i >= rules.size() || rules[i].name.empty())
            return nullptr;
        return &rules[i];
    }
};

// Function-local static avoids static-init-order problems across TUs
Registry& R() {
    static Registry r;
    return r;
}

} // namespace

// Access via ID
Entry* GetEntry(const Id _id) {
    return R().Find(_id);
}

Type GetRaw(const Id _id) {
    const Entry* e = R().Find(_id);
    return e ? e->value : Type{};
}

bool SetRaw(const Id _id, const Type& _value) {
    Entry* e = R().Find(_id);
    if (!e)
        return false;
    // Reject type changes
    if (e->value.index() != _value.index())
        return false;
    e->value = _value;
    return true;
}

// Access via string
Entry* GetEntry(const std::string& _name) {
    return GetEntry(ToId(_name));
}
Type GetRaw(const std::string& _name) {
    return GetRaw(ToId(_name));
}
bool SetRaw(const std::string& _name, const Type& _value) {
    return SetRaw(ToId(_name), _value);
}

bool Add(const std::string& _name, const std::string& _description, const Type& _value) {
    auto& r = R();
    if (r.byName.contains(_name))
        return false;
    const Id id = static_cast<Id>(r.rules.size());
    r.rules.push_back(Entry{_name, _description, _value});
    r.byName[_name] = id;
    return true;
}

bool Remove(const std::string& _name) {
    auto& r = R();
    auto it = r.byName.find(_name);
    if (it == r.byName.end())
        return false;
    r.rules[static_cast<size_t>(it->second)] = Entry{};  // tombstone
    r.byName.erase(it);
    return true;
}

std::string ToString(const Id _id) {
    const Entry* e = R().Find(_id);
    return e ? e->name : "";
}

Id ToId(const std::string& _name) {
    auto& r = R();
    auto it = r.byName.find(_name);
    return it == r.byName.end() ? Id::Invalid : it->second;
}

const std::vector<Entry>& GetAll() {
    return R().rules;
}

const std::map<std::string, Id, std::less<>>& GetAllNames() {
    return R().byName;
}

std::string ValueToString(const Type& _value) {
    return std::visit([](const auto& x) -> std::string {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, bool>)
            return x ? "true" : "false";
        else if constexpr (std::is_same_v<T, std::string>)
            return x;
        else
            return std::to_string(x);
    }, _value);
}

std::optional<Type> ParseLike(const Type& _current, std::string_view _text) {
    return std::visit([&](const auto& cur) -> std::optional<Type> {
        using T = std::decay_t<decltype(cur)>;
        if constexpr (std::is_same_v<T, bool>) {
            if (_text == "true")  return Type{true};
            if (_text == "false") return Type{false};
            return std::nullopt;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return Type{std::string(_text)};
        } else if constexpr (std::is_floating_point_v<T>) {
            // std::from_chars for floating point is unavailable on macOS < 26
            if (_text.empty() || std::isspace(static_cast<unsigned char>(_text.front())))
                return std::nullopt;
            const std::string str(_text);
            char* endPtr = nullptr;
            errno = 0;
            const double val = std::strtod(str.c_str(), &endPtr);
            if (endPtr != str.c_str() + str.size() || errno == ERANGE || !std::isfinite(val))
                return std::nullopt;
            return Type{static_cast<T>(val)};
        } else {
            T out{};
            const char* end = _text.data() + _text.size();
            auto [ptr, ec] = std::from_chars(_text.data(), end, out);
            if (ec != std::errc{} || ptr != end)
                return std::nullopt;
            return Type{out};
        }
    }, _current);
}

bool ReadFromConfig(Config& _config) {
    bool missing = false;
    for (Entry& e : R().rules) {
        if (e.name.empty())
            continue; // unregistered / tombstone
        if (!_config.Has(e.name)) {
            missing = true;
            continue;
        }
        std::string text = _config.GetAsString(e.name);
        // Config only trims the front of values, so drop stray trailing whitespace (e.g. CRLF files)
        text.erase(text.find_last_not_of(" \t\r\n\f\v") + 1);
        const auto parsed = ParseLike(e.value, text);
        if (!parsed) {
            Logger::Warning << "Invalid value '" << text << "' for gamerule '" << e.name
                            << "' in gamerules.txt, keeping " << ValueToString(e.value) << "\n";
            continue;
        }
        e.value = *parsed;
    }
    return missing;
}

void WriteToConfig(Config& _config) {
    for (const Entry& e : R().rules) {
        if (e.name.empty())
            continue;
        _config.Set(e.name, ValueToString(e.value));
    }
}

} // namespace Gamerule
