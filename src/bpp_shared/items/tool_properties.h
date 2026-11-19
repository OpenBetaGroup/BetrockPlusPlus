/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "direction.h"

#include "base_types.h"
#include "blocks.h"
#include "entities/entity.h"
#include "inventory/item_stack.h"
#include "numeric_structs.h"
#include "spawner.h"
#include "world.h"
#include <cstdint>
#include <unordered_map>

namespace Items {
// Tool material durability
static constexpr ItemDamage DURABILITY_WOOD = 59;
static constexpr ItemDamage DURABILITY_STONE = 131;
static constexpr ItemDamage DURABILITY_IRON = 250;
static constexpr ItemDamage DURABILITY_DIAMOND = 1561;
static constexpr ItemDamage DURABILITY_GOLD = 32;

// Armor max damage = maxDamageArray[slot] * 3 << material
// material: leather=0, chain/gold=1, iron=2, diamond=3
// slot: helmet=0(x11), chest=1(x16), legs=2(x15), boots=3(x13)
static constexpr ItemDamage DURABILITY_HELMET_LEATHER = 11 * 3 * 1; // 33
static constexpr ItemDamage DURABILITY_CHEST_LEATHER = 16 * 3 * 1;  // 48
static constexpr ItemDamage DURABILITY_LEGS_LEATHER = 15 * 3 * 1;   // 45
static constexpr ItemDamage DURABILITY_BOOTS_LEATHER = 13 * 3 * 1;  // 39

static constexpr ItemDamage DURABILITY_HELMET_CHAINMAIL = 11 * 3 * 2; // 66
static constexpr ItemDamage DURABILITY_CHEST_CHAINMAIL = 16 * 3 * 2;  // 96
static constexpr ItemDamage DURABILITY_LEGS_CHAINMAIL = 15 * 3 * 2;   // 90
static constexpr ItemDamage DURABILITY_BOOTS_CHAINMAIL = 13 * 3 * 2;  // 78

static constexpr ItemDamage DURABILITY_HELMET_IRON = 11 * 3 * 4; // 132
static constexpr ItemDamage DURABILITY_CHEST_IRON = 16 * 3 * 4;  // 192
static constexpr ItemDamage DURABILITY_LEGS_IRON = 15 * 3 * 4;   // 180
static constexpr ItemDamage DURABILITY_BOOTS_IRON = 13 * 3 * 4;  // 156

static constexpr ItemDamage DURABILITY_HELMET_DIAMOND = 11 * 3 * 8; // 264
static constexpr ItemDamage DURABILITY_CHEST_DIAMOND = 16 * 3 * 8;  // 384
static constexpr ItemDamage DURABILITY_LEGS_DIAMOND = 15 * 3 * 8;   // 360
static constexpr ItemDamage DURABILITY_BOOTS_DIAMOND = 13 * 3 * 8;  // 312

static constexpr ItemDamage DURABILITY_HELMET_GOLD = 11 * 3 * 2; // 66
static constexpr ItemDamage DURABILITY_CHEST_GOLD = 16 * 3 * 2;  // 96
static constexpr ItemDamage DURABILITY_LEGS_GOLD = 15 * 3 * 2;   // 90
static constexpr ItemDamage DURABILITY_BOOTS_GOLD = 13 * 3 * 2;  // 78

static constexpr ItemDamage DURABILITY_FISHING_ROD = 64;
static constexpr ItemDamage DURABILITY_FLINT_AND_STEEL = 64;
static constexpr ItemDamage DURABILITY_SHEARS = 238;
static constexpr ItemDamage DURABILITY_BOW = 384;

enum class ToolLevel : int8_t {
	None = -1,
	WoodenOrGold = 0,
	Stone = 1,
	Iron = 2,
	Diamond = 3
};
enum class ToolMaterial : int8_t {
	None = -1,
	Wooden = 0,
	Gold = 1,
	Stone = 2,
	Iron = 3,
	Diamond = 4
};
enum class ToolType : int8_t {
	None = -1,
	Hoe = 0,
	Shovel = 1,
	Pickaxe = 2,
	Axe = 3,
	Sword = 4,
};
enum class ArmorType : int8_t {
	None = -1,
	Boots = 0,
	Leggings = 1,
	Chestplate = 2,
	Helmet = 3,
};

ItemDamage GetMaterialUses(ToolMaterial _material);

constexpr bool IsHoe(ItemId _id);
constexpr bool IsSword(ItemId _id);
constexpr bool IsPickaxe(ItemId _id);
constexpr bool IsAxe(ItemId _id);
constexpr bool IsShovel(ItemId _id);
bool IsEffective(ToolType _type, BlockType _block);
bool IsWeapon(ItemId _id);
bool IsTool(ItemId _id);
bool IsRecord(ItemId _id);

// True if the held item (or bare hand via material.isHarvestable) can harvest the block.
// Determines whether dig speed uses /30 (harvest) or /100 (cannot harvest).
bool CanPlayerHarvest(ItemStack* _stack, BlockType _targetBlock);

// Item strength vs block (tool efficiency, else 1.0). Does not apply water/air penalties.
float GetStrengthAgainstBlock(ItemStack* _stack, BlockType _targetBlock);

// Block damage dealt this tick (0..1+). Instant break when >= 1.0.
// Water and airborne each divide efficiency by 5 (stacking), matching beta EntityPlayer.
float BlockDamagePerTick(ItemStack* _stack, BlockType _targetBlock, bool _inWater, bool _onGround);

// CanHarvest callbacks used by tools that override Item.canHarvestBlock
bool CanPickaxeHarvest(ToolLevel _level, BlockType _block);
bool CanShovelHarvest(ToolLevel _level, BlockType _block);
bool CanShearsOrSwordHarvest(ToolLevel _level, BlockType _block);

// Effectiveness (GetStrengthAgainstBlock) callbacks
float ToolEffectiveness(ItemStack* _stack, BlockType _block);
float SwordEffectiveness(ItemStack* _stack, BlockType _block);
float ShearsEffectiveness(ItemStack* _stack, BlockType _block);

// Usage
void HarmTool(ItemStack* _stack, const int _amount = 1);
void OnToolFinishMining(ItemStack* _stack, BlockType _targetBlock);
void UseHoe(WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& _user, Cardinal::Value _face);
void UseFlintAndSteel(WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& _user, Cardinal::Value _face);
void UseBoat(WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& _user, Cardinal::Value _face);
void UseMinecart(WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& _user, Cardinal::Value _face);
void UseBucket(WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& _user, Cardinal::Value _face);
void UseWaterBucket(WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& _user, Cardinal::Value _face);
void UseLavaBucket(WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& _user, Cardinal::Value _face);
void UseShears(WorldManager& _world, Entity& _targetEntity, ItemStack* _stack);
void UseSaddle(WorldManager& _world, Entity& _targetEntity, ItemStack* _stack);
void UseBucketOnEntity(WorldManager& _world, Entity& _targetEntity, ItemStack* _stack);
void UseBow(PlayerSession& _session, ItemStack* /*_stack*/, Entity& /*_target*/);
void UseThrowable(PlayerSession& _session, ItemStack* _stack, Entity& /*_target*/);
void UseFishingRod(PlayerSession& _session, ItemStack* _stack, Entity& /*_target*/);
void UseDye(WorldManager& /*_world*/, Entity& _targetEntity, ItemStack* _stack);

// Attack
void TestSetGoal(WorldManager& _world, ItemStack* _stack, Int3 _pos, Cardinal::Value _face);
void AttackWithItem(Entity& _targetEntity, Entity& _sourceEntity, ItemStack* _stack);

struct ToolProperties {
	ToolType type = ToolType::None;
	ToolMaterial material = ToolMaterial::None;
	ItemDamage maxUses = -1;
	bool (*canHarvest)(ToolLevel _level, BlockType _targetBlock) = nullptr;
	float (*howEffectiveAgainstBlock)(ItemStack* _stack, BlockType _targetBlock) = nullptr;
};

struct ToolBehavior {
	void (*onBlockStartMining)(ItemStack* _stack, BlockType _targetBlock) = nullptr;
	void (*onBlockFinishMining)(ItemStack* _stack, BlockType _targetBlock) = nullptr;
	void (*onEntityAttack)(Entity& _attackedEntity, Entity& _sourceEntity, ItemStack* _stack) = nullptr;
	void (*onEntityUse)(WorldManager& _world, Entity& _targetEntity, ItemStack* _stack) = nullptr;
};

extern std::unordered_map<ItemId, ToolProperties> toolProperties;
extern std::unordered_map<ItemId, ToolBehavior> toolBehavior;
}; // namespace Items