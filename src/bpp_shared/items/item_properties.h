/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "base_types.h"
#include "direction.h"
#include "items.h"
#include "logger.h"
#include "numeric_structs.h"
#include "packet_data.h"
#include <cstdint>
#include <unordered_map>

class WorldManager;
struct PlayerSession;
struct Entity;
class Server;

namespace Items {

struct ItemProperties {
	ItemAmount maxStack = STACK_MAX;
};

enum class ArmorPiece : int8_t {
	INVALID_PIECE = -1,
	HELMET,
	CHESTPLATE,
	LEGGING,
	BOOT
};

struct ItemBehavior {
	void (*onUse)(PlayerSession& _session, ItemStack* _stack, Entity& _target) = nullptr;
	void (*onBlockUse)(WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& _user,
	                   Cardinal::Value _face) = nullptr;
	void (*onStartHolding)(ItemStack* _stack, PlayerSession& _session) = nullptr;
	void (*whileHeld)(ItemStack* _stack, PlayerSession& _session, Server& _server) = nullptr;
	void (*onStopHolding)(ItemStack* _stack) = nullptr;
};

extern std::unordered_map<ItemId, ItemBehavior> itemBehavior;
extern std::unordered_map<ItemId, ItemProperties> itemProperties;

void EatFood(PlayerSession& _session, ItemStack* _stack, Entity& _target);

// Returns max stack size for this item/block id
ItemAmount GetMaxStack(const ItemId _id);

// Returns max durability (0 = not damageable)
ItemDamage GetMaxDurability(const ItemId _id);

// Returns regen amount
EntityHealth GetRegenerationAmount(const ItemId _id);

constexpr bool HasSubtypes(ItemId _id) {
	return (_id == Id::DYE || _id == BLOCK_SAPLING || _id == BLOCK_LEAVES || _id == BLOCK_WOOL || _id == BLOCK_LOG);
}

constexpr bool IsItem(ItemId _id) {
	return (_id >= Id::SHOVEL_IRON && _id < Id::MAX) || (_id >= Id::RECORD_13 && _id < Id::RECORD_MAX);
}

constexpr bool IsBlock(ItemId _id) {
	return _id > BLOCK_INVALID && _id < BLOCK_MAX;
}

constexpr bool IsValidId(ItemId _id) {
	return IsBlock(_id) || IsItem(_id);
}

constexpr bool IsArmor(ItemId _id) {
	return (_id >= Items::HELMET_LEATHER && _id <= Items::BOOTS_GOLD) || _id == BLOCK_PUMPKIN;
}

constexpr bool IsThrowable(ItemId _id) {
	return (_id == Items::SNOWBALL || _id == Items::EGG);
}

bool IsStackable(ItemId _id);

bool IsFood(ItemId _id);

constexpr int GetArmorDamageReduction(ItemId _id) {
	if (!IsArmor(_id) || _id == BLOCK_PUMPKIN)
		return 0;

	int damageReduceAmounts[4] = { 3, 8, 6, 3 };
	// ok ok I wanted to explain this because it is a bit convoluted.
	// Armor starts at id 298, and from there it goes helmet, chestplate, leggings, boots
	// Each of the following tiers of armor follow this pattern from then on
	int pieceIndex = (_id - Items::Id::HELMET_LEATHER) % 4;
	return damageReduceAmounts[pieceIndex];
}

constexpr Items::ArmorPiece GetArmorPiece(ItemId _id) {
	if (!IsArmor(_id))
		return ArmorPiece::INVALID_PIECE;

	if (_id == BLOCK_PUMPKIN)
		return ArmorPiece::HELMET;

	int pieceIndex = (_id - Items::Id::HELMET_LEATHER) % 4;
	Items::ArmorPiece type[4] = { Items::ArmorPiece::HELMET, Items::ArmorPiece::CHESTPLATE, Items::ArmorPiece::LEGGING,
		                          Items::ArmorPiece::BOOT };
	return type[pieceIndex];
}

inline void DamageItem(ItemStack& _stack, int _damage) {
	int max = GetMaxDurability(_stack.id);
	if (!max)
		return;

	_stack.data += _damage;
	if (_stack.data >= max) {
		_stack = {};
	}
}

// True when an items data value is meaningless for crafting
[[nodiscard]] constexpr bool IgnoresMetadataInRecipes(ItemId _id) noexcept {
	switch (_id) {
	case BLOCK_WOOL:
	case BLOCK_LOG:
	case BLOCK_SAPLING:
	case BLOCK_PLANKS:
		return true;
	default:
		return false;
	}
}

}; // namespace Items