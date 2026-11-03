/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "tile_entity_manager.h"
#include "world/world.h"

void TileEntityManager::TickTileEntities(WorldManager& _world) {
	// Snapshot for iteration so Tick() can safely mutate tickableTileEntities
	std::vector<std::weak_ptr<TileEntity>> tickableTileEntitiesCopy = tickableTileEntities;
	for (const std::weak_ptr<TileEntity>& wp : tickableTileEntitiesCopy) {
		if (auto te = wp.lock()) {
			te->Tick(_world);
		}
	}
	std::erase_if(tickableTileEntities, [](const std::weak_ptr<TileEntity>& _wp) { return _wp.expired(); });
}

// Provided a tile entity nbt compound, will return a shared ptr of 
// The provided tile entity's id with the data deserialized
std::shared_ptr<TileEntity> TileEntityHelper::CreateTileEntityShared(const Tag& _nbt) {
	std::shared_ptr<TileEntity> returnedTE;

	const auto& id = _nbt.Get("id").GetString();
	int32_t tx = _nbt.Get("x").GetInt();
	int32_t ty = _nbt.Get("y").GetInt();
	int32_t tz = _nbt.Get("z").GetInt();
	Int3 pos{ tx, ty, tz };

	// Annoying!!
	if (id == "Chest") {
		returnedTE = std::make_shared<TileEntityChest>(pos);
	} else if (id == "Furnace") {
		returnedTE = std::make_shared<TileEntityFurnace>(pos);
	} else if (id == "Trap") {
		returnedTE = std::make_shared<TileEntityDispenser>(pos);
	} else if (id == "Sign") {
		returnedTE = std::make_shared<TileEntitySign>(pos);
	} else if (id == "Music") {
		returnedTE = std::make_shared<TileEntityNoteblock>(pos);
	} else if (id == "RecordPlayer") {
		returnedTE = std::make_shared<TileEntityJukebox>(pos);
	} else if (id == "MobSpawner") {
		returnedTE = std::make_shared<TileEntityMobSpawner>(pos);
	} else if (id == "Piston") {
		returnedTE = std::make_shared<TileEntityPistonMoving>(pos);
	} else {
		Logger::Warning << "Tried to create a tile entity with an invalid id!\n";
		return nullptr;
	}

	returnedTE->Deserialize(_nbt);

	return returnedTE;
}
