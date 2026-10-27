/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "block_properties.h"
#include "blocks.h"
#include "entities/entity_item.h"
#include "packet_data.h"
#include "world/world.h"
#include <cstdint>

namespace Blocks {

// Global table definitions
BlockProperties blockProperties[BLOCK_MAX] = {};

// Behavior helper functions
bool CanFallAt(WorldAccess& _world, Int3 _position) {
	auto block = _world.GetBlockId(_position);
	if (block == BLOCK_AIR)
		return true;
	if (block == BLOCK_FIRE)
		return true;
	auto material = Blocks::blockProperties[block].material.type;
	if (material == MaterialType::Lava || material == MaterialType::Water)
		return true;
	return false;
}

void BreakAndDropBlock(WorldManager& _world, Int3 _pos) {
	BlockType blockId = _world.GetBlockId({ _pos.x, _pos.y, _pos.z });
	if (blockId == BLOCK_AIR)
		return;
	uint8_t meta = _world.GetMetadata({ _pos.x, _pos.y, _pos.z });
	_world.SetBlock({ _pos.x, _pos.y, _pos.z }, BLOCK_AIR);

	std::vector<ItemStack> drops = Blocks::GetBlockDrops(blockId, meta, _world.rand);

	for (ItemStack drop : drops) {
		Vec3 dropPos = { double(_pos.x), double(_pos.y), double(_pos.z) };
		float offset = 0.7f;
		dropPos.x += (_world.rand.NextFloat() * offset) + (1.0f - offset) * 0.5;
		dropPos.y += (_world.rand.NextFloat() * offset) + (1.0f - offset) * 0.5;
		dropPos.z += (_world.rand.NextFloat() * offset) + (1.0f - offset) * 0.5;
		ItemEntity item(dropPos);
		item.itemStack = drop;
		_world.entityManager.AddEntity(std::make_shared<ItemEntity>(item));
	}
	return;
}

void BreakAndDropBlockWithChance(WorldManager& _world, Int3 _pos, float _chance) {
	BlockType blockId = _world.GetBlockId({ _pos.x, _pos.y, _pos.z });
	if (blockId == BLOCK_AIR)
		return;
	uint8_t meta = _world.GetMetadata({ _pos.x, _pos.y, _pos.z });
	_world.SetBlock({ _pos.x, _pos.y, _pos.z }, BLOCK_AIR);

	DropBlockItemsWithChance(_world, _pos, blockId, meta, _chance);
}

void DropBlockItemsWithChance(WorldManager& _world, Int3 _pos, BlockType _blockId, uint8_t _meta, float _chance) {
	std::vector<ItemStack> drops = Blocks::GetBlockDrops(_blockId, _meta, _world.rand);

	for (ItemStack drop : drops) {
		if (_world.rand.NextFloat() <= _chance) {
			Vec3 dropPos = { double(_pos.x), double(_pos.y), double(_pos.z) };
			float offset = 0.7f;
			dropPos.x += (_world.rand.NextFloat() * offset) + (1.0f - offset) * 0.5;
			dropPos.y += (_world.rand.NextFloat() * offset) + (1.0f - offset) * 0.5;
			dropPos.z += (_world.rand.NextFloat() * offset) + (1.0f - offset) * 0.5;
			ItemEntity item(dropPos);
			item.itemStack = drop;
			_world.entityManager.AddEntity(std::make_shared<ItemEntity>(item));
		}
	}
	return;
}

void DropBlockAt(WorldManager& _world, Int3 _pos, BlockType _id, ItemAmount _count, int16_t _data) {
	Vec3 dropPos = { double(_pos.x), double(_pos.y), double(_pos.z) };
	float offset = 0.7f;
	dropPos.x += (_world.rand.NextFloat() * offset) + (1.0f - offset) * 0.5;
	dropPos.y += (_world.rand.NextFloat() * offset) + (1.0f - offset) * 0.5;
	dropPos.z += (_world.rand.NextFloat() * offset) + (1.0f - offset) * 0.5;
	ItemEntity item(dropPos);
	item.itemStack = { .id = _id, .count = _count, .data = _data };
	_world.entityManager.AddEntity(std::make_shared<ItemEntity>(item));
	return;
}

void DropItemAt(WorldManager& _world, Int3 _pos, Items::Id _id, ItemAmount _count, int16_t _data) {
	Vec3 dropPos = { double(_pos.x), double(_pos.y), double(_pos.z) };
	float offset = 0.7f;
	dropPos.x += (_world.rand.NextFloat() * offset) + (1.0f - offset) * 0.5;
	dropPos.y += (_world.rand.NextFloat() * offset) + (1.0f - offset) * 0.5;
	dropPos.z += (_world.rand.NextFloat() * offset) + (1.0f - offset) * 0.5;
	ItemEntity item(dropPos);
	item.itemStack = { .id = _id, .count = _count, .data = _data };
	_world.entityManager.AddEntity(std::make_shared<ItemEntity>(item));
	return;
}

bool CanSugarcaneSurviveAt(WorldAccess& _world, Int3 _pos) {
	auto belowBlock = _world.GetBlockId({ _pos.x, _pos.y - 1, _pos.z });
	if (belowBlock == BLOCK_SUGARCANE)
		return true;
	if (belowBlock != BLOCK_GRASS && belowBlock != BLOCK_DIRT)
		return false;
	// Check for water
	int d[4] = { -1, 1, 0, 0 };
	for (int i = 0; i < 4; i++) {
		int dx = d[i];
		int dz = d[3 - i];
		auto adjacentBlock = _world.GetBlockId({ _pos.x + dx, _pos.y - 1, _pos.z + dz });
		if (adjacentBlock == BLOCK_WATER_FLOWING || adjacentBlock == BLOCK_WATER_STILL)
			return true;
	}
	return false;
}

bool CanCakeSurviveAt(WorldAccess& _world, Int3 _pos) {
	return _world.GetMaterial(_pos.Offset(Cardinal::Value::Down)).isSolid;
}

bool CanCropsSurviveAt(WorldAccess& _world, Int3 _pos) {
	return _world.GetBlockId(_pos.Offset(Cardinal::Value::Down)) == BLOCK_FARMLAND;
}

bool CanGenericPlantSurviveAt(WorldAccess& _world, Int3 _pos) {
	auto lightLevel = _world.GetBlockLightRaw(_pos);
	bool canSeeSky = _world.CanBlockSeeSky(_pos);
	auto belowBlock = _world.GetBlockId({ _pos.x, _pos.y - 1, _pos.z });
	bool canGrowOnBlock = belowBlock == BLOCK_FARMLAND || belowBlock == BLOCK_DIRT || belowBlock == BLOCK_GRASS;
	return (lightLevel >= 8 || canSeeSky) && canGrowOnBlock;
}

bool CanMushroomSurviveAt(WorldAccess& _world, Int3 _pos) {
	if (_pos.y < 0 || _pos.y >= CHUNK_HEIGHT)
		return false;
	auto lightLevel = _world.GetBlockLightRaw(_pos);
	auto belowBlock = _world.GetBlockId({ _pos.x, _pos.y - 1, _pos.z });
	bool canGrowOnBlock = blockProperties[belowBlock].isOpaqueCube;
	return lightLevel < 13 && canGrowOnBlock;
}

bool CanCactusSurviveAt(WorldAccess& _world, Int3 _pos) {
	bool adjacentBlocksClear = true;
	int d[4] = { -1, 1, 0, 0 };
	for (int i = 0; i < 4; i++) {
		int dx = _pos.x + d[i];
		int dz = _pos.z + d[3 - i];
		if (_world.GetMaterial({ dx, _pos.y, dz }).isSolid) {
			adjacentBlocksClear = false;
			break;
		}
	}
	auto belowBlock = _world.GetBlockId({ _pos.x, _pos.y - 1, _pos.z });
	bool canGrowOnBlock = belowBlock == BLOCK_CACTUS || belowBlock == BLOCK_SAND;
	return canGrowOnBlock && adjacentBlocksClear;
}

/**
 * @brief Says if a torch can attach to the block its placed on
 * 
 * @param _world Active world
 * @param _pos The position of the torch
 * @param _face The direction the torch is pointing out towards
 * @return true The torch can be placed
 * @return false The torch cannot be placed
 */
// ^^ Why is this one function so elaborated on? lol
bool CanTorchAttachTo(WorldManager& _world, Int3 _pos, Cardinal::Value _face) {
	// Torches cannot be placed on the ceiling
	if (_face == Cardinal::Value::Down)
		return false;
	// Get supporting block
	Int3 support = _pos.WithOffset(Cardinal::Opposite(_face));
	return _world.GetBlockId(support) != BLOCK_ICE &&
	       (_world.IsBlockNormalCube(support) ||
	        (_face == Cardinal::Value::Up && _world.GetBlockId(support) == BLOCK_FENCE));
}

// Some fluid specific stuff
float GetFluidPercentAir(uint8_t _meta) {
	if (_meta >= 8)
		_meta = 0;

	return float(_meta + 1) / 9.0f;
}

void RegisterBlockProperties() {
	// Air
	blockProperties[BlockType::BLOCK_AIR] = {
		.material = Material::Air(),
		.hardness = 0.0f,
		.resistance = 0.0f,
		.lightOpacity = 0,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.canBlockGrass = false,
		.enableStats = false,
	};

	// Stone
	blockProperties[BlockType::BLOCK_STONE] = {
		.material = Material::Rock(),
		.hardness = 1.5f,
		.resistance = 10.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Grass
	blockProperties[BlockType::BLOCK_GRASS] = {
		.material = Material::Grass(),
		.hardness = 0.6f,
		.lightOpacity = 255,
		.stepSound = StepSound::Grass,
		.ticksOnLoad = true,
	};

	// Dirt
	blockProperties[BlockType::BLOCK_DIRT] = {
		.material = Material::Ground(),
		.hardness = 0.5f,
		.lightOpacity = 255,
		.stepSound = StepSound::Gravel,
	};

	// Cobblestone
	blockProperties[BlockType::BLOCK_COBBLESTONE] = {
		.material = Material::Rock(),
		.hardness = 2.0f,
		.resistance = 10.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Planks (Oak Wood)
	blockProperties[BlockType::BLOCK_PLANKS] = {
		.material = Material::Wood(),
		.hardness = 2.0f,
		.resistance = 5.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Wood,
	};

	// Sapling
	blockProperties[BlockType::BLOCK_SAPLING] = {
		.material = Material::Plants(),
		.hardness = 0.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Grass,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.ticksOnLoad = true,
	};

	// Bedrock
	blockProperties[BlockType::BLOCK_BEDROCK] = {
		.material = Material::Rock(),
		.hardness = -1.0f, // unbreakable
		.resistance = 6000000.0f,
		.stepSound = StepSound::Stone,
		.enableStats = false,
	};

	// Water (flowing)
	blockProperties[BlockType::BLOCK_WATER_FLOWING] = {
		.material = Material::Water(),
		.hardness = 100.0f,
		.lightOpacity = 3,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.ticksOnLoad = true,
		.enableStats = false,
	};

	// Water (still/stationary)
	blockProperties[BlockType::BLOCK_WATER_STILL] = {
		.material = Material::Water(),
		.hardness = 100.0f,
		.lightOpacity = 3,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.enableStats = false,
	};

	// Lava (flowing)
	blockProperties[BlockType::BLOCK_LAVA_FLOWING] = {
		.material = Material::Lava(),
		.hardness = 0.0f,
		.lightEmission = 15, // setLightValue(1.0f) -> 15*1.0 = 15
		.lightOpacity = 255,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.ticksOnLoad = true,
		.enableStats = false,
	};

	// Lava (still/stationary)
	blockProperties[BlockType::BLOCK_LAVA_STILL] = {
		.material = Material::Lava(),
		.hardness = 100.0f,
		.lightEmission = 15,
		.lightOpacity = 255,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.ticksOnLoad = true,
		.enableStats = false,
	};

	// Sand
	blockProperties[BlockType::BLOCK_SAND] = {
		.material = Material::Sand(),
		.hardness = 0.5f,
		.lightOpacity = 255,
		.stepSound = StepSound::Sand,
	};

	// Gravel
	blockProperties[BlockType::BLOCK_GRAVEL] = {
		.material = Material::Sand(),
		.hardness = 0.6f,
		.lightOpacity = 255,
		.stepSound = StepSound::Gravel,
	};

	// Gold Ore
	blockProperties[BlockType::BLOCK_ORE_GOLD] = {
		.material = Material::Rock(),
		.hardness = 3.0f,
		.resistance = 5.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Iron Ore
	blockProperties[BlockType::BLOCK_ORE_IRON] = {
		.material = Material::Rock(),
		.hardness = 3.0f,
		.resistance = 5.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Coal Ore
	blockProperties[BlockType::BLOCK_ORE_COAL] = {
		.material = Material::Rock(),
		.hardness = 3.0f,
		.resistance = 5.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Wood Log
	blockProperties[BlockType::BLOCK_LOG] = {
		.material = Material::Wood(),
		.hardness = 2.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Wood,
	};

	// Leaves
	blockProperties[BlockType::BLOCK_LEAVES] = {
		.material = Material::Leaves(),
		.hardness = 0.2f,
		.lightOpacity = 1,
		.stepSound = StepSound::Grass,
		.ticksOnLoad = true,
		.enableStats = false,
	};

	// Sponge
	blockProperties[BlockType::BLOCK_SPONGE] = {
		.material = Material::Sponge(),
		.hardness = 0.6f,
		.lightOpacity = 255,
		.stepSound = StepSound::Grass,
	};

	// Glass
	blockProperties[BlockType::BLOCK_GLASS] = {
		.material = Material::Glass(),
		.hardness = 0.3f,
		.lightOpacity = 0,
		.stepSound = StepSound::Glass,
		.isOpaqueCube = false,
	};

	// Lapis Lazuli Ore
	blockProperties[BlockType::BLOCK_ORE_LAPIS_LAZULI] = {
		.material = Material::Rock(),
		.hardness = 3.0f,
		.resistance = 5.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Lapis Lazuli Block
	blockProperties[BlockType::BLOCK_LAPIS_LAZULI] = {
		.material = Material::Rock(),
		.hardness = 3.0f,
		.resistance = 5.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Dispenser
	blockProperties[BlockType::BLOCK_DISPENSER] = {
		.material = Material::Rock(),
		.hardness = 3.5f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Sandstone
	blockProperties[BlockType::BLOCK_SANDSTONE] = {
		.material = Material::Rock(),
		.hardness = 0.8f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Note Block
	blockProperties[BlockType::BLOCK_NOTEBLOCK] = {
		.material = Material::Wood(),
		.hardness = 0.8f,
		.lightOpacity = 255,
		.stepSound = StepSound::Wood,
	};

	// Bed
	blockProperties[BlockType::BLOCK_BED] = {
		.material = Material::Cloth(),
		.hardness = 0.2f,
		.pushMobilityOverride = PushabilityFlag::NoPush,
		.lightOpacity = 0,
		.stepSound = StepSound::Wood,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.enableStats = false,
	};

	// Powered Rail (Golden Rail)
	blockProperties[BlockType::BLOCK_RAIL_POWERED] = {
		.material = Material::Circuits(),
		.hardness = 0.7f,
		.pushMobilityOverride = PushabilityFlag::Normal,
		.lightOpacity = 0,
		.stepSound = StepSound::Stone,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Detector Rail
	blockProperties[BlockType::BLOCK_RAIL_DETECTOR] = {
		.material = Material::Circuits(),
		.hardness = 0.7f,
		.pushMobilityOverride = PushabilityFlag::Normal,
		.lightOpacity = 0,
		.stepSound = StepSound::Stone,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.ticksOnLoad = true
	};

	// Sticky Piston Base
	blockProperties[BlockType::BLOCK_PISTON_STICKY] = {
		.material = Material::Piston(),
		.hardness = 0.5f,
		.lightOpacity = 0,
		.stepSound = StepSound::Stone,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Cobweb
	blockProperties[BlockType::BLOCK_COBWEB] = {
		.material = Material::Web(),
		.hardness = 4.0f,
		.lightOpacity = 1,
		.stepSound = StepSound::Cloth,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Tall Grass
	blockProperties[BlockType::BLOCK_TALLGRASS] = {
		.material = Material::Plants(),
		.hardness = 0.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Grass,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Dead Bush
	blockProperties[BlockType::BLOCK_DEADBUSH] = {
		.material = Material::Plants(),
		.hardness = 0.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Grass,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Piston Base
	blockProperties[BlockType::BLOCK_PISTON] = {
		.material = Material::Piston(),
		.hardness = 0.5f,
		.lightOpacity = 0,
		.stepSound = StepSound::Stone,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Piston Extension (head)
	blockProperties[BlockType::BLOCK_PISTON_HEAD] = {
		.material = Material::Piston(),
		.hardness = 0.5f,
		.lightOpacity = 0,
		.stepSound = StepSound::Stone,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Wool (Cloth)
	blockProperties[BlockType::BLOCK_WOOL] = {
		.material = Material::Cloth(),
		.hardness = 0.8f,
		.lightOpacity = 255,
		.stepSound = StepSound::Cloth,
	};

	// Piston Moving (tile entity placeholder)
	blockProperties[BlockType::BLOCK_PISTON_MOVING] = {
		.material = Material::Piston(),
		.hardness = -1.0f,
		.lightOpacity = 0,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.enableStats = false,
	};

	// Dandelion (Yellow Flower)
	blockProperties[BlockType::BLOCK_DANDELION] = {
		.material = Material::Plants(),
		.hardness = 0.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Grass,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Rose (Red Flower)
	blockProperties[BlockType::BLOCK_ROSE] = {
		.material = Material::Plants(),
		.hardness = 0.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Grass,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Brown Mushroom
	blockProperties[BlockType::BLOCK_MUSHROOM_BROWN] = { .material = Material::Plants(),
		                                                 .hardness = 0.0f,
		                                                 .lightEmission = 1,
		                                                 .lightOpacity = 0,
		                                                 .stepSound = StepSound::Grass,
		                                                 .isCollidable = false,
		                                                 .isOpaqueCube = false,
		                                                 .isNormalCube = false,
		                                                 .ticksOnLoad = true };

	// Red Mushroom
	blockProperties[BlockType::BLOCK_MUSHROOM_RED] = { .material = Material::Plants(),
		                                               .hardness = 0.0f,
		                                               .lightOpacity = 0,
		                                               .stepSound = StepSound::Grass,
		                                               .isCollidable = false,
		                                               .isOpaqueCube = false,
		                                               .isNormalCube = false,
		                                               .ticksOnLoad = true };

	// Gold Block
	blockProperties[BlockType::BLOCK_GOLD] = {
		.material = Material::Iron(),
		.hardness = 3.0f,
		.resistance = 10.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Iron Block
	blockProperties[BlockType::BLOCK_IRON] = {
		.material = Material::Iron(),
		.hardness = 5.0f,
		.resistance = 10.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Double Stone Slab
	blockProperties[BlockType::BLOCK_DOUBLE_SLAB] = {
		.material = Material::Rock(),
		.hardness = 2.0f,
		.resistance = 10.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Stone Slab (single)
	blockProperties[BlockType::BLOCK_SLAB] = {
		.material = Material::Rock(),
		.hardness = 2.0f,
		.resistance = 10.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Bricks
	blockProperties[BlockType::BLOCK_BRICKS] = {
		.material = Material::Rock(),
		.hardness = 2.0f,
		.resistance = 10.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// TNT
	blockProperties[BlockType::BLOCK_TNT] = {
		.material = Material::TNT(),
		.hardness = 0.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Grass,
	};

	// Bookshelf
	blockProperties[BlockType::BLOCK_BOOKSHELF] = {
		.material = Material::Wood(),
		.hardness = 1.5f,
		.lightOpacity = 255,
		.stepSound = StepSound::Wood,
	};

	// Mossy Cobblestone
	blockProperties[BlockType::BLOCK_COBBLESTONE_MOSSY] = {
		.material = Material::Rock(),
		.hardness = 2.0f,
		.resistance = 10.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Obsidian
	blockProperties[BlockType::BLOCK_OBSIDIAN] = {
		.material = Material::Rock(),
		.hardness = 10.0f,
		.resistance = 2000.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Torch
	blockProperties[BlockType::BLOCK_TORCH] = {
		.material = Material::Circuits(),
		.hardness = 0.0f,
		.lightEmission = 14,
		.lightOpacity = 0,
		.stepSound = StepSound::Wood,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Fire
	blockProperties[BlockType::BLOCK_FIRE] = {
		.material = Material::Fire(),
		.hardness = 0.0f,
		.lightEmission = 15,
		.lightOpacity = 0,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.ticksOnLoad = true,
		.canBlockGrass = false,
		.enableStats = false,
	};

	// Monster Spawner
	blockProperties[BlockType::BLOCK_MOB_SPAWNER] = {
		.material = Material::Iron(),
		.hardness = 5.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Stone,
		.enableStats = false,
	};

	// Oak Wood Stairs
	blockProperties[BlockType::BLOCK_STAIRS_WOOD] = {
		.material = Material::Wood(),
		.hardness = 2.0f,
		.resistance = 5.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Wood,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Chest
	blockProperties[BlockType::BLOCK_CHEST] = {
		.material = Material::Wood(),
		.hardness = 2.5f,
		.lightOpacity = 255,
		.stepSound = StepSound::Wood,
	};

	// Redstone Wire
	blockProperties[BlockType::BLOCK_REDSTONE] = {
		.material = Material::Circuits(),
		.hardness = 0.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Stone,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.enableStats = false,
	};

	// Diamond Ore
	blockProperties[BlockType::BLOCK_ORE_DIAMOND] = {
		.material = Material::Rock(),
		.hardness = 3.0f,
		.resistance = 5.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Diamond Block
	blockProperties[BlockType::BLOCK_DIAMOND] = {
		.material = Material::Iron(),
		.hardness = 5.0f,
		.resistance = 10.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Crafting Table (Workbench)
	blockProperties[BlockType::BLOCK_CRAFTING_TABLE] = {
		.material = Material::Wood(),
		.hardness = 2.5f,
		.lightOpacity = 255,
		.stepSound = StepSound::Wood,
	};

	// Crops / Wheat
	blockProperties[BlockType::BLOCK_CROP_WHEAT] = {
		.material = Material::Plants(),
		.hardness = 0.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Grass,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.ticksOnLoad = true,
		.enableStats = false,
	};

	// Farmland (Tilled Field)
	blockProperties[BlockType::BLOCK_FARMLAND] = { .material = Material::Ground(),
		                                           .hardness = 0.6f,
		                                           .lightOpacity = 255,
		                                           .stepSound = StepSound::Gravel,
		                                           .isOpaqueCube = false,
		                                           .isNormalCube = false,
		                                           .ticksOnLoad = true };

	// Furnace (idle)
	blockProperties[BlockType::BLOCK_FURNACE] = {
		.material = Material::Rock(),
		.hardness = 3.5f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Furnace (active/lit)
	blockProperties[BlockType::BLOCK_FURNACE_LIT] = {
		.material = Material::Rock(),
		.hardness = 3.5f,
		.lightEmission = 13,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Sign (standing)
	blockProperties[BlockType::BLOCK_SIGN_STANDING] = {
		.material = Material::Wood(),
		.hardness = 1.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Wood,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.enableStats = false,
	};

	// Wooden Door
	blockProperties[BlockType::BLOCK_DOOR_WOOD] = {
		.material = Material::Wood(),
		.hardness = 3.0f,
		.resistance = 3.0f,
		.pushMobilityOverride = PushabilityFlag::NoPush,
		.lightOpacity = 0,
		.stepSound = StepSound::Wood,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.enableStats = false,
	};

	// Ladder
	blockProperties[BlockType::BLOCK_LADDER] = {
		.material = Material::Circuits(),
		.hardness = 0.4f,
		.lightOpacity = 0,
		.stepSound = StepSound::Wood,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Rail (normal)
	blockProperties[BlockType::BLOCK_RAIL] = {
		.material = Material::Circuits(),
		.hardness = 0.7f,
		.pushMobilityOverride = PushabilityFlag::Normal,
		.lightOpacity = 0,
		.stepSound = StepSound::Stone,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Cobblestone Stairs
	blockProperties[BlockType::BLOCK_STAIRS_COBBLESTONE] = {
		.material = Material::Rock(),
		.hardness = 2.0f,
		.resistance = 10.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Wall Sign
	blockProperties[BlockType::BLOCK_SIGN_WALL] = {
		.material = Material::Wood(),
		.hardness = 1.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Wood,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.enableStats = false,
	};

	// Lever
	blockProperties[BlockType::BLOCK_LEVER] = {
		.material = Material::Circuits(),
		.hardness = 0.5f,
		.lightOpacity = 0,
		.stepSound = StepSound::Wood,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Stone Pressure Plate
	blockProperties[BlockType::BLOCK_PRESSURE_PLATE_STONE] = { .material = Material::Rock(),
		                                                       .hardness = 0.5f,
		                                                       .pushMobilityOverride = PushabilityFlag::NoPush,
		                                                       .lightOpacity = 0,
		                                                       .stepSound = StepSound::Stone,
		                                                       .isCollidable = false,
		                                                       .isOpaqueCube = false,
		                                                       .isNormalCube = false,
		                                                       .ticksOnLoad = true };

	// Iron Door
	blockProperties[BlockType::BLOCK_DOOR_IRON] = {
		.material = Material::Iron(),
		.hardness = 5.0f,
		.pushMobilityOverride = PushabilityFlag::NoPush,
		.lightOpacity = 0,
		.stepSound = StepSound::Stone,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.enableStats = false,
	};

	// Wooden Pressure Plate
	blockProperties[BlockType::BLOCK_PRESSURE_PLATE_WOOD] = { .material = Material::Wood(),
		                                                      .hardness = 0.5f,
		                                                      .pushMobilityOverride = PushabilityFlag::NoPush,
		                                                      .lightOpacity = 0,
		                                                      .stepSound = StepSound::Wood,
		                                                      .isCollidable = false,
		                                                      .isOpaqueCube = false,
		                                                      .isNormalCube = false,
		                                                      .ticksOnLoad = true };

	// Redstone Ore
	blockProperties[BlockType::BLOCK_ORE_REDSTONE_OFF] = {
		.material = Material::Rock(),
		.hardness = 3.0f,
		.resistance = 5.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Redstone Ore (glowing/lit)
	blockProperties[BlockType::BLOCK_ORE_REDSTONE_ON] = {
		.material = Material::Rock(),
		.hardness = 3.0f,
		.resistance = 5.0f,
		.lightEmission = 9,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Redstone Torch (off)
	blockProperties[BlockType::BLOCK_REDSTONE_TORCH_OFF] = { .material = Material::Circuits(),
		                                                     .hardness = 0.0f,
		                                                     .lightOpacity = 0,
		                                                     .stepSound = StepSound::Wood,
		                                                     .isCollidable = false,
		                                                     .isOpaqueCube = false,
		                                                     .isNormalCube = false,
		                                                     .ticksOnLoad = true };

	// Redstone Torch (on)
	blockProperties[BlockType::BLOCK_REDSTONE_TORCH_ON] = { .material = Material::Circuits(),
		                                                    .hardness = 0.0f,
		                                                    .lightEmission = 7,
		                                                    .lightOpacity = 0,
		                                                    .stepSound = StepSound::Wood,
		                                                    .isCollidable = false,
		                                                    .isOpaqueCube = false,
		                                                    .isNormalCube = false,
		                                                    .ticksOnLoad = true };

	// Stone Button
	blockProperties[BlockType::BLOCK_BUTTON_STONE] = { .material = Material::Circuits(),
		                                               .hardness = 0.5f,
		                                               .lightOpacity = 0,
		                                               .stepSound = StepSound::Stone,
		                                               .isCollidable = false,
		                                               .isOpaqueCube = false,
		                                               .isNormalCube = false,
		                                               .ticksOnLoad = true };

	// Snow (layer)
	blockProperties[BlockType::BLOCK_SNOW_LAYER] = { .material = Material::SnowLayer(),
		                                             .hardness = 0.1f,
		                                             .lightOpacity = 0,
		                                             .stepSound = StepSound::Cloth,
		                                             .isOpaqueCube = false,
		                                             .isNormalCube = false,
		                                             .ticksOnLoad = true,
		                                             .canBlockGrass = false };

	// Ice
	blockProperties[BlockType::BLOCK_ICE] = { .material = Material::Ice(),
		                                      .hardness = 0.5f,
		                                      .slipperiness = 0.98f,
		                                      .lightOpacity = 3,
		                                      .stepSound = StepSound::Glass,
		                                      .ticksOnLoad = true };

	// Snow Block
	blockProperties[BlockType::BLOCK_SNOW] = {
		.material = Material::SnowBlock(),
		.hardness = 0.2f,
		.lightOpacity = 255,
		.stepSound = StepSound::Cloth,
	};

	// Cactus
	blockProperties[BlockType::BLOCK_CACTUS] = {
		.material = Material::Cactus(),
		.hardness = 0.4f,
		.lightOpacity = 0,
		.stepSound = StepSound::Cloth,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.ticksOnLoad = true,
	};

	// Clay Block
	blockProperties[BlockType::BLOCK_CLAY] = {
		.material = Material::Clay(),
		.hardness = 0.6f,
		.lightOpacity = 255,
		.stepSound = StepSound::Gravel,
	};

	// Sugar Cane (Reed)
	blockProperties[BlockType::BLOCK_SUGARCANE] = {
		.material = Material::Plants(),
		.hardness = 0.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Grass,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.ticksOnLoad = true,
		.enableStats = false,
	};

	// Jukebox
	blockProperties[BlockType::BLOCK_JUKEBOX] = {
		.material = Material::Wood(),
		.hardness = 2.0f,
		.resistance = 10.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Fence
	blockProperties[BlockType::BLOCK_FENCE] = {
		.material = Material::Wood(),
		.hardness = 2.0f,
		.resistance = 5.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Wood,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Pumpkin
	blockProperties[BlockType::BLOCK_PUMPKIN] = {
		.material = Material::Pumpkin(),
		.hardness = 1.0f,
		.lightOpacity = 255,
		.stepSound = StepSound::Wood,
	};

	// Netherrack
	blockProperties[BlockType::BLOCK_NETHERRACK] = {
		.material = Material::Rock(),
		.hardness = 0.4f,
		.lightOpacity = 255,
		.stepSound = StepSound::Stone,
	};

	// Soul Sand
	blockProperties[BlockType::BLOCK_SOULSAND] = {
		.material = Material::Sand(),
		.hardness = 0.5f,
		.lightOpacity = 255,
		.stepSound = StepSound::Sand,
	};

	// Glowstone
	blockProperties[BlockType::BLOCK_GLOWSTONE] = {
		.material = Material::Rock(),
		.hardness = 0.3f,
		.lightEmission = 15,
		.lightOpacity = 255,
		.stepSound = StepSound::Glass,
	};

	// Nether Portal
	blockProperties[BlockType::BLOCK_NETHER_PORTAL] = {
		.material = Material::Portal(),
		.hardness = -1.0f,
		.lightEmission = 11,
		.lightOpacity = 0,
		.stepSound = StepSound::Glass,
		.isCollidable = false,
		.isOpaqueCube = false,
		.isNormalCube = false,
	};

	// Jack-o-Lantern (Lit Pumpkin)
	blockProperties[BlockType::BLOCK_PUMPKIN_LIT] = {
		.material = Material::Pumpkin(),
		.hardness = 1.0f,
		.lightEmission = 15,
		.lightOpacity = 255,
		.stepSound = StepSound::Wood,
	};

	// Cake
	blockProperties[BlockType::BLOCK_CAKE] = {
		.material = Material::Cake(),
		.hardness = 0.5f,
		.lightOpacity = 0,
		.stepSound = StepSound::Cloth,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.enableStats = false,
	};

	// Redstone Repeater (off)
	blockProperties[BlockType::BLOCK_REDSTONE_REPEATER_OFF] = {
		.material = Material::Circuits(),
		.hardness = 0.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Wood,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.enableStats = false,
	};

	// Redstone Repeater (on)
	blockProperties[BlockType::BLOCK_REDSTONE_REPEATER_ON] = {
		.material = Material::Circuits(),
		.hardness = 0.0f,
		.lightEmission = 9,
		.lightOpacity = 0,
		.stepSound = StepSound::Wood,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.enableStats = false,
	};

	// Trapdoor
	blockProperties[BlockType::BLOCK_TRAPDOOR] = {
		.material = Material::Wood(),
		.hardness = 3.0f,
		.lightOpacity = 0,
		.stepSound = StepSound::Wood,
		.isOpaqueCube = false,
		.isNormalCube = false,
		.enableStats = false,
	};
}

} // namespace Blocks