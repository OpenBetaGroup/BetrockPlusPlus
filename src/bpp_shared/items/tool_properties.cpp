/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "tool_properties.h"
#include "blocks.h"
#include "blocks/block_properties.h"
#include "entities/entity.h"
#include "entities/entity_arrow.h"
#include "entities/entity_boat.h"
#include "entities/entity_cow.h"
#include "entities/entity_egg.h"
#include "entities/entity_fishing_bobber.h"
#include "entities/entity_item.h"
#include "entities/entity_manager.h"
#include "entities/entity_minecart.h"
#include "entities/entity_mobile.h"
#include "entities/entity_pig.h"
#include "entities/entity_player.h"
#include "entities/entity_sheep.h"
#include "entities/entity_snowball.h"
#include "inventory/item_stack.h"
#include "items.h"
#include "items/item_properties.h"
#include "logger.h"
#include "packet/packet_utils.h"
#include "raycast.h"
#include "world/managers/rail_manager.h"

namespace Items {
std::unordered_map<ItemId, ToolProperties> toolProperties = {};
std::unordered_map<ItemId, ToolBehavior> toolBehavior = {};

constexpr bool IsHoe(ItemId _id) {
	return (_id >= Items::HOE_WOOD && _id <= Items::HOE_GOLD);
}

constexpr bool IsSword(ItemId _id) {
	return (_id == Items::SWORD_IRON || _id == Items::SWORD_WOOD || _id == Items::SWORD_STONE ||
	        _id == Items::SWORD_DIAMOND || _id == Items::SWORD_GOLD);
}

constexpr bool IsPickaxe(ItemId _id) {
	return (_id == Items::PICKAXE_IRON || _id == Items::PICKAXE_WOOD || _id == Items::PICKAXE_STONE ||
	        _id == Items::PICKAXE_DIAMOND || _id == Items::PICKAXE_GOLD);
}

constexpr bool IsAxe(ItemId _id) {
	return (_id == Items::AXE_IRON || _id == Items::AXE_WOOD || _id == Items::AXE_STONE || _id == Items::AXE_DIAMOND ||
	        _id == Items::AXE_GOLD);
}

constexpr bool IsShovel(ItemId _id) {
	return (_id == Items::SHOVEL_IRON || _id == Items::SHOVEL_WOOD || _id == Items::SHOVEL_STONE ||
	        _id == Items::SHOVEL_DIAMOND || _id == Items::SHOVEL_GOLD);
}

bool IsWeapon(ItemId _id) {
	return IsSword(_id) || _id == Items::BOW;
}

bool IsTool(ItemId _id) {
	return IsShovel(_id) || IsAxe(_id) || IsPickaxe(_id) || IsHoe(_id) || _id == Items::FLINT_AND_STEEL ||
	       _id == Items::FISHING_ROD || _id == Items::SHEARS;
}

bool IsRecord(ItemId _id) {
	return _id == Items::RECORD_13 || _id == Items::RECORD_CAT;
}

constexpr ToolLevel MaterialToLevel(ToolMaterial _material) {
	switch (_material) {
	case ToolMaterial::None:
		return ToolLevel::None;
	case ToolMaterial::Wooden:
		return ToolLevel::WoodenOrGold;
	case ToolMaterial::Gold:
		return ToolLevel::WoodenOrGold;
	case ToolMaterial::Stone:
		return ToolLevel::Stone;
	case ToolMaterial::Iron:
		return ToolLevel::Iron;
	case ToolMaterial::Diamond:
		return ToolLevel::Diamond;
	}
	return ToolLevel::None;
}

constexpr float MaterialToEfficiency(ToolMaterial _material) {
	switch (_material) {
	case ToolMaterial::None:
		return 1.0f;
	case ToolMaterial::Wooden:
		return 2.0f;
	case ToolMaterial::Gold:
		return 12.0f;
	case ToolMaterial::Stone:
		return 4.0f;
	case ToolMaterial::Iron:
		return 6.0f;
	case ToolMaterial::Diamond:
		return 8.0f;
	}
	return 1.0f;
}

constexpr int BaseToolDamage(ToolType _type) {
	switch (_type) {
	case ToolType::Sword:
		return 4;
	case ToolType::Axe:
		return 3;
	case ToolType::Pickaxe:
		return 2;
	case ToolType::Shovel:
	case ToolType::Hoe:
		return 1;
	case ToolType::None:
		return 0;
	}
	// Default
	return 0;
}

EntityHealth CalculateDamage(ToolType _type, ToolLevel _level) {
	return BaseToolDamage(_type) + (int(_level) * 2);
}

static bool InflictDamage(Entity& _targetEntity, Entity& _sourceEntity, EntityHealth _damage, DamageType _type) {
	return _targetEntity.AttackEntityFrom(&_sourceEntity, _damage, _type);
}

bool IsEffective(ToolType _type, BlockType _block) {
	switch (_type) {
	case ToolType::Pickaxe:
		return (_block == BLOCK_COBBLESTONE || _block == BLOCK_DOUBLE_SLAB || _block == BLOCK_SLAB ||
		        _block == BLOCK_STONE || _block == BLOCK_SANDSTONE || _block == BLOCK_COBBLESTONE_MOSSY ||
		        _block == BLOCK_ORE_IRON || _block == BLOCK_IRON || _block == BLOCK_ORE_COAL ||
		        _block == BLOCK_ORE_GOLD || _block == BLOCK_GOLD || _block == BLOCK_ORE_DIAMOND ||
		        _block == BLOCK_DIAMOND || _block == BLOCK_ICE || _block == BLOCK_NETHERRACK ||
		        _block == BLOCK_ORE_LAPIS_LAZULI || _block == BLOCK_LAPIS_LAZULI);
	case ToolType::Axe:
		return (_block == BLOCK_PLANKS || _block == BLOCK_BOOKSHELF || _block == BLOCK_LOG || _block == BLOCK_CHEST);
	case ToolType::Shovel:
		return (_block == BLOCK_GRASS || _block == BLOCK_DIRT || _block == BLOCK_SAND || _block == BLOCK_GRAVEL ||
		        _block == BLOCK_SNOW_LAYER || _block == BLOCK_SNOW || _block == BLOCK_CLAY || _block == BLOCK_FARMLAND);
	default:
		return false;
	}
}

bool CanPickaxeHarvest(ToolLevel _level, BlockType _block) {
	if (_block == BLOCK_OBSIDIAN)
		return (_level == ToolLevel::Diamond);
	if (_block == BLOCK_DIAMOND || _block == BLOCK_ORE_DIAMOND)
		return (_level >= ToolLevel::Iron);
	if (_block == BLOCK_GOLD || _block == BLOCK_ORE_GOLD)
		return (_level >= ToolLevel::Iron);
	if (_block == BLOCK_IRON || _block == BLOCK_ORE_IRON)
		return (_level >= ToolLevel::Stone);
	if (_block == BLOCK_LAPIS_LAZULI || _block == BLOCK_ORE_LAPIS_LAZULI)
		return (_level >= ToolLevel::Stone);
	if (_block == BLOCK_ORE_REDSTONE_OFF || _block == BLOCK_ORE_REDSTONE_ON)
		return (_level >= ToolLevel::Iron);
	auto mat = Blocks::blockProperties[_block].material;
	if (mat != Material::Rock()) {
		return (mat == Material::Iron());
	}
	return true;
}

bool CanShovelHarvest(ToolLevel /*_level*/, BlockType _block) {
	return (_block == BLOCK_SNOW_LAYER || _block == BLOCK_SNOW);
}

bool CanShearsOrSwordHarvest(ToolLevel /*_level*/, BlockType _block) {
	return (_block == BLOCK_COBWEB);
}

float ToolEffectiveness(ItemStack* _stack, BlockType _block) {
	if (!_stack || !toolProperties.contains(_stack->id))
		return 1.0f;
	const auto& props = toolProperties[_stack->id];
	if (IsEffective(props.type, _block))
		return MaterialToEfficiency(props.material);
	return 1.0f;
}

float SwordEffectiveness(ItemStack* /*_stack*/, BlockType _block) {
	// ItemSword.GetStrengthAgainstBlock: cobweb 15, everything else 1.5
	return (_block == BLOCK_COBWEB) ? 15.0f : 1.5f;
}

float ShearsEffectiveness(ItemStack* /*_stack*/, BlockType _block) {
	switch (_block) {
	case BLOCK_COBWEB:
	case BLOCK_LEAVES:
		return 15.0f;
	case BLOCK_WOOL:
		return 5.0f;
	default:
		return 1.0f;
	}
}

bool CanPlayerHarvest(ItemStack* _stack, BlockType _targetBlock) {
	// InventoryPlayer.canHarvestBlock: material.isHarvestable OR held.canHarvestBlock
	if (Blocks::blockProperties[_targetBlock].material.isHarvestable)
		return true;
	if (!_stack || !toolProperties.contains(_stack->id))
		return false;
	const auto& props = toolProperties[_stack->id];
	if (!props.canHarvest)
		return false;
	return props.canHarvest(MaterialToLevel(props.material), _targetBlock);
}

float GetStrengthAgainstBlock(ItemStack* _stack, BlockType _targetBlock) {
	if (!_stack || !toolProperties.contains(_stack->id))
		return 1.0f;
	const auto& props = toolProperties[_stack->id];
	if (props.howEffectiveAgainstBlock)
		return props.howEffectiveAgainstBlock(_stack, _targetBlock);
	return 1.0f;
}

// If hardness < 0 -> 0
// If can't Harvest -> 1 / hardness / 100   (no water/air penalty)
// If can Harvest -> strengthVsBlock / hardness / 30
// Water and not onGround each divide strengthVsBlock by 5
float BlockDamagePerTick(ItemStack* _stack, BlockType _targetBlock, bool _inWater, bool _onGround) {
	float hardness = Blocks::blockProperties[_targetBlock].hardness;
	if (hardness < 0.0f)
		return 0.0f;
	if (hardness == 0.0f)
		return 1.0f;

	if (!CanPlayerHarvest(_stack, _targetBlock))
		return 1.0f / hardness / 100.0f;

	float efficiency = GetStrengthAgainstBlock(_stack, _targetBlock);
	if (_inWater) {
		efficiency /= 5.0f;
	}
	if (!_onGround) {
		efficiency /= 5.0f;
	}
	return efficiency / hardness / 30.0f;
}

void AttackWithItem(Entity& _targetEntity, Entity& _sourceEntity, ItemStack* _stack) {
	EntityHealth damage = 1;
	if (toolProperties.contains(_stack->id))
		damage = CalculateDamage(toolProperties[_stack->id].type, MaterialToLevel(toolProperties[_stack->id].material));
	InflictDamage(_targetEntity, _sourceEntity, damage, DamageType::Item);

	// Players alert wolves
	auto player = dynamic_cast<PlayerEntity*>(&_sourceEntity);
	if (player) {
		_sourceEntity.entityManager->AlertWolves(*player, _targetEntity, /*ignoreSitting=*/ true);
	}

	// ItemSword.hitEntity damages 1; ItemTool.hitEntity damages 2
	int durabilityLoss = 1;
	if (toolProperties.contains(_stack->id)) {
		switch (toolProperties[_stack->id].type) {
		case ToolType::Pickaxe:
		case ToolType::Axe:
		case ToolType::Shovel:
			durabilityLoss = 2;
			break;
		default:
			break;
		}
	}
	HarmTool(_stack, durabilityLoss);
}

ItemDamage GetMaterialUses(ToolMaterial _material) {
	switch (_material) {
	case ToolMaterial::Wooden:
		return DURABILITY_WOOD;
	case ToolMaterial::Gold:
		return DURABILITY_GOLD;
	case ToolMaterial::Stone:
		return DURABILITY_STONE;
	case ToolMaterial::Iron:
		return DURABILITY_IRON;
	case ToolMaterial::Diamond:
		return DURABILITY_DIAMOND;
	default:
		return -1;
	}
}

void HarmTool(ItemStack* _stack, const int _amount) {
	if (!_stack || _amount <= 0 || !toolProperties.contains(_stack->id))
		return;
	_stack->data = static_cast<ItemDamage>(_stack->data + _amount);
	if (_stack->data >= toolProperties[_stack->id].maxUses) {
		_stack->DecrementCount(1);
	}
}

void OnToolFinishMining(ItemStack* _stack, BlockType _targetBlock) {
	if (!_stack || !toolProperties.contains(_stack->id))
		return;
	const auto& props = toolProperties[_stack->id];
	switch (props.type) {
	case ToolType::Sword:
		HarmTool(_stack, 2);
		return;
	case ToolType::Pickaxe:
	case ToolType::Axe:
	case ToolType::Shovel:
		HarmTool(_stack, 1);
		return;
	case ToolType::Hoe:
		return;
	case ToolType::None:
		if (_stack->id == Items::SHEARS && (_targetBlock == BLOCK_LEAVES || _targetBlock == BLOCK_COBWEB)) {
			HarmTool(_stack, 1);
		}
		return;
	}
}

void UseHoe(WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& /*_user*/, Cardinal::Value /*_face*/) {
	BlockType type = _world.GetBlockId(_pos);
	if (type != BLOCK_GRASS && type != BLOCK_DIRT)
		return;
	_world.SetBlock(_pos, BLOCK_FARMLAND);
	HarmTool(_stack, 1);
}

void UseBow(PlayerSession& _session, ItemStack* /*_stack*/, Entity& /*_target*/) {
	auto& inv = _session.inventory;
	int slot = -1;

	// Check hotbar then main inventory
	for (int i = 36; i <= 44 && slot < 0; i++)
		if (inv.slots[i].id == Items::Id::ARROW)
			slot = i;
	for (int i = 9; i <= 35 && slot < 0; i++)
		if (inv.slots[i].id == Items::Id::ARROW)
			slot = i;
	if (slot < 0 || !_session.entity || !_session.entity->world)
		return;

	inv.slots[slot].DecrementCount(1);
	inv.OnInventoryChanged();
	_session.entity->world->entityManager.AddEntity(std::make_shared<ArrowEntity>(_session.entity));
}

void UseThrowable(PlayerSession& _session, ItemStack* _stack, Entity& /*_target*/) {
	if (_stack->id != Items::EGG && _stack->id != Items::SNOWBALL)
		return;

	auto type = _stack->id;
	_stack->DecrementCount(1);

	if (type == Items::EGG) {
		_session.entity->world->entityManager.AddEntity(std::make_shared<EggEntity>(_session.entity));
		return;
	}
	_session.entity->world->entityManager.AddEntity(std::make_shared<SnowballEntity>(_session.entity));
}

void UseFishingRod(PlayerSession& _session, ItemStack* _stack, Entity& /*_target*/) {
	if (!_session.entity || !_session.entity->world || !_stack || _stack->id != Items::Id::FISHING_ROD)
		return;

	constexpr double PULL_STRENGTH = 0.1;

	auto player = std::static_pointer_cast<PlayerEntity>(_session.entity);
	auto bobber = player->fishingBobber.lock();

	if (!bobber || bobber->isDead) {
		Vec3 direction = player->GetLookVector(player->rotationYaw, player->rotationPitch);
		auto cast = std::make_shared<FishingBobberEntity>(player, direction);
		player->fishingBobber = cast;
		player->world->entityManager.AddEntity(cast);
		return;
	}

	int duraCost = 0;
	if (auto hooked = bobber->hookedEntity.lock()) {
		Vec3 delta = player->position - bobber->position;
		double length = delta.Length();

		hooked->velocity.x += delta.x * PULL_STRENGTH;
		hooked->velocity.y += delta.y * PULL_STRENGTH + MathHelper::SqrtDouble(length) * 0.08;
		hooked->velocity.z += delta.z * PULL_STRENGTH;
		duraCost = 3;
	} else if (bobber->IsCatchable()) {
		Vec3 delta = player->position - bobber->position;
		double length = delta.Length();
		auto fish = std::make_shared<ItemEntity>(bobber->position);

		fish->itemStack = { Items::Id::FISH, 1, 0 };
		fish->dim = player->dim;
		fish->velocity = { delta.x * PULL_STRENGTH, delta.y * PULL_STRENGTH + MathHelper::SqrtDouble(length) * 0.08,
			               delta.z * PULL_STRENGTH };

		player->world->entityManager.AddEntity(fish);
		duraCost = 1;
	} else if (bobber->IsInGround()) {
		duraCost = 2;
	}

	bobber->isDead = true;
	player->fishingBobber.reset();

	if (duraCost > 0) {
		ItemStack before = *_stack;
		DamageItem(*_stack, duraCost);
		if (*_stack != before)
			PacketUtilities::SendSlot(_session, 0, _session.inventory.activeHotbarSlot + 36, _stack);
	}
}

void UseBoat(WorldManager& _world, ItemStack* _stack, Int3 /*_pos*/, Entity& _user, Cardinal::Value /*_face*/) {
	float reach = 5.0f;
	auto pe = dynamic_cast<PlayerEntity*>(&_user);
	if (!pe) {
		return;
	}

	// Do our raycast
	Vec3 startPos = pe->position;
	startPos.y += PLAYER_EYE_HEIGHT;
	Vec3 lookVec = pe->GetLookVector(pe->rotationYaw, pe->rotationPitch);
	Vec3 endPos = startPos + (lookVec * Vec3{ reach, reach, reach });
	RayCastResult result = Raycast::Raycast(_world, startPos, endPos, RayCastMode::ACCEPT_SOURCES);

	if (!result.hit) {
		return;
	}
	BoatEntity boat;

	if (result.hitBlock.type == BLOCK_SNOW_LAYER) {
		result.blockPosition.y--;
	}

	if (!_world.CanModify(_user, result.blockPosition)) {
		return;
	}

	boat.Teleport(
	    { result.blockPosition.x + 0.5f, result.blockPosition.y + 1.0f + boat.yOffset, result.blockPosition.z + 0.5f });
	_world.entityManager.AddEntity(std::make_shared<BoatEntity>(boat));
	_stack->DecrementCount(1);
}

void UseMinecart(WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& /*_user*/, Cardinal::Value /*_face*/) {
	if (!RailManager::IsRail(_world.GetBlockId(_pos)))
		return;

	auto cart = std::make_shared<MinecartEntity>();
	if (_stack->id == Items::Id::MINECART_CHEST)
		cart->cartType = MinecartType::Chest;
	else if (_stack->id == Items::Id::MINECART_FURNACE)
		cart->cartType = MinecartType::Furnace;

	Vec3 spawnPos = { double(float(_pos.x) + 0.5f), double(float(_pos.y) + 0.5f) + double(cart->yOffset),
		              double(float(_pos.z) + 0.5f) };
	cart->Teleport(spawnPos);
	cart->prevPosition = { spawnPos.x, double(float(_pos.y) + 0.5f), spawnPos.z }; // vanilla prevPos skips yOffset
	_world.entityManager.AddEntity(cart);

	_stack->DecrementCount(1);
}

void UseBucket(WorldManager& _world, ItemStack* _stack, Int3 /*_pos*/, Entity& _user, Cardinal::Value /*_face*/) {
	float reach = 5.0f;
	auto pe = dynamic_cast<PlayerEntity*>(&_user);
	if (!pe) {
		return;
	}

	// Do our raycast
	Vec3 startPos = pe->position;
	startPos.y += PLAYER_EYE_HEIGHT;
	Vec3 lookVec = pe->GetLookVector(pe->rotationYaw, pe->rotationPitch);
	Vec3 endPos = startPos + (lookVec * Vec3{ reach, reach, reach });
	RayCastResult result = Raycast::Raycast(_world, startPos, endPos, RayCastMode::ACCEPT_SOURCES);

	if (!result.hit) {
		return;
	}

	if (!_world.CanModify(_user, result.blockPosition)) {
		return;
	}

	// Accept water or lava
	if (result.hitBlock.type == BLOCK_WATER_FLOWING || result.hitBlock.type == BLOCK_WATER_STILL) {
		_world.SetBlock(result.blockPosition, BLOCK_AIR);
		_stack->id = Items::Id::BUCKET_WATER;
	} else if (result.hitBlock.type == BLOCK_LAVA_FLOWING || result.hitBlock.type == BLOCK_LAVA_STILL) {
		_world.SetBlock(result.blockPosition, BLOCK_AIR);
		_stack->id = Items::Id::BUCKET_LAVA;
	}
}

void UseWaterBucket(WorldManager& _world, ItemStack* _stack, Int3 /*_pos*/, Entity& _user, Cardinal::Value /*_face*/) {
	float reach = 5.0f;
	auto pe = dynamic_cast<PlayerEntity*>(&_user);
	if (!pe) {
		return;
	}

	Vec3 startPos = pe->position;
	startPos.y += PLAYER_EYE_HEIGHT;
	Vec3 lookVec = pe->GetLookVector(pe->rotationYaw, pe->rotationPitch);
	Vec3 endPos = startPos + (lookVec * Vec3{ reach, reach, reach });
	RayCastResult result = Raycast::Raycast(_world, startPos, endPos, RayCastMode::IGNORE_FLUIDS);

	if (!result.hit) {
		return;
	}

	Int3 placePos = result.blockPosition.WithOffset(result.face);
	auto m = _world.GetMaterial(placePos);
	if (m.isSolid || !_world.CanModify(_user, placePos)) {
		return; // can't place into solid ground or protected area
	}

	if (_world.GetDimension() == Dimension::Nether) {
		_stack->id = Items::Id::BUCKET;
		return;
	}

	_world.SetBlock(placePos, BLOCK_WATER_FLOWING);
	_stack->id = Items::Id::BUCKET;
}

void UseLavaBucket(WorldManager& _world, ItemStack* _stack, Int3 /*_pos*/, Entity& _user, Cardinal::Value /*_face*/) {
	float reach = 5.0f;
	auto pe = dynamic_cast<PlayerEntity*>(&_user);
	if (!pe) {
		return;
	}

	Vec3 startPos = pe->position;
	startPos.y += PLAYER_EYE_HEIGHT;
	Vec3 lookVec = pe->GetLookVector(pe->rotationYaw, pe->rotationPitch);
	Vec3 endPos = startPos + (lookVec * Vec3{ reach, reach, reach });
	RayCastResult result = Raycast::Raycast(_world, startPos, endPos, RayCastMode::IGNORE_FLUIDS);

	if (!result.hit) {
		return;
	}

	Int3 placePos = result.blockPosition.WithOffset(result.face);
	auto m = _world.GetMaterial(placePos);
	if (m.isSolid || !_world.CanModify(_user, placePos)) {
		return; // can't place into solid ground or protected area
	}

	_world.SetBlock(placePos, BLOCK_LAVA_FLOWING);
	_stack->id = Items::Id::BUCKET;
}

void UseFlintAndSteel(WorldManager& _world, ItemStack* _stack, Int3 _pos, Entity& /*_user*/, Cardinal::Value _face) {
	_pos.Offset(_face);
	if (_world.GetBlockId(_pos) == BLOCK_AIR)
		_world.SetBlock(_pos, BLOCK_FIRE);
	HarmTool(_stack, 1);
}

void UseShears(WorldManager& _world, Entity& _targetEntity, ItemStack* _stack) {
	SheepEntity* se = dynamic_cast<SheepEntity*>(&_targetEntity);
	// Not a sheep, skip
	if (!se)
		return;
	// Already sheared, can't shear again!
	if (se->isSheared)
		return;
	se->DropItemAtEntity(BLOCK_WOOL, 2 + _world.rand.NextInt(3), se->color);
	se->UpdateMetadata(se->isSheared, true);

	HarmTool(_stack, 1);
}

void UseSaddle(WorldManager& /*_world*/, Entity& _targetEntity, ItemStack* _stack) {
	PigEntity* se = dynamic_cast<PigEntity*>(&_targetEntity);
	if (!se)
		return;
	if (se->isSaddled)
		return;
	se->isSaddled = true;
	_stack->DecrementCount(1);
}

void UseDye(WorldManager& /*_world*/, Entity& _targetEntity, ItemStack* _stack) {
	SheepEntity* se = dynamic_cast<SheepEntity*>(&_targetEntity);
	int8_t dyeColorInverted = 15 - _stack->data;
	if (!se)
		return;
	if (se->color == dyeColorInverted)
		return;
	if (se->isSheared)
		return;
	se->color = dyeColorInverted;
	se->wasMetadataUpdated = true;
	_stack->DecrementCount(1);
}

void UseBucketOnEntity(WorldManager& /*_world*/, Entity& _targetEntity, ItemStack* _stack) {
	CowEntity* se = dynamic_cast<CowEntity*>(&_targetEntity);
	// Not a cow, skip
	if (!se)
		return;
	_stack->id = BUCKET_MILK;
}

void TestSetGoal(WorldManager& _world, ItemStack* /*_stack*/, Int3 _pos, Cardinal::Value /*_face*/) {
	Int3 topPos = _pos;
	topPos.y += 1;
	_world.SetBlock(topPos, BLOCK_AIR);
	std::cout << "lol!!" << std::endl;
	for (auto& entity : _world.entityManager.entities) {
		std::cout << int(entity->type) << std::endl;
		if (entity->type == EntityType::PIG) {
			auto finder = std::static_pointer_cast<MobileEntity>(entity);
			std::cout << "Setting goal to" << topPos << std::endl;
			finder->SetGoal(topPos);
		}
	}
}

}; // namespace Items