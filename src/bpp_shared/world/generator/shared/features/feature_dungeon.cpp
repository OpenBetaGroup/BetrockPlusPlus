/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../feature_gen.h"
#include "world.h"
#include "tile_entities/tile_entity.h"

bool FeatureGenerator::GenerateDungeon(WorldWrapper& _world, Java::Random& _rand, Int3 _pos) {
	const int8_t dungeonHeight = 3;
	int32_t dungeonWidthX = _rand.NextInt(2) + 2;
	int32_t dungeonWidthZ = _rand.NextInt(2) + 2;
	int32_t validEntries = 0;

	for (int32_t xi = _pos.x - dungeonWidthX - 1; xi <= _pos.x + dungeonWidthX + 1; ++xi)
		for (int32_t yi = _pos.y - 1; yi <= _pos.y + dungeonHeight + 1; ++yi)
			for (int32_t zi = _pos.z - dungeonWidthZ - 1; zi <= _pos.z + dungeonWidthZ + 1; ++zi) {
				BlockType bt = _world.GetBlockId({ xi, yi, zi });
				if (yi == _pos.y - 1 && !IsSolid(bt))
					return false;
				if (yi == _pos.y + dungeonHeight + 1 && !IsSolid(bt))
					return false;
				bool isWall = (xi == _pos.x - dungeonWidthX - 1 || xi == _pos.x + dungeonWidthX + 1 ||
				               zi == _pos.z - dungeonWidthZ - 1 || zi == _pos.z + dungeonWidthZ + 1);
				if (isWall && yi == _pos.y && bt == BLOCK_AIR && _world.GetBlockId({ xi, yi + 1, zi }) == BLOCK_AIR)
					++validEntries;
			}

	if (validEntries < 1 || validEntries > 5)
		return false;

	for (int32_t xi = _pos.x - dungeonWidthX - 1; xi <= _pos.x + dungeonWidthX + 1; ++xi)
		for (int32_t yi = _pos.y + dungeonHeight; yi >= _pos.y - 1; --yi)
			for (int32_t zi = _pos.z - dungeonWidthZ - 1; zi <= _pos.z + dungeonWidthZ + 1; ++zi) {
				bool interior = (xi != _pos.x - dungeonWidthX - 1 && xi != _pos.x + dungeonWidthX + 1 &&
				                 yi != _pos.y - 1 && yi != _pos.y + dungeonHeight + 1 &&
				                 zi != _pos.z - dungeonWidthZ - 1 && zi != _pos.z + dungeonWidthZ + 1);
				if (interior) {
					_world.SetBlock({ xi, yi, zi }, BLOCK_AIR);
				} else if (yi >= 0 && !IsSolid(_world.GetBlockId({ xi, yi - 1, zi }))) {
					_world.SetBlock({ xi, yi, zi }, BLOCK_AIR);
				} else if (IsSolid(_world.GetBlockId({ xi, yi, zi }))) {
					BlockType wall = (yi == _pos.y - 1 && _rand.NextInt(4) != 0) ? BLOCK_COBBLESTONE_MOSSY
					                                                             : BLOCK_COBBLESTONE;
					_world.SetBlock({ xi, yi, zi }, wall);
				}
			}

	// Up to 2 chests, 3 placement attempts each
	for (int32_t chestAttempt = 0; chestAttempt < 2; ++chestAttempt) {
		for (int32_t attempt = 0; attempt < 3; ++attempt) {
			int32_t cx = _pos.x + _rand.NextInt(dungeonWidthX * 2 + 1) - dungeonWidthX;
			int32_t cz = _pos.z + _rand.NextInt(dungeonWidthZ * 2 + 1) - dungeonWidthZ;
			if (_world.GetBlockId({ cx, _pos.y, cz }) != BLOCK_AIR)
				continue;
			int32_t adj = 0;
			if (IsSolid(_world.GetBlockId({ cx - 1, _pos.y, cz })))
				++adj;
			if (IsSolid(_world.GetBlockId({ cx + 1, _pos.y, cz })))
				++adj;
			if (IsSolid(_world.GetBlockId({ cx, _pos.y, cz - 1 })))
				++adj;
			if (IsSolid(_world.GetBlockId({ cx, _pos.y, cz + 1 })))
				++adj;
			if (adj == 1) {
				_world.SetBlock({ cx, _pos.y, cz }, BLOCK_CHEST);
				auto chest = std::make_shared<TileEntityChest>(Int3{ cx, _pos.y, cz });
				for (int32_t slot = 0; slot < 8; ++slot) {
					auto stack = GenerateDungeonChestLoot(_rand);
					if (stack.id != Items::Id::INVALID) {
						int32_t slotIndex = _rand.NextInt(27);
						chest->inventory.SetInventorySlotContents(slotIndex, &stack);
					}
				}
				_world.manager.CreateTileEntity(std::move(chest));
				break;
			}
		}
	}

	_world.SetBlock(_pos, BLOCK_MOB_SPAWNER);
	auto spawner = std::make_shared<TileEntityMobSpawner>(_pos);
	spawner->entityId = PickMobToSpawn(_rand);
	_world.manager.CreateTileEntity(std::move(spawner));
	return true;
}

// Creates Dungeon Chest loot
ItemStack FeatureGenerator::GenerateDungeonChestLoot(Java::Random& _rand) {
	int32_t roll = _rand.NextInt(11);
	switch (roll) {
	case 0:
		return { .id = Items::Id::SADDLE, .count = 1 };
	case 1: {
		int8_t qty = _rand.NextInt(4) + 1;
		return { .id = Items::Id::IRON, .count = qty };
	}
	case 2:
		return { .id = Items::Id::BREAD, .count = 1 };
	case 3: {
		int8_t qty = _rand.NextInt(4) + 1;
		return { .id = Items::Id::WHEAT, .count = qty };
	}
	case 4: {
		int8_t qty = _rand.NextInt(4) + 1;
		return { .id = Items::Id::GUNPOWDER, .count = qty };
	}
	case 5: {
		int8_t qty = _rand.NextInt(4) + 1;
		return { .id = Items::Id::STRING, .count = qty };
	}
	case 6:
		return { .id = Items::Id::BUCKET, .count = 1 };
	case 7:
		if (_rand.NextInt(100) == 0)
			return { .id = Items::Id::APPLE_GOLDEN, .count = 1 };
		return { .id = Items::Id::INVALID };
	case 8:
		if (_rand.NextInt(2) == 0) {
			int8_t qty = _rand.NextInt(4) + 1;
			return { .id = Items::Id::REDSTONE, .count = qty };
		}
		return { .id = Items::Id::INVALID };
	case 9:
		if (_rand.NextInt(10) == 0) {
			int16_t discId = (_rand.NextInt(2) == 0) ? Items::Id::RECORD_13 : Items::Id::RECORD_CAT;
			return { .id = discId, .count = 1 };
		}
		return { .id = Items::Id::INVALID };
	case 10:
		return { .id = Items::Id::DYE, .count = 1, .data = 3 };
	default:
		return { .id = Items::Id::INVALID };
	}
}

std::string FeatureGenerator::PickMobToSpawn(Java::Random& _rand) {
	switch (_rand.NextInt(4)) {
	case 0:
		return "Skeleton";
	case 1:
	case 2:
		return "Zombie";
	case 3:
		return "Spider";
	default:
		return "Zombie";
	}
}